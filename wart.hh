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

// lib curl
#include <curl/curl.h>

// nlohmann json
#include <nlohmann/json.hpp>

#define WART_VERSION 0

// Concatenate using std::string instead of macros for better handling
#define SAFE_GETENV(var) (getenv(var) ? getenv(var) : "")
#define WART_HOME (std::string(SAFE_GETENV("HOME")) + "/.wart/")
#define WART_CONFIG (WART_HOME + "wartrc")

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

struct memoryStruct {
    char *memory;
    size_t size;
};

#endif // WART_HH
