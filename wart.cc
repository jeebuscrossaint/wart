#include "wart.hh"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <thread>
#include <unordered_map>

using namespace std;

using json = nlohmann::json;

namespace fs = std::filesystem;

const std::string wartHome = WART_HOME;
const std::string wartConfig = WART_CONFIG;

void printVersion() {
    // Wart version
    std::cout << "Running on Wart: " << WART_VERSION << std::endl;

    // libcurl version
    curl_version_info_data* curl_info = curl_version_info(CURLVERSION_NOW);
    std::cout << "libcurl version: " << curl_info->version << std::endl;

    // nlohmann json version
    std::cout << "nlohmann json version: "
              << NLOHMANN_JSON_VERSION_MAJOR << "."
              << NLOHMANN_JSON_VERSION_MINOR << "."
              << NLOHMANN_JSON_VERSION_PATCH << std::endl;
}

void logMessage(LogLevel level, const std::string& message) {
    static const std::unordered_map<LogLevel, std::string> levelStrings = {
        {LogLevel::DEBUG, "DEBUG"},
        {LogLevel::INFO, "INFO"},
        {LogLevel::WARNING, "WARNING"},
        {LogLevel::ERROR, "ERROR"}
    };

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::cout << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << " [" << levelStrings.at(level) << "] " << message << std::endl;
}

// helper function to validate each part of rc file
bool validateInterval(const std::string& value) {
  try {
    int interval = std::stoi(value);
    return interval > 0;
  } catch (...) {
    return false;
  }
}

bool validateHooks(const std::string& line) {
    return line.starts_with("x11hooks ") ||
           line.starts_with("waylandhooks ") ||
           line.starts_with("hooks ");  // for DE-agnostic hooks
}

bool validateApplier(const std::string& line) {
    return line.starts_with("x11applier ") ||
           line.starts_with("waylandapplier ") ||
           line.starts_with("applier ");  // for DE-agnostic applier
}

bool validateResolution(const std::string& value) {
    const std::vector<std::string> validResolutions = {
        "UHD", "1920x1200", "1920x1080", "1366x768", "1280x768",
        "1024x768", "800x600", "800x480", "768x1280", "720x1280",
        "640x480", "480x800", "400x240", "320x240", "240x320"
    };
    return std::find(validResolutions.begin(), validResolutions.end(), value) != validResolutions.end();
}

bool validateFormat(std::string_view value) {
    return value == "jpg" || value == "webp" || value == "png";
}

bool validateBoolean(const std::string& value) {
  return value == "0" || value == "1";
}

bool validatePreviewer(const std::string& line) {
    return line.starts_with("x11previewer ") ||
           line.starts_with("waylandpreviewer ") ||
           line.starts_with("previewer ");
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
           << "clean 1\n"
           << "resolution 1920x1200\n"
           << "format jpg\n"
           << "# Hook examples:\n"
           << "# x11hooks wal -i $WARTPAPER\n"
           << "# waylandhooks swww img $WARTPAPER\n"
           << "# hooks notify-send \"New wallpaper set\"\n"
           << "# Applier examples:\n"
           << "# x11applier feh --bg-fill $WARTPAPER\n"
           << "# waylandapplier swww img $WARTPAPER\n"
           << "# applier custom-wallpaper-script $WARTPAPER\n"
           << "# Previewer examples:\n"
           << "# x11previewer eog $WARTPAPER\n"
           << "# waylandpreviewer imv $WARTPAPER\n"
           << "# previewer xdg-open $WARTPAPER\n";


    wartrc.close(); // Close the file after creating it
  }

  std::unordered_map<std::string, std::string> config;

  if (rcValidate(wartConfig, config)) {
    std::cout << "Config is valid!" << std::endl;
    std::cout << "Interval: " << config["interval"] << " seconds" << std::endl;
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
    std::string url = "https://bing.biturl.top/?resolution=" + config.at("resolution") +
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

void cleanOldWallpapers(const std::string& format) {
    const std::string ext = "." + format;
    for (const auto& entry : fs::directory_iterator(wartHome)) {
        if (entry.path().extension() == ext) {
            fs::remove(entry.path());
        }
    }
}

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
  std::cout << "Wart has been destroyed." << std::endl;
}

std::atomic<bool> running{true};

void signalHandler(int signum [[maybe_unused]]) {
    running = false;
}

bool setWallpaper(const std::string& path) {
    const char* sessionType = getenv("XDG_SESSION_TYPE");
    if (!sessionType) {
        PRINT_ERROR("Could not detect session type");
        return false;
    }

    std::string session(sessionType);
    std::ifstream file(wartConfig);
    std::string line;
    std::string applierCmd;

    // Find appropriate applier
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        if ((session == "x11" && line.starts_with("x11applier ")) ||
            (session == "wayland" && line.starts_with("waylandapplier ")) ||
            line.starts_with("applier ")) {

            applierCmd = line.substr(line.find(' ') + 1);
            break;
        }
    }

    if (applierCmd.empty()) {
        // Fallback to default appliers if none specified
        if (session == "wayland") {
            applierCmd = "swww img";
        } else if (session == "x11") {
            applierCmd = "feh --bg-fill";
        } else {
            PRINT_ERROR("No applier configured and no fallback available");
            return false;
        }
    }

    // Replace $WARTPAPER with actual path
    std::string absPath = fs::absolute(path).string();
    size_t pos = applierCmd.find("$WARTPAPER");
    while (pos != std::string::npos) {
        applierCmd.replace(pos, 10, absPath);
        pos = applierCmd.find("$WARTPAPER");
    }

    if (applierCmd.find("$WARTPAPER") == std::string::npos) {
        applierCmd += " " + absPath;  // Append path if $WARTPAPER not in command
    }

    logMessage(LogLevel::INFO, "Setting wallpaper with: " + applierCmd);
    return system(applierCmd.c_str()) == 0;
}

void updateResolution(const std::string& newResolution) {
    if (!validateResolution(newResolution)) {
        PRINT_ERROR("Invalid resolution: " << newResolution);
        return;
    }

    // Read current config
    std::vector<std::string> lines;
    lines.reserve(10);
    std::ifstream file(wartConfig);
    std::string line;

    while (std::getline(file, line)) {
        if (line.substr(0, 10) == "resolution") {
            lines.emplace_back("resolution " + newResolution);
        } else {
            lines.emplace_back(line);
        }
    }

    // Write updated config
    std::ofstream outFile(wartConfig);
    for (const auto& l : lines) {
        outFile << l << "\n";
    }
}

void updateInterval(const std::string& newInterval) {
    if (!validateInterval(newInterval)) {
        PRINT_ERROR("Invalid interval: " << newInterval);
        return;
    }

    std::vector<std::string> lines;
    std::ifstream file(wartConfig);
    std::string line;

    while (std::getline(file, line)) {
        if (line.substr(0, 8) == "interval") {
            lines.push_back("interval " + newInterval);
        } else {
            lines.push_back(line);
        }
    }

    std::ofstream outFile(wartConfig);
    for (const auto& l : lines) {
        outFile << l << "\n";
    }
}

void showStatus() {
    if (!fs::exists(wartHome + "wallpaper.*")) {
        std::cout << "No wallpaper has been downloaded yet." << std::endl;
        return;
    }

    std::unordered_map<std::string, std::string> config;
    if (rcValidate(wartConfig, config)) {
        std::cout << "Current configuration:" << std::endl;
        std::cout << "Resolution: " << config["resolution"] << std::endl;
        std::cout << "Format: " << config["format"] << std::endl;
        std::cout << "Interval: " << config["interval"] << " seconds" << std::endl;
        std::cout << "Clean mode: " << (config["clean"] == "1" ? "Enabled" : "Disabled") << std::endl;

        // Show wallpaper file info
        auto wallpaperPath = wartHome + "wallpaper." + config["format"];
        std::cout << "Current wallpaper: " << wallpaperPath << std::endl;
        std::cout << "Size: " << fs::file_size(wallpaperPath) << " bytes" << std::endl;
        auto ftime = fs::last_write_time(wallpaperPath);
        auto sctp = std::chrono::file_clock::to_sys(ftime);
        auto time = std::chrono::system_clock::to_time_t(sctp);
        std::cout << "Last updated: " << std::ctime(&time);
    }
}

void showHelp() {
    std::cout << "Wart - Wallpaper Art\n"
              << "Usage: wart [command] [options]\n\n"
              << "Commands:\n"
              << "  resolution <res>   Set wallpaper resolution (e.g., 1920x1080, UHD)\n"
              << "  init              Initialize wart configuration\n"
              << "  format <fmt>       Set image format (jpg, webp, png)\n"
              << "  interval <sec>     Set update interval in seconds\n"
              << "  status            Show current configuration and wallpaper status\n"
              << "  preview           Download and preview next wallpaper\n"
              << "  destroy           Remove all wart files and configurations\n"
              << "  -d, daemon        Run in daemon mode\n"
              << "  -h, --help        Show this help message\n\n"
              << "  restore" << "         Restore previous wallpaper\n\n"
              << "Example:\n"
              << "  wart resolution UHD\n"
              << "  wart format webp\n"
              << "  wart preview\n"
              << "  wart -d\n";
}

void updateFormat(const std::string& newFormat) {
    if (!validateFormat(newFormat)) {
        PRINT_ERROR("Invalid format: " << newFormat);
        return;
    }

    std::vector<std::string> lines;
    std::ifstream file(wartConfig);
    std::string line;

    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "format") {
            lines.push_back("format " + newFormat);
        } else {
            lines.push_back(line);
        }
    }

    std::ofstream outFile(wartConfig);
    for (const auto& l : lines) {
        outFile << l << "\n";
    }
}

bool daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        PRINT_ERROR("Failed to fork");
        return false;
    }
    if (pid > 0) {
        exit(0); // parent exists
    }

    // child process continues
    setsid(); // create a new session

    // redirect std file descriptors
    std::freopen("/dev/null", "r", stdin);
    std::freopen("/dev/null", "w", stdout);
    std::freopen("/dev/null", "w", stderr);

    return true;
}

void executeHooks(const std::string& wallpaperPath) {
    std::ifstream file(wartConfig);
    std::string line;
    const char* sessionType = getenv("XDG_SESSION_TYPE");

    if (!sessionType) {
        PRINT_ERROR("Could not detect session type");
        return;
    }

    std::string session(sessionType);

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        if ((session == "x11" && line.starts_with("x11hooks ")) ||
            (session == "wayland" && line.starts_with("waylandhooks ")) ||
            line.starts_with("hooks ")) {

            // Extract the command part
            std::string cmd = line.substr(line.find(' ') + 1);

            // Replace $WARTPAPER with actual path
            size_t pos = cmd.find("$WARTPAPER");
            while (pos != std::string::npos) {
                cmd.replace(pos, 10, wallpaperPath);
                pos = cmd.find("$WARTPAPER");
            }

            logMessage(LogLevel::INFO, "Executing hook: " + cmd);
            if (system(cmd.c_str()) != 0) {
                logMessage(LogLevel::ERROR, "Hook failed: " + cmd);
            }
        }
    }
}

bool retryOperation(std::function<bool()> operation, int maxRetries = 3, int delaySeconds = 5) {
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        if (operation()) {
            return true;
        }

        if (attempt < maxRetries) {
            logMessage(LogLevel::WARNING, "Operation failed, retrying in " + std::to_string(delaySeconds) + " seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(delaySeconds));
        }
    }
    return false;
}

void reloadConfig(std::unordered_map<std::string, std::string>& config) {
    logMessage(LogLevel::INFO, "Reloading configuration...");
    std::unordered_map<std::string, std::string> newConfig;

    if (rcValidate(wartConfig, newConfig)) {
        config = newConfig;
        logMessage(LogLevel::INFO, "Configuration reloaded successfully");
    } else {
        logMessage(LogLevel::ERROR, "Failed to reload configuration");
    }
}

bool previewWallpaper() {
    std::unordered_map<std::string, std::string> config;
    if (!rcValidate(wartConfig, config)) {
        PRINT_ERROR("Failed to load configuration");
        return false;
    }

    if (fetchWallpaper(config)) {
        std::string wallpaperPath = wartHome + "wallpaper." + config["format"];

        const char* sessionType = getenv("XDG_SESSION_TYPE");
        if (!sessionType) {
            PRINT_ERROR("Could not detect session type");
            return false;
        }

        std::string session(sessionType);
        std::ifstream file(wartConfig);
        std::string line;
        std::string previewerCmd;

        // Find appropriate previewer
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            if ((session == "x11" && line.starts_with("x11previewer ")) ||
                (session == "wayland" && line.starts_with("waylandpreviewer ")) ||
                line.starts_with("previewer ")) {

                previewerCmd = line.substr(line.find(' ') + 1);
                break;
            }
        }

        if (previewerCmd.empty()) {
            // Fallback to default previewers if none specified
            if (session == "wayland") {
                if (system("which imv >/dev/null 2>&1") == 0) {
                    previewerCmd = "imv";
                } else if (system("which swayimg >/dev/null 2>&1") == 0) {
                    previewerCmd = "swayimg";
                }
            } else {
                if (system("which feh >/dev/null 2>&1") == 0) {
                    previewerCmd = "feh";
                } else if (system("which eog >/dev/null 2>&1") == 0) {
                    previewerCmd = "eog";
                }
            }

            if (previewerCmd.empty()) {
                previewerCmd = "xdg-open";
            }
        }

        // Replace $WARTPAPER with actual path
        std::string absPath = fs::absolute(wallpaperPath).string();
        size_t pos = previewerCmd.find("$WARTPAPER");
        while (pos != std::string::npos) {
            previewerCmd.replace(pos, 10, absPath);
            pos = previewerCmd.find("$WARTPAPER");
        }

        if (previewerCmd.find("$WARTPAPER") == std::string::npos) {
            previewerCmd += " " + absPath;
        }

        logMessage(LogLevel::INFO, "Previewing with: " + previewerCmd);
        return system(previewerCmd.c_str()) == 0;
    }
    return false;
}

void backupWallpaper(const std::string& currentWallpaper) {
    std::string backupPath = wartHome + "previous." +
                            fs::path(currentWallpaper).extension().string().substr(1);
    try {
        if (fs::exists(currentWallpaper)) {
            fs::copy_file(currentWallpaper, backupPath,
                         fs::copy_options::overwrite_existing);
        }
    } catch (const fs::filesystem_error& e) {
        PRINT_ERROR("Failed to backup wallpaper: " << e.what());
    }
}

bool restorePreviousWallpaper() {
    std::unordered_map<std::string, std::string> config;
    if (!rcValidate(wartConfig, config)) {
        return false;
    }

    std::string previousPath = wartHome + "previous." + config["format"];
    if (!fs::exists(previousPath)) {
        PRINT_ERROR("No previous wallpaper found");
        return false;
    }

    std::string currentPath = wartHome + "wallpaper." + config["format"];
    try {
        fs::copy_file(previousPath, currentPath,
                     fs::copy_options::overwrite_existing);
        return setWallpaper(currentPath);
    } catch (const fs::filesystem_error& e) {
        PRINT_ERROR("Failed to restore previous wallpaper: " << e.what());
        return false;
    }
}

void wartLoop(const std::unordered_map<std::string, std::string>& config) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    while (running) {
        if (config.at("clean") == "1") {
            cleanOldWallpapers(config.at("format"));
        }

        std::string wallpaperPath = wartHome + "wallpaper." + config.at("format");

        // Backup current wallpaper before fetching new one
        if (fs::exists(wallpaperPath)) {
            backupWallpaper(wallpaperPath);
        }

        if (fetchWallpaper(config)) {
            if (setWallpaper(wallpaperPath)) {
                std::cout << "Successfully set wallpaper" << std::endl;
                executeHooks(wallpaperPath);
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

bool wartInit() {
    // Check and create directory
    if (!fs::exists(wartHome)) {
        std::cout << "Creating directory: " << wartHome << std::endl;
        if (!fs::create_directory(wartHome)) {
            PRINT_ERROR("Failed to create directory " << wartHome);
            return false;
        }
    }

    // Check and create config file
    if (!fs::exists(wartConfig)) {
        std::cout << "Creating config file: " << wartConfig << std::endl;
        std::ofstream wartrc(wartConfig);
        if (!wartrc) {
            PRINT_ERROR("Failed to create config file " << wartConfig);
            return false;
        }

        // Write default configuration
        wartrc << "interval 3600\n"
               << "clean 1\n"
               << "resolution 1920x1200\n"
               << "format jpg\n"
               << "# Hook examples:\n"
               << "# x11hooks wal -i $WARTPAPER\n"
               << "# waylandhooks swww img $WARTPAPER\n"
               << "# hooks notify-send \"New wallpaper set\"\n"
               << "# Applier examples:\n"
               << "# x11applier feh --bg-fill $WARTPAPER\n"
               << "# waylandapplier swww img $WARTPAPER\n"
               << "# applier custom-wallpaper-script $WARTPAPER\n"
               << "# Previewer examples:\n"
               << "# x11previewer feh $WARTPAPER\n"
               << "# waylandpreviewer imv $WARTPAPER\n"
               << "# previewer custom-preview-script $WARTPAPER\n";

        wartrc.close();
        std::cout << "Configuration file created successfully." << std::endl;
    } else {
        std::cout << "Configuration file already exists." << std::endl;
    }

    return true;
}

int main(int argc, char *argv[]) {
    printVersion();

    bool daemon = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "init") {
            if (wartInit()) {
                return 0;
            }
        }
        else if (arg == "destroy") {
            wartDestroy();
            return 0;
        } else if (arg == "resolution" && i + 1 < argc) {
            updateResolution(argv[++i]);
            return 0;
        } else if (arg == "format" && i + 1 < argc) {
            updateFormat(argv[++i]);
            return 0;
        } else if (arg == "daemon" || arg == "-d") {
            daemon = true;
        } else if (arg == "help" || arg == "-h" || arg == "--help") {
            showHelp();
            return 0;
        } else if (arg == "interval" && i + 1 < argc) {
            updateInterval(argv[++i]);
            return 0;
        } else if (arg == "status") {
            showStatus();
            return 0;
        } else if (arg == "preview") {
            if (previewWallpaper()) {
                return 0;
            }
        } else if (arg == "restore") {
            if (restorePreviousWallpaper()) {
                std::cout << "Previous wallpaper restored succesfully" << std::endl;
                return 0;
            }
            return 1;
        }
    }

    if (!wartExists()) {
        std::cerr << "Wart setup failed!" << std::endl;
        return 1;
    }

    if (daemon && !daemonize()) {
        return 1;
    }

    if (!createLockFile()) {
        return 1;
    }

    std::unordered_map<std::string, std::string> config;
    if (!rcValidate(wartConfig, config)) {
        removeLockFile();
        return 1;
    }

    try {
        wartLoop(config);
    } catch (const std::exception& e) {
        logMessage(LogLevel::ERROR, "Exception in main loop: " + std::string(e.what()));
        removeLockFile();
        return 1;
    }

    removeLockFile();
    return 0;
}
