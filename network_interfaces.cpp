#include "network_interfaces.h"

#include <charconv>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#elif defined(__APPLE__)
#include <ifaddrs.h>
#include <net/if.h>
#include <SystemConfiguration/SystemConfiguration.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <ifaddrs.h>
#include <net/if.h>
#endif

namespace {

#if defined(__APPLE__)
bool isMacVirtualInterface(const std::string& name) {
    constexpr const char* excludedPrefixes[] = {
        //note: im filtering virtual interfaces, 
        //this may be reactivated in the future if needed
        "anpi", "bridge", "utun", "ap", "awdl", "llw"
    };
    for (const char* prefix : excludedPrefixes) {
        const std::string prefixString(prefix);
        if (name.compare(0, prefixString.size(), prefixString) == 0) {
            return true;
        }
    }
    return false;
}

bool isMacWifiInterface(const std::string& name) {
    CFArrayRef allInterfaces = SCNetworkInterfaceCopyAll();
    if (allInterfaces == nullptr) {
        return false;
    }

    bool isWifi = false;
    const CFIndex count = CFArrayGetCount(allInterfaces);
    for (CFIndex index = 0; index < count; ++index) {
        auto* interface = static_cast<SCNetworkInterfaceRef>(
            const_cast<void*>(CFArrayGetValueAtIndex(allInterfaces, index)));
        CFStringRef interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
        if (interfaceType == nullptr || !CFEqual(interfaceType, kSCNetworkInterfaceTypeIEEE80211)) {
            continue;
        }
        CFStringRef bsdName = SCNetworkInterfaceGetBSDName(interface);
        if (bsdName == nullptr) {
            continue;
        }
        char interfaceName[IFNAMSIZ] = {};
        if (CFStringGetCString(bsdName, interfaceName, sizeof(interfaceName), kCFStringEncodingUTF8)
            && name == interfaceName) {
            isWifi = true;
            break;
        }
    }

    CFRelease(allInterfaces);
    return isWifi;
}
#endif

#if defined(_WIN32)
std::string toUtf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}
#endif

bool enumerateNetworkInterfaces(std::vector<NetworkInterface>& interfaces) {
#if defined(_WIN32)
    ULONG bufferSize = 0;
    ULONG result = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, nullptr, &bufferSize);
    if (result != ERROR_BUFFER_OVERFLOW || bufferSize == 0) {
        return false;
    }

    std::vector<unsigned char> buffer(bufferSize);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    result = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, adapters, &bufferSize);
    if (result != NO_ERROR) {
        return false;
    }

    for (const IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        const std::string id = adapter->AdapterName == nullptr ? "" : adapter->AdapterName;
        std::string name = toUtf8(adapter->FriendlyName);
        if (name.empty()) {
            name = id;
        }
        if (adapter->IfType == IF_TYPE_IEEE80211) {
            name += " (Wi-Fi)";
        }
        if (!id.empty()) {
            interfaces.push_back({id, name});
        }
    }
    return true;
#elif defined(__APPLE__) || defined(__linux__)
    ifaddrs* addresses = nullptr;
    if (getifaddrs(&addresses) != 0) {
        return false;
    }

    for (const ifaddrs* address = addresses; address != nullptr; address = address->ifa_next) {
        if (address->ifa_name == nullptr || (address->ifa_flags & IFF_UP) == 0
            || (address->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        const std::string name = address->ifa_name;
    #if defined(__APPLE__)
        if (isMacVirtualInterface(name)) {
            continue;
        }
    #endif
        bool alreadyListed = false;
        for (const NetworkInterface& item : interfaces) {
            if (item.id == name) {
                alreadyListed = true;
                break;
            }
        }
        if (!alreadyListed) {
            std::string displayName = name;
#if defined(__APPLE__)
            if (isMacWifiInterface(name)) {
                displayName += " (Wi-Fi)";
            }
#elif defined(__linux__)
            const std::string wirelessPath = "/sys/class/net/" + name + "/wireless";
            if (access(wirelessPath.c_str(), F_OK) == 0) {
                displayName += " (Wi-Fi)";
            }
#endif
            interfaces.push_back({name, displayName});
        }
    }
    freeifaddrs(addresses);
    return true;
#else
    (void)interfaces;
    return false;
#endif
}

bool chooseInterface(const std::vector<NetworkInterface>& available, const std::string& label,
                     NetworkInterface& selected) {
    while (true) {
        std::cout << label << " interface number: ";
        std::string input;
        if (!std::getline(std::cin, input)) {
            return false;
        }

        std::size_t selection = 0;
        const char* begin = input.data();
        const char* end = begin + input.size();
        const auto parsed = std::from_chars(begin, end, selection);
        if (parsed.ec != std::errc{} || parsed.ptr != end || selection == 0 || selection > available.size()) {
            std::cout << "Choose a number from the listed interfaces.\n";
            continue;
        }

        selected = available[selection - 1];
        return true;
    }
}

}

bool readNetworkInterfaces(NetworkInterfaces& interfaces) {
    std::vector<NetworkInterface> available;
    if (!enumerateNetworkInterfaces(available)) {
        std::cerr << "Could not read network interfaces from this operating system.\n";
        return false;
    }
    if (available.size() < 2) {
        std::cerr << "At least two active, non-loopback network interfaces are required.\n";
        return false;
    }

    std::cout << "Available active network interfaces:\n";
    for (std::size_t index = 0; index < available.size(); ++index) {
        std::cout << "  " << index + 1 << ". " << available[index].name << '\n';
    }

    if (!chooseInterface(available, "Xbox-facing", interfaces.xbox)) {
        std::cerr << "Xbox-facing interface selection was cancelled.\n";
        return false;
    }

    while (true) {
        if (!chooseInterface(available, "Internet-facing", interfaces.internet)) {
            std::cerr << "Internet-facing interface selection was cancelled.\n";
            return false;
        }
        if (interfaces.internet.id != interfaces.xbox.id) {
            return true;
        }
        std::cout << "The Xbox-facing and internet-facing interfaces must be different.\n";
    }
}