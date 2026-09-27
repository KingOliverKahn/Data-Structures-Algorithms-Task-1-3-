#pragma once

#include <string>

int calculateFee(long long durationMinutes);
std::string normalizeRegistration(std::string registration);
bool validRegistration(const std::string& registration);
