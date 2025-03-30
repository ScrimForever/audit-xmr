// log.cpp
#include "log.hpp"
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

static std::mutex log_mutex;

void log_message(const std::string& log_path, const std::string& message) {
    log_message(log_path, message, false);
}

void log_message(const std::string& log_path, const std::string& message, bool is_block_end) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

    std::lock_guard<std::mutex> lock(log_mutex);
    std::ofstream log_file(log_path, std::ios::app);
    if (log_file.is_open()) {
        log_file << "[" << timestamp.str() << "] " << message << std::endl;
        if (is_block_end) {
            log_file << "-----" << std::endl;
        }
    }
}
