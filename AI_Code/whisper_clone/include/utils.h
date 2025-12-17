#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <chrono>

namespace Utils {
    // File operations
    bool fileExists(const std::string& filepath);
    std::string getFileExtension(const std::string& filepath);
    size_t getFileSize(const std::string& filepath);

    // String operations
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string toLowerCase(const std::string& str);

    // Time utilities
    class Timer {
    public:
        Timer();
        void start();
        void stop();
        double elapsed() const;

    private:
        std::chrono::high_resolution_clock::time_point start_time_;
        std::chrono::high_resolution_clock::time_point end_time_;
        bool running_;
    };

    // Logging
    void log(const std::string& message);
    void logError(const std::string& message);
    void logInfo(const std::string& message);
}

#endif // UTILS_H
