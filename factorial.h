#ifndef FACTORIAL_H
#define FACTORIAL_H

#include "stack.h"
#include <vector>
#include <string>

struct FactorialStep {
    int n;
    unsigned long long value;
};

bool canMultiply(unsigned long long current, unsigned long long multiplier);
bool calculateForward(int n, Stack& stack, std::vector<FactorialStep>& steps, std::string& error);
std::vector<FactorialStep> calculateBackward(Stack& stack, int startingN);

#endif
