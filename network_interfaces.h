#pragma once

#include <string>

struct NetworkInterface {
    std::string id;
    std::string name;
};

struct NetworkInterfaces {
    NetworkInterface xbox;
    NetworkInterface internet;
};

bool readNetworkInterfaces(NetworkInterfaces& interfaces);