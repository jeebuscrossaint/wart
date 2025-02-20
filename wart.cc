#include "wart.hh"
#include <atomic>
#include <curl/curl.h>
#include <curl/easy.h>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <unordered_map>

using namespace std;

using json = nlohmann::json;

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

size_t writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memoryStruct *mem = (struct memoryStruct *)userp;

    char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        PRINT_ERROR("out of memory. wtf?");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;

}

bool fetchWallpaper(const std::unordered_map<std::string, std::string>& config) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        PRINT_ERROR("Failed to initialize CURL");
        return false;
    }

    // construct url with parameters
    std::string url = config.at("source") + "/?resolution=" + config.at("resolution") +
                     "&format=json&index=0&mkt=en-US";

    std::cout << "Fetching from URL: " << url << std::endl;

    struct memoryStruct chunk;
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 Wart/1.0");

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        PRINT_ERROR("Failed to fetch wallpaper data: " << curl_easy_strerror(res));
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return false;
    }

    // Debug: Print received data
    std::cout << "Received data: " << chunk.memory << std::endl;

    try {
        // Parse JSON response
        json response = json::parse(chunk.memory);

        // Extract image URL
        std::string imageUrl = response["url"];
        std::cout << "Image URL: " << imageUrl << std::endl;

        // Create a new CURL handle for downloading the image
        CURL* imgCurl = curl_easy_init();
        if (!imgCurl) {
            PRINT_ERROR("Failed to initialize CURL for image download");
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return false;
        }

        // Construct image filename
        std::string filename = wartHome + "wallpaper." + config.at("format");
        FILE* fp = fopen(filename.c_str(), "wb");
        if (!fp) {
            PRINT_ERROR("Failed to create image file");
            free(chunk.memory);
            curl_easy_cleanup(curl);
            curl_easy_cleanup(imgCurl);
            return false;
        }

        // Download image
        curl_easy_setopt(imgCurl, CURLOPT_URL, imageUrl.c_str());
        curl_easy_setopt(imgCurl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(imgCurl, CURLOPT_WRITEDATA, fp);

        res = curl_easy_perform(imgCurl);
        fclose(fp);

        if (res != CURLE_OK) {
            PRINT_ERROR("Failed to download image");
            free(chunk.memory);
            curl_easy_cleanup(curl);
            curl_easy_cleanup(imgCurl);
            return false;
        }

        curl_easy_cleanup(imgCurl);

    } catch (const json::exception& e) {
        PRINT_ERROR("JSON parsing failed: " << e.what());
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return true;
}

void cleanOldWallpapers(const std::string& format) { for (const auto& entry : fs::directory_iterator(wartHome)) { if (entry.path().extension() == "." + format) { fs::remove(entry.path()); } } }

bool createLockFile() {
    if (fs::exists(WART_LOCK)) {
        // check if the process is still running
        std::ifstream lock(WART_LOCK);
        pid_t pid;
        lock >> pid;

        if (kill(pid, 0) == 0) {
            PRINT_ERROR("Another instance is already running");
            return false;
        }

        // if its not running remove a stale lockfile
        fs::remove(WART_LOCK);
    }

    std::ofstream lock(WART_LOCK);
    lock << getpid();
    return true;
}

void removeLockFile() {
    if (fs::exists(WART_LOCK)) {
        fs::remove(WART_LOCK);
    }
}

void wartDestroy() {
  fs::remove(wartConfig);
  fs::remove_all(wartHome);
  removeLockFile();
}

std::atomic<bool> running{true};

void signalHandler(int signum [[maybe_unused]]) {
    running = false;
}

bool setWallpaper(const std::string& path) {
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if (!desktop) {
        PRINT_ERROR("Could not detect desktop environment");
        return false;
    }

    std::string cmd;
    std::string desktopStr(desktop);
    std::string absPath = fs::absolute(path).string();

    if (desktopStr == "GNOME" || desktopStr == "Unity") {
        cmd = "gsettings set org.gnome.desktop.background picture-uri 'file://" + absPath + "'";
    }
    else if (desktopStr == "KDE") {
        cmd = "qdbus org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript '"
              "var allDesktops = desktops();for (i=0;i<allDesktops.length;i++) {"
              "d = allDesktops[i];d.wallpaperPlugin = \"org.kde.image\";"
              "d.currentConfigGroup = [\"Wallpaper\", \"org.kde.image\", \"General\"];"
              "d.writeConfig(\"Image\", \"" + absPath + "\")}'";
    }
    else if (desktopStr == "XFCE") {
        cmd = "xfconf-query -c xfce4-desktop -p /backdrop/screen0/monitor0/workspace0/last-image -s " + absPath;
    }
    else if (desktopStr == "Hyprland") {
        cmd = "Downloaded succesfully.";
    }
    else {
        PRINT_ERROR("Unsupported desktop environment: " << desktopStr);
        return false;
    }

    std::cout << "Executing: " << cmd << std::endl;
    return system(cmd.c_str()) == 0;
}

void wartLoop(const std::unordered_map<std::string, std::string>& config) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    while (running) {
        if (config.at("clean") == "1") {
            cleanOldWallpapers(config.at("format"));
        }

        if (fetchWallpaper(config)) {
            std::string wallpaperPath = wartHome + "wallpaper." + config.at("format");
            if (setWallpaper(wallpaperPath)) {
                std::cout << "Successfully set wallpaper" << std::endl;
            } else {
                PRINT_ERROR("Failed to set wallpaper");
            }
        } else {
            PRINT_ERROR("Failed to fetch wallpaper");
        }

        std::cout << "Sleeping for " << config.at("interval") << " seconds..." << std::endl;
        for (int i = 0; i < std::stoi(config.at("interval")) && running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
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
            std::string newResolution = argv[2];
            // TODO: Update resolution in config file
        }
        else if (flag == "format" && argc > 2) {
            std::string newFormat = argv[2];
            // TODO: Update format in config file
        }
    }

    if (!wartExists()) {
        std::cerr << "Wart setup failed!" << std::endl;
        return 1;
    }

    // Create lock file
    if (!createLockFile()) {
        return 1;
    }

    std::unordered_map<std::string, std::string> config;
    if (!rcValidate(wartConfig, config)) {
        removeLockFile();
        return 1;
    }

    // Run the main loop
    try {
        wartLoop(config);
    } catch (const std::exception& e) {
        PRINT_ERROR("Exception in main loop: " << e.what());
        removeLockFile();
        return 1;
    }

    // Clean up
    removeLockFile();
    return 0;
}
