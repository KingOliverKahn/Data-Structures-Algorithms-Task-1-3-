#include "parking.h"
#include <algorithm>
#include <cctype>

int calculateFee(long long minutes) {
    if (minutes <= 30) return 0;
    if (minutes <= 120) return 50;
    if (minutes <= 240) return 100;
    if (minutes <= 360) return 300;
    return 500;
}

std::string normalizeRegistration(std::string registration) {
    registration.erase(
        std::remove_if(registration.begin(), registration.end(),
                       [](unsigned char c) { return std::isspace(c); }),
        registration.end()
    );

    std::transform(registration.begin(), registration.end(), registration.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    return registration;
}

bool validRegistration(const std::string& registration) {
    if (registration.empty() || registration.size() > 12) return false;

    for (unsigned char c : registration) {
        if (!std::isalnum(c)) return false;
    }

    return true;
}
