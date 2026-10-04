#include "party.h"

#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        cerr << "Usage: 2pac-link -h (host) | -j (join) [--DEBUG=1]\n";
        return 1;
    }

    string mode;
    bool debugMode = false;
    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        const string argument = argv[argumentIndex];
        if (argument == "--DEBUG=1") {
            debugMode = true;
        } else if (argument == "-h" || argument == "-j") {
            if (!mode.empty()) {
                cerr << "Specify only one mode: -h or -j.\n";
                return 1;
            }
            mode = argument;
        } else {
            cerr << "Unknown argument: " << argument << '\n'
                 << "Usage: 2pac-link -h (host) | -j (join) [--DEBUG=1]\n";
            return 1;
        }
    }
    if (mode.empty()) {
        cerr << "Specify a mode: -h (host) or -j (join).\n";
        return 1;
    }

    cout << "2pac-link\nConnect an Xbox network to an internet network.\n\n";

    NetworkInterfaces interfaces;
    if (!readNetworkInterfaces(interfaces)) {
        return 1;
    }

    return mode == "-h" ? runHost(interfaces, debugMode) : runClient(interfaces);
}
