#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    bool initialise();
    sqlite3* handle() const;

private:
    sqlite3* db_{nullptr};
    std::string path_;
};
