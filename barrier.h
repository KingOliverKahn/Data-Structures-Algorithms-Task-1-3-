#pragma once

class Barrier {
public:
    void open();
    void close();
    bool isOpen() const;

private:
    bool open_{false};
};
