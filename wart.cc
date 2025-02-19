#include "wart.hh"

using namespace std;

namespace fs = std::filesystem;

const std::string wartHome = WART_HOME;
const std::string wartConfig = WART_CONFIG;

void printVersion() { std::cout << "Running on Wart v" << WART_VERSION << std::endl; }

// helper function to validate each part of rc file
bool validateInterval(const std::string& value) {
  try {
    int interval = std::stoi(value);
    return interval > 0;
  } catch (...) {
    return false;
  }
}

bool validateResolution(const std::string& value) {
    const std::vector<std::string> validResolutions = {
        "UHD", "1920x1200", "1920x1080", "1366x768", "1280x768",
        "1024x768", "800x600", "800x480", "768x1280", "720x1280",
        "640x480", "480x800", "400x240", "320x240", "240x320"
    };
    return std::find(validResolutions.begin(), validResolutions.end(), value) != validResolutions.end();
}

bool validateFormat(const std::string& value) {
    return value == "jpg" || value == "webp" || value == "png";
}

bool validateBoolean(const std::string& value) {
  return value == "0" || value == "1";
}

bool rcValidate(const std::string& filepath, std::unordered_map<std::string, std::string>& config) {
  std::ifstream file(filepath);
  if (!file) {
    PRINT_ERROR("");
    std::cerr << "Error: Cannot open config file: " << filepath << std::endl;
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string key, value;

    if (!(iss >> key >> value)) {
      PRINT_ERROR("")
      std::cerr << "Error: malformed line in config file: " << line << std::endl;

      return false;

    }

    config[key] = value;

  }

  if (config.find("interval") == config.end() || !validateInterval(config["interval"])) {
    std::cerr << "Error: 'interval' must be an integer > 0" << std::endl;
    return false;

  }

  if (config.find("source") == config.end() || config["source"].empty()) {
    std::cerr << "Error: 'source' must be specified." << std::endl;
    return false;

  }

  if (config.find("clean") == config.end() || !validateBoolean(config["clean"])) {
    std::cerr << "Error: 'clean' must be 0 or 1." << std::endl;
    return false;

  }

  if (config.find("resolution") == config.end() || !validateResolution(config["resolution"])) {
      std::cerr << "Error: 'resolution' must be a valid resolution" << std::endl;
      return false;
  }

  if (config.find("format") == config.end() || !validateFormat(config["format"])) {
      std::cerr << "Error: 'format' must be jpg, webp, or png" << std::endl;
      return false;
  }

  return true;
};

// Helper to check if wart's property exists
bool wartExists() {
  // Check if the directory exists
  if (!fs::exists(wartHome)) {
    // Directory does not exist, create it
    std::cout << "Creating directory: " << wartHome << std::endl;
    if (!fs::create_directory(wartHome)) {
      std::cerr << "Error: Failed to create directory " << wartHome
                << std::endl;
      return false;
    }
  }

  // Check if the file exists
  if (!fs::exists(wartConfig)) {
    // File does not exist, create it
    std::cout << "Creating file: " << wartConfig << std::endl;
    std::ofstream wartrc(wartConfig); // Create the file
    if (!wartrc) {
      std::cerr << "Error: Failed to create file " << wartConfig << std::endl;
      return false;
    }

    // Write default configuration
    wartrc << "interval 3600\n"
           << "source https://bing.biturl.top/\n"
           << "clean 1\n"
           << "resolution 1920x1200\n"
           << "format jpg\n";

    wartrc.close(); // Close the file after creating it
  }

  std::unordered_map<std::string, std::string> config;

  if (rcValidate(wartConfig, config)) {
    std::cout << "Config is valid!" << std::endl;
    std::cout << "Interval: " << config["interval"] << " seconds" << std::endl;
    std::cout << "Source: " << config["source"] << std::endl;
    std::cout << "Clean: " << (config["clean"] == "1" ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Resolution: " << (config["resolution"]) << std::endl;
    std::cout << "Format: " << (config["format"]) << std::endl;
  } else {
    std::cerr << "Invalid config. Exiting." << std::endl;
  }
  // If we reached here, everything exists or was created successfully
  std::cout << "Wart is healthy." << std::endl;
  return true;
}

void wartDestroy() {
  fs::remove(wartConfig);
  fs::remove_all(wartHome);
}



int main(int argc, char *argv[]) {
    printVersion();

    // Check if any arguments were provided
    if (argc > 1) {
        std::string flag = argv[1];
        if (flag == "destroy") {
            wartDestroy();
            return 0;
        }
        else if (flag == "resolution" && argc > 2) {
            // TODO: Update resolution in config file
            std::string newResolution = argv[2];
            // Add code to update config file with new resolution
        }
        else if (flag == "format" && argc > 2) {
            // TODO: Update format in config file
            std::string newFormat = argv[2];
            // Add code to update config file with new format
        }
    }

    // Default behavior when no arguments are provided
    if (!wartExists()) {
        std::cerr << "Wart setup failed!" << std::endl;
        return 1;
    }
    return 0;
}
