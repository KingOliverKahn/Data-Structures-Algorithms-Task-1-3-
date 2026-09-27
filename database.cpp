#include "database.h"
#include <fstream>
#include <sstream>

Database::Database(const std::string& path) : path_(path) {
    sqlite3_open(path_.c_str(), &db_);
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

sqlite3* Database::handle() const {
    return db_;
}

bool Database::initialise() {
    if (!db_) return false;

    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    const char* sql = R"SQL(
CREATE TABLE IF NOT EXISTS vehicles (
    vehicle_id INTEGER PRIMARY KEY AUTOINCREMENT,
    registration_number TEXT NOT NULL UNIQUE,
    vehicle_type TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS parking_slots (
    slot_id INTEGER PRIMARY KEY AUTOINCREMENT,
    slot_number TEXT NOT NULL UNIQUE,
    slot_type TEXT NOT NULL DEFAULT 'STANDARD',
    status TEXT NOT NULL DEFAULT 'AVAILABLE'
        CHECK(status IN ('AVAILABLE','OCCUPIED'))
);
CREATE TABLE IF NOT EXISTS parking_sessions (
    session_id INTEGER PRIMARY KEY AUTOINCREMENT,
    vehicle_id INTEGER NOT NULL,
    slot_id INTEGER NOT NULL,
    entry_time TEXT NOT NULL,
    exit_time TEXT,
    duration_minutes INTEGER,
    amount_due INTEGER,
    session_status TEXT NOT NULL DEFAULT 'ACTIVE'
        CHECK(session_status IN ('ACTIVE','COMPLETED')),
    FOREIGN KEY(vehicle_id) REFERENCES vehicles(vehicle_id),
    FOREIGN KEY(slot_id) REFERENCES parking_slots(slot_id)
);
CREATE TABLE IF NOT EXISTS payments (
    payment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL UNIQUE,
    amount INTEGER NOT NULL,
    payment_time TEXT,
    payment_status TEXT NOT NULL DEFAULT 'PENDING',
    FOREIGN KEY(session_id) REFERENCES parking_sessions(session_id)
);
CREATE INDEX IF NOT EXISTS idx_vehicle_registration
ON vehicles(registration_number);
CREATE INDEX IF NOT EXISTS idx_active_sessions
ON parking_sessions(vehicle_id, session_status);
)SQL";

    char* error = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &error);
    if (error) sqlite3_free(error);
    if (rc != SQLITE_OK) return false;

    int count = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM parking_slots;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        sqlite3_exec(db_,
            "BEGIN; "
            "INSERT INTO parking_slots(slot_number) VALUES "
            "('P01'),('P02'),('P03'),('P04'),('P05'),('P06'),('P07'),('P08'),"
            "('P09'),('P10'),('P11'),('P12'); "
            "COMMIT;", nullptr, nullptr, nullptr);
    }

    return true;
}
