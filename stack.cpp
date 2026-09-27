#include "stack.h"

void Stack::push(unsigned long long value) { items_.push_back(value); }

unsigned long long Stack::pop() {
    if (items_.empty()) throw std::underflow_error("Cannot pop from an empty stack.");
    auto value = items_.back();
    items_.pop_back();
    return value;
}

unsigned long long Stack::peek() const {
    if (items_.empty()) throw std::underflow_error("Cannot peek at an empty stack.");
    return items_.back();
}

bool Stack::isEmpty() const { return items_.empty(); }
std::size_t Stack::size() const { return items_.size(); }
