CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror

ifeq ($(OS),Windows_NT)
LDLIBS += -liphlpapi -lws2_32
endif

.PHONY: all clean

all: app

app: main.cpp party.cpp terminal.cpp network_interfaces.cpp guests.h party.h terminal.h network_interfaces.h
	$(CXX) $(CXXFLAGS) main.cpp party.cpp terminal.cpp network_interfaces.cpp -o app $(LDLIBS)

clean:
	rm -f app