#ifndef WART_HH
#define WART_HH

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <string> // To handle string concatenation properly
#include <unordered_map>
#include <vector>
#include <cstring>
#include <sstream>
#include <atomic>
#include <thread>
#include <string_view>


// linux
#include <signal.h>

// lib curl
#include <curl/curl.h>

// nlohmann json
#include <nlohmann/json.hpp>

#define WART_VERSION 11

// Concatenate using std::string instead of macros for better handling
#define SAFE_GETENV(var) (getenv(var) ? getenv(var) : "")
#define WART_HOME (std::string(SAFE_GETENV("HOME")) + "/.wart/")
#define WART_CONFIG (WART_HOME + "wartrc")
#define WART_LOCK (WART_HOME + "wart.lock")

#define PRINT_ERROR(msg)                                                       \
  std::cerr << "Error: " << msg << " at " << __FILE__ << " l: " << __LINE__    \
            << std::endl;

enum class wartCondition {
  HOME_EXISTS,
  CONFIG_EXISTS,
  HOME_DNE,
  CONFIG_DNE,
  FULL_FAIL,
  FULL_PASS
};

enum class wartError {
  NETWORK_ERROR,
  FS_ERROR,
  PERM_ERROR,
  CURL_ERROR,
};

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

struct memoryStruct {
    char *memory;
    size_t size;
};

#endif // WART_HH
