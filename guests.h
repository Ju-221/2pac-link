#pragma once

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

constexpr int firstGuestId = 2;
constexpr int maxGuestId = 16;

class Guests {
public:
    int guestID;
    std::string Nickname;
    bool connected = true;

    Guests(int id, std::string nickname) : guestID(id), Nickname(std::move(nickname)) {
        if (guestID < firstGuestId || guestID > maxGuestId) {
            throw std::invalid_argument("Guest ID must be between 2 and 16.");
        }
        if (Nickname.empty()) {
            Nickname = "guest";
        }
        if (Nickname.size() > 16) {
            Nickname.resize(16);
        }
    }

    void setConnected(bool isConnected) {
        connected = isConnected;
    }

    void setDisconnected() {
        connected = false;
    }

    void setNickname(const std::string& newNickname) {
        if (newNickname.empty()) {
            Nickname = "guest";
        } else if (newNickname.size() > 16) {
            std::cout << "Nickname is too long. It will be shortened to 16 characters.\n";
            Nickname = newNickname.substr(0, 16);
        } else {
            Nickname = newNickname;
        }
    }
};