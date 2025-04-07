#pragma once

// Standard Library
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

// System headers
#include <signal.h>
#include <unistd.h>

// External libraries
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace wart {

// Version information
constexpr const char *VERSION = "1.1.0";

// Path configurations
inline std::string getWartHome() {
  const char *home = std::getenv("HOME");
  return home ? std::string(home) + "/.wart/" : "";
}

inline const std::string WART_HOME = getWartHome();
inline const std::string WART_CONFIG = WART_HOME + "wartrc";
inline const std::string WART_LOCK = WART_HOME + "wart.lock";

// Error handling macro
#ifdef DEBUG
#define LOG_ERROR(msg)                                                         \
  std::cerr << "Error: " << msg << " at " << __FILE__ << ":" << __LINE__       \
            << std::endl
#else
#define LOG_ERROR(msg) std::cerr << "Error: " << msg << std::endl
#endif

// Logging levels
enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

// Configuration interface
struct Config {
  std::unordered_map<std::string, std::string> values;

  std::string get(const std::string &key,
                  const std::string &defaultValue = "") const {
    auto it = values.find(key);
    return (it != values.end()) ? it->second : defaultValue;
  }

  int getInt(const std::string &key, int defaultValue = 0) const {
    try {
      return std::stoi(get(key, std::to_string(defaultValue)));
    } catch (...) {
      return defaultValue;
    }
  }

  bool getBool(const std::string &key, bool defaultValue = false) const {
    std::string val = get(key, defaultValue ? "1" : "0");
    return val == "1" || val == "true" || val == "yes";
  }
};

// Memory buffer for curl operations
class MemoryBuffer {
public:
  MemoryBuffer() : memory(static_cast<char *>(malloc(1))), size(0) {
    if (memory)
      memory[0] = '\0';
  }

  ~MemoryBuffer() { free(memory); }

  // Prevent copying
  MemoryBuffer(const MemoryBuffer &) = delete;
  MemoryBuffer &operator=(const MemoryBuffer &) = delete;

  // Allow moving
  MemoryBuffer(MemoryBuffer &&other) noexcept
      : memory(other.memory), size(other.size) {
    other.memory = nullptr;
    other.size = 0;
  }

  MemoryBuffer &operator=(MemoryBuffer &&other) noexcept {
    if (this != &other) {
      free(memory);
      memory = other.memory;
      size = other.size;
      other.memory = nullptr;
      other.size = 0;
    }
    return *this;
  }

  // Append data to the buffer
  bool append(const void *data, size_t dataSize) {
    char *newMem = static_cast<char *>(realloc(memory, size + dataSize + 1));
    if (!newMem) {
      return false;
    }

    memory = newMem;
    memcpy(memory + size, data, dataSize);
    size += dataSize;
    memory[size] = '\0';
    return true;
  }

  const char *data() const { return memory; }
  size_t length() const { return size; }

private:
  char *memory;
  size_t size;
};

// Forward declarations of key functions
void logMessage(LogLevel level, const std::string &message);
bool loadConfig(const std::string &path, Config &config);
bool validateConfig(const Config &config);
bool fetchWallpaper(const Config &config);
bool setWallpaper(const std::string &path);
void executeHooks(const std::string &wallpaperPath);

} // namespace wart
