#include "factorial.h"
#include <cassert>
#include <iostream>

int main() {
    Stack s;
    assert(s.isEmpty());
    s.push(10); s.push(20); s.push(30);
    assert(s.size() == 3);
    assert(s.peek() == 30);
    assert(s.pop() == 30);
    assert(s.pop() == 20);
    assert(s.pop() == 10);
    assert(s.isEmpty());

    Stack factorialStack;
    std::vector<FactorialStep> forward;
    std::string error;
    assert(calculateForward(5, factorialStack, forward, error));
    assert(error.empty());
    assert(forward.size() == 5);
    assert(forward[0].value == 1);
    assert(forward[4].value == 120);

    auto backward = calculateBackward(factorialStack, 5);
    assert(backward.size() == 5);
    assert(backward[0].n == 5 && backward[0].value == 120);
    assert(backward[1].n == 4 && backward[1].value == 24);
    assert(backward[4].n == 1 && backward[4].value == 1);

    Stack zeroStack;
    std::vector<FactorialStep> zero;
    assert(calculateForward(0, zeroStack, zero, error));
    assert(zero.size() == 1 && zero[0].value == 1);
    auto zeroBack = calculateBackward(zeroStack, 0);
    assert(zeroBack.size() == 1 && zeroBack[0].n == 0 && zeroBack[0].value == 1);

    Stack overflowStack;
    std::vector<FactorialStep> overflow;
    assert(!calculateForward(21, overflowStack, overflow, error));
    assert(!error.empty());

    std::cout << "All Task 3 tests passed.\n";
    return 0;
}
