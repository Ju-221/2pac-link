#include "party.h"

#include "guests.h"
#include "terminal.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {

string generateRoomCode() {
    random_device randomDevice;
    mt19937 generator(randomDevice());
    uniform_int_distribution<int> distribution(0, 999999);

    // IP from host -- generated 

    // translate with a hash - [a]192.168.0 or [b]10.0.0. or [c]172.16.0 (internal networks)
    // code A,B,C -> A -> FF -> 
    // slash subnet mask 16 <-> 24
    //

    ostringstream code;
    code << setw(6) << setfill('0') << distribution(generator);
    return code.str();
}

bool isValidRoomCode(const string& code) {
    return code.size() == 6 && all_of(code.begin(), code.end(), [](unsigned char digit) {
        return isdigit(digit) != 0;
    });
}

void printPartyStatus(const vector<Guests>& guests, const string& hostNickname) {
    cout << "\nParty status: " << (guests.empty() ? "Waiting (" : "Ready (")
         << guests.size() + 1 << "/4)\n"
         << "  [1] " << (hostNickname.empty() ? "Host" : hostNickname)
         << " (host): Ready\n";
    for (const Guests& guest : guests) {
        cout << "[" << guest.guestID << "] " << guest.Nickname
             << " (local preview): Ready\n";
    }
}

int findAvailableGuestId(const vector<Guests>& guests) {
    for (int guestId = firstGuestId; guestId <= maxGuestId; ++guestId) {
        const auto existingGuest = find_if(guests.begin(), guests.end(), [guestId](const Guests& guest) {
            return guest.guestID == guestId;
        });
        if (existingGuest == guests.end()) {
            return guestId;
        }
    }
    return 0;
}

void compactGuestIds(vector<Guests>& guests) {
    int guestId = firstGuestId;
    for (Guests& guest : guests) {
        guest.guestID = guestId++;
    }
}

void printGamePlayerStatus(const vector<Guests>& guests, const string& hostNickname, bool hostConnected) {
    cout << "\nPlayer status:\n"
         << "[1] " << (hostNickname.empty() ? "Host" : hostNickname)
         << " (host): " << (hostConnected ? "connected" : "disconnected") << '\n';
    for (const Guests& guest : guests) {
        cout << "[" << guest.guestID << "] " << guest.Nickname << ": "
            << (guest.connected ? "connected" : "disconnected") << '\n';
    }
}

}

int runHost(const NetworkInterfaces& interfaces, bool debugMode) {
    clearScreen();
    const string roomCode = generateRoomCode();
    cout << "\nRoom code: " << roomCode << '\n'
         << "Wait room preview. Networking is not connected yet.\n"
         << "Party capacity: 4 consoles total (host plus up to 3 guests).\n";
    vector<Guests> guests;
    const string hostNickname;
    printPartyStatus(guests, hostNickname);

    string input;
    while (true) {
        if (debugMode) {
            cout << "Enter to simulate join, k <id> to kick, d <id> to disconnect, s to start, q to quit: ";
        } else {
            cout << "Enter k <id> to kick, s to start, or q to quit: ";
        }
        if (!getline(cin, input)) {
            return 1;
        }
        if (input == "q" || input == "Q") {
            return 0;
        }
        if (input.empty()) {
            if (!debugMode) {
                cout << "Unknown command. Use 'k <id>', 's' to start, or 'q' to quit.\n";
                continue;
            }
            if (guests.size() == 3) {
                cout << "Room is full (4/4 consoles).\n";
                continue;
            }
            const int availableGuestId = findAvailableGuestId(guests);
            if (availableGuestId == 0) {
                cout << "Guest ID limit reached (2-16).\n";
                continue;
            }
            guests.emplace_back(availableGuestId, "");
            printPartyStatus(guests, hostNickname);
            continue;
        }
        if (input.rfind("k ", 0) == 0) {
            int guestId = 0;
            const char* numberStart = input.data() + 2;
            const char* numberEnd = input.data() + input.size();
            const auto parsed = std::from_chars(numberStart, numberEnd, guestId);
            if (parsed.ec != errc{} || parsed.ptr != numberEnd) {
                cout << "Use 'k' followed by a guest ID, for example: 'k 2'\n";
                continue;
            }

            if (guestId == 1) {
                cout << "You cannot kick yourself.\n";
                continue;
            }

            const auto guest = find_if(guests.begin(), guests.end(), [guestId](const Guests& member) {
                return member.guestID == guestId;
            });
            if (guest == guests.end()) {
                cout << "Guest " << guestId << " is not in this room.\n";
                continue;
            }

            cout << "Guest " << guestId << " (" << guest->Nickname
                 << ") was kicked from the room.\n";
            guests.erase(guest);
            compactGuestIds(guests);
            printPartyStatus(guests, hostNickname);
            continue;
        }
        if (debugMode && input.rfind("d ", 0) == 0) {
            int userId = 0;
            const char* numberStart = input.data() + 2;
            const char* numberEnd = input.data() + input.size();
            const auto parsed = std::from_chars(numberStart, numberEnd, userId);
            if (parsed.ec != errc{} || parsed.ptr != numberEnd) {
                cout << "Use 'd' followed by a user ID, for example: 'd 2'\n";
                continue;
            }

            if (userId == 1) {
                cout << "You (host) disconnected. Closing room.\n";
                return 0;
            }

            const auto guest = find_if(guests.begin(), guests.end(), [userId](const Guests& member) {
                return member.guestID == userId;
            });
            if (guest == guests.end()) {
                cout << "User " << userId << " is not connected to this room.\n";
                continue;
            }

            cout << "Guest " << userId << " (" << guest->Nickname << ") disconnected.\n";
            guests.erase(guest);
            compactGuestIds(guests);
            printPartyStatus(guests, hostNickname);
            continue;
        }
        if (input == "s" || input == "S") {
            break;
        }
        cout << "Unknown command.\n";
    }

    const NetworkInterfaces lockedSettings = interfaces;
    cout << "\nGame started. Settings are now locked for this session.\n"
         << "  Xbox interface: " << lockedSettings.xbox.name << '\n'
         << "  Internet interface: " << lockedSettings.internet.name << '\n'
         << '\n';

    bool hostConnected = true;
    printGamePlayerStatus(guests, hostNickname, hostConnected);

    while (true) {
        if (debugMode) {
            cout << "Game continues, enter ID to simulate disconnect (d <id>)";
        } else {
            cout << "Game session active";
        }
        cout << ", or q to end the session: ";
        if (!getline(cin, input)) {
            cout << "\nSession ended.\n";
            return 0;
        }
        if (input == "q" || input == "Q") {
            cout << "Session ended.\n";
            return 0;
        }
        if (input.empty()) {
            printGamePlayerStatus(guests, hostNickname, hostConnected);
            if (!hostConnected) {
                cout << "Player [1] disconnected.\n";
            }
            for (const Guests& guest : guests) {
                if (!guest.connected) {
                    cout << "Player [" << guest.guestID << "] disconnected.\n";
                }
            }
            continue;
        }
        if (debugMode && input.rfind("d ", 0) == 0) {
            int userId = 0;
            const char* numberStart = input.data() + 2;
            const char* numberEnd = input.data() + input.size();
            const auto parsed = std::from_chars(numberStart, numberEnd, userId);
            if (parsed.ec != errc{} || parsed.ptr != numberEnd) {
                cout << "Use 'd' followed by a user ID, for example: 'd 2'\n";
                continue;
            }

            string nickname;
            if (userId == 1) {
                if (!hostConnected) {
                    cout << "User 1 is not in this session.\n";
                    continue;
                }
                nickname = hostNickname.empty() ? "Host" : hostNickname;
                hostConnected = false;
                cout << "User " << userId << " " << nickname << " disconnected.\n";
                printGamePlayerStatus(guests, hostNickname, hostConnected);
                cout << "Session ended because the host disconnected.\n";
                return 0;
            }

            const auto guest = find_if(guests.begin(), guests.end(), [userId](const Guests& member) {
                return member.guestID == userId;
            });
            if (guest == guests.end()) {
                cout << "User " << userId << " is not in this session.\n";
                continue;
            }
            guest->connected = false;
            cout << "Press Enter to view player status, or q to end the session.\n";
            continue;
        }
        cout << (debugMode ? "Unknown command. Use d <id> or q.\n" : "Unknown command. Use q to quit.\n");
    }
}

int runClient(const NetworkInterfaces& interfaces) {
    clearScreen();
    cout << "Room code: ";
    string roomCode;
    if (!getline(cin, roomCode) || !isValidRoomCode(roomCode)) {
        cerr << "Enter a six-digit room code.\n";
        return 1;
    }

    cout << "\nWait room preview for code " << roomCode << ".\n"
         << "  Host: Not connected (network transport not implemented)\n"
         << "  You: Ready locally; not signed into a remote party\n"
         << "  Xbox interface: " << interfaces.xbox.name << '\n'
         << "  Internet interface: " << interfaces.internet.name << '\n'
         << "The room code format is valid, but no remote room lookup occurred.\n";
    return 0;
}
