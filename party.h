#pragma once

#include "network_interfaces.h"

int runHost(const NetworkInterfaces& interfaces, bool debugMode);
int runClient(const NetworkInterfaces& interfaces);
