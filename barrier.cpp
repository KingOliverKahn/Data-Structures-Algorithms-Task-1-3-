#include "barrier.h"

void Barrier::open() { open_ = true; }
void Barrier::close() { open_ = false; }
bool Barrier::isOpen() const { return open_; }
