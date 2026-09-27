#ifndef STACK_H
#define STACK_H

#include <vector>
#include <stdexcept>

class Stack {
private:
    std::vector<unsigned long long> items_;
public:
    void push(unsigned long long value);
    unsigned long long pop();
    unsigned long long peek() const;
    bool isEmpty() const;
    std::size_t size() const;
};

#endif
