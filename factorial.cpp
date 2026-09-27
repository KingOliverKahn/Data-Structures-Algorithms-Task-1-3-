#include "factorial.h"
#include <limits>

bool canMultiply(unsigned long long current, unsigned long long multiplier) {
    if (multiplier == 0) return true;
    return current <= std::numeric_limits<unsigned long long>::max() / multiplier;
}

bool calculateForward(int n, Stack& stack, std::vector<FactorialStep>& steps, std::string& error) {
    stack = Stack();
    steps.clear();
    error.clear();
    if (n < 0) { error = "Factorial is only defined here for non-negative integers."; return false; }

    unsigned long long factorial = 1;
    if (n == 0) {
        stack.push(1);
        steps.push_back({0, 1});
        return true;
    }

    for (int i = 1; i <= n; ++i) {
        if (!canMultiply(factorial, static_cast<unsigned long long>(i))) {
            error = "Overflow: the factorial is too large for unsigned long long.";
            return false;
        }
        factorial *= static_cast<unsigned long long>(i);
        stack.push(factorial);
        steps.push_back({i, factorial});
    }
    return true;
}

std::vector<FactorialStep> calculateBackward(Stack& stack, int startingN) {
    std::vector<FactorialStep> result;
    int current = startingN;
    while (!stack.isEmpty()) {
        result.push_back({current, stack.pop()});
        --current;
    }
    return result;
}
