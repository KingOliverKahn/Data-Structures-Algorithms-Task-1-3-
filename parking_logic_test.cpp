#include "parking.h"
#include <cassert>
#include <iostream>

int main() {
    assert(calculateFee(0) == 0);
    assert(calculateFee(30) == 0);
    assert(calculateFee(31) == 50);
    assert(calculateFee(120) == 50);
    assert(calculateFee(121) == 100);
    assert(calculateFee(240) == 100);
    assert(calculateFee(241) == 300);
    assert(calculateFee(360) == 300);
    assert(calculateFee(361) == 500);

    assert(normalizeRegistration(" kda 123a ") == "KDA123A");
    assert(validRegistration("KDA123A"));
    assert(!validRegistration("KDA-123A"));
    assert(!validRegistration(""));

    std::cout << "Parking logic tests passed.\n";
    return 0;
}
