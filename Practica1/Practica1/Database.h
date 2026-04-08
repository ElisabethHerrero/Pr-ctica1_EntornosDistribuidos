#pragma once
#include <windows.h>
#include <fstream>

class Database {
public:
    static Database& getInstance() {
        static Database instance;
        return instance;
    }

    void initialize() {
        try {
            logFile_.open("roomba_log.txt", std::ios::app);
            if (logFile_.is_open()) {
                logFile_ << "\n=== Nueva sesion ===\n";
                logFile_.flush();
            }
        }
        catch (...) {}
    }

    void logEvent(const wchar_t* event) {
        try {
            if (logFile_.is_open() && event) {
                char buffer[512] = { 0 };
                WideCharToMultiByte(CP_UTF8, 0, event, -1, buffer, sizeof(buffer) - 1, NULL, NULL);
                logFile_ << buffer << "\n";
                logFile_.flush();
            }
        }
        catch (...) {}
    }

    ~Database() {
        try {
            if (logFile_.is_open()) logFile_.close();
        }
        catch (...) {}
    }

private:
    Database() = default;
    std::ofstream logFile_;
};