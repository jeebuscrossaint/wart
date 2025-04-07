#include "wart.hh"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace wart {

// Global state
std::atomic<bool> running{true};

void logMessage(LogLevel level, const std::string &message) {
  static const std::unordered_map<LogLevel, std::string> levelStrings = {
      {LogLevel::DEBUG, "DEBUG"},
      {LogLevel::INFO, "INFO"},
      {LogLevel::WARNING, "WARNING"},
      {LogLevel::ERROR, "ERROR"}};

  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);

  std::cout << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << " ["
            << levelStrings.at(level) << "] " << message << std::endl;
}

// CURL callback function
static size_t writeMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
  size_t realsize = size * nmemb;
  auto *mem = static_cast<MemoryBuffer *>(userp);

  if (!mem->append(contents, realsize)) {
    LOG_ERROR("Memory allocation failed");
    return 0;
  }

  return realsize;
}

// Configuration validation functions
bool validateInterval(const std::string &value) {
  try {
    int interval = std::stoi(value);
    return interval > 0;
  } catch (...) {
    return false;
  }
}

bool validateResolution(const std::string &value) {
  static const std::vector<std::string> validResolutions = {
      "UHD",      "1920x1200", "1920x1080", "1366x768", "1280x768",
      "1024x768", "800x600",   "800x480",   "768x1280", "720x1280",
      "640x480",  "480x800",   "400x240",   "320x240",  "240x320"};
  return std::find(validResolutions.begin(), validResolutions.end(), value) !=
         validResolutions.end();
}

bool validateFormat(std::string_view value) {
  return value == "jpg" || value == "webp" || value == "png";
}

bool validateBoolean(const std::string &value) {
  return value == "0" || value == "1" || value == "true" || value == "false" ||
         value == "yes" || value == "no";
}

// Load configuration from file
bool loadConfig(const std::string &filepath, Config &config) {
  std::ifstream file(filepath);
  if (!file) {
    LOG_ERROR("Cannot open config file: " + filepath);
    return false;
  }

  config.values.clear();
  std::string line;

  while (std::getline(file, line)) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#')
      continue;

    std::istringstream iss(line);
    std::string key, value;

    if (!(iss >> key >> value)) {
      LOG_ERROR("Malformed line in config file: " + line);
      continue; // Skip malformed lines but continue processing
    }

    config.values[key] = value;
  }

  return validateConfig(config);
}

// Validate configuration values
bool validateConfig(const Config &config) {
  bool valid = true;

  if (!validateInterval(config.get("interval", "3600"))) {
    LOG_ERROR("'interval' must be an integer > 0");
    valid = false;
  }

  if (!validateBoolean(config.get("clean", "1"))) {
    LOG_ERROR("'clean' must be 0 or 1");
    valid = false;
  }

  if (!validateResolution(config.get("resolution", "1920x1080"))) {
    LOG_ERROR("'resolution' must be a valid resolution");
    valid = false;
  }

  if (!validateFormat(config.get("format", "jpg"))) {
    LOG_ERROR("'format' must be jpg, webp, or png");
    valid = false;
  }

  return valid;
}

// Create and initialize wart configuration
bool initializeWart() {
  // Create home directory if it doesn't exist
  if (!fs::exists(WART_HOME)) {
    std::cout << "Creating directory: " << WART_HOME << std::endl;
    if (!fs::create_directory(WART_HOME)) {
      LOG_ERROR("Failed to create directory " + WART_HOME);
      return false;
    }
  }

  // Create config file if it doesn't exist
  if (!fs::exists(WART_CONFIG)) {
    std::cout << "Creating file: " << WART_CONFIG << std::endl;
    std::ofstream wartrc(WART_CONFIG);
    if (!wartrc) {
      LOG_ERROR("Failed to create file " + WART_CONFIG);
      return false;
    }

    // Write default configuration
    wartrc << "interval 3600\n"
           << "clean 1\n"
           << "resolution 1920x1080\n"
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
           << "# previewer xdg-open $WARTPAPER\n";

    wartrc.close();
  }

  // Validate the configuration
  Config config;
  if (loadConfig(WART_CONFIG, config)) {
    std::cout << "Config is valid!" << std::endl;
    std::cout << "Interval: " << config.get("interval") << " seconds"
              << std::endl;
    std::cout << "Clean: " << (config.getBool("clean") ? "Enabled" : "Disabled")
              << std::endl;
    std::cout << "Resolution: " << config.get("resolution") << std::endl;
    std::cout << "Format: " << config.get("format") << std::endl;
    std::cout << "Wart is healthy." << std::endl;
    return true;
  } else {
    std::cerr << "Invalid config. Exiting." << std::endl;
    return false;
  }
}

// Process lock file management
bool createLockFile() {
  if (fs::exists(WART_LOCK)) {
    // Check if the process is still running
    std::ifstream lock(WART_LOCK);
    pid_t pid;
    lock >> pid;

    if (kill(pid, 0) == 0) {
      LOG_ERROR("Another instance is already running");
      return false;
    }

    // Remove stale lockfile
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

// Clean old wallpapers
void cleanOldWallpapers(const std::string &format) {
  const std::string ext = "." + format;
  try {
    for (const auto &entry : fs::directory_iterator(WART_HOME)) {
      // Only remove wallpaper files, not previous or special files
      if (entry.path().extension() == ext &&
          entry.path().filename().string().find("wallpaper") !=
              std::string::npos &&
          entry.path().filename().string() != ("wallpaper" + ext) &&
          entry.path().filename().string() != ("previous" + ext)) {
        fs::remove(entry.path());
      }
    }
  } catch (const fs::filesystem_error &e) {
    logMessage(LogLevel::ERROR,
               std::string("Error cleaning wallpapers: ") + e.what());
  }
}

// Backup current wallpaper
void backupWallpaper(const std::string &currentWallpaper) {
  std::string backupPath =
      WART_HOME + "previous." +
      fs::path(currentWallpaper).extension().string().substr(1);
  try {
    if (fs::exists(currentWallpaper)) {
      fs::copy_file(currentWallpaper, backupPath,
                    fs::copy_options::overwrite_existing);
    }
  } catch (const fs::filesystem_error &e) {
    logMessage(LogLevel::ERROR,
               std::string("Failed to backup wallpaper: ") + e.what());
  }
}

// Restore previous wallpaper
bool restorePreviousWallpaper(const Config &config) {
  std::string previousPath = WART_HOME + "previous." + config.get("format");
  if (!fs::exists(previousPath)) {
    LOG_ERROR("No previous wallpaper found");
    return false;
  }

  std::string currentPath = WART_HOME + "wallpaper." + config.get("format");
  try {
    fs::copy_file(previousPath, currentPath,
                  fs::copy_options::overwrite_existing);
    return setWallpaper(currentPath);
  } catch (const fs::filesystem_error &e) {
    LOG_ERROR(std::string("Failed to restore previous wallpaper: ") + e.what());
    return false;
  }
}

// Fetch wallpaper from API
bool fetchWallpaper(const Config &config) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    LOG_ERROR("Failed to initialize CURL");
    return false;
  }

  // Construct URL with parameters
  std::string url =
      "https://bing.biturl.top/?resolution=" + config.get("resolution") +
      "&format=json&index=0&mkt=en-US";

  logMessage(LogLevel::INFO, "Fetching from URL: " + url);

  MemoryBuffer chunk;

  // Set curl options
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void *>(&chunk));
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,
                   ("Mozilla/5.0 Wart/" + std::string(VERSION)).c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Set timeout to 30 seconds

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    LOG_ERROR(std::string("Failed to fetch wallpaper data: ") +
              curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return false;
  }

  try {
    // Parse JSON response
    json response = json::parse(chunk.data());

    // Extract image URL
    std::string imageUrl = response["url"];
    logMessage(LogLevel::INFO, "Image URL: " + imageUrl);

    // Create a new CURL handle for downloading the image
    CURL *imgCurl = curl_easy_init();
    if (!imgCurl) {
      LOG_ERROR("Failed to initialize CURL for image download");
      curl_easy_cleanup(curl);
      return false;
    }

    // Construct image filename
    std::string filename = WART_HOME + "wallpaper." + config.get("format");
    FILE *fp = fopen(filename.c_str(), "wb");
    if (!fp) {
      LOG_ERROR("Failed to create image file");
      curl_easy_cleanup(curl);
      curl_easy_cleanup(imgCurl);
      return false;
    }

    // Download image
    curl_easy_setopt(imgCurl, CURLOPT_URL, imageUrl.c_str());
    curl_easy_setopt(imgCurl, CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(imgCurl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(imgCurl, CURLOPT_TIMEOUT,
                     60L); // Set timeout to 60 seconds

    res = curl_easy_perform(imgCurl);
    fclose(fp);

    if (res != CURLE_OK) {
      LOG_ERROR(std::string("Failed to download image: ") +
                curl_easy_strerror(res));
      curl_easy_cleanup(curl);
      curl_easy_cleanup(imgCurl);
      return false;
    }

    curl_easy_cleanup(imgCurl);

  } catch (const json::exception &e) {
    LOG_ERROR(std::string("JSON parsing failed: ") + e.what());
    curl_easy_cleanup(curl);
    return false;
  }

  curl_easy_cleanup(curl);
  return true;
}

// Set wallpaper using configured applier
bool setWallpaper(const std::string &path) {
  const char *sessionType = getenv("XDG_SESSION_TYPE");
  if (!sessionType) {
    LOG_ERROR("Could not detect session type");
    return false;
  }

  std::string session(sessionType);
  std::ifstream file(WART_CONFIG);
  std::string line;
  std::string applierCmd;

  // Find appropriate applier
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;

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
      LOG_ERROR("No applier configured and no fallback available");
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
    applierCmd += " " + absPath; // Append path if $WARTPAPER not in command
  }

  logMessage(LogLevel::INFO, "Setting wallpaper with: " + applierCmd);
  return system(applierCmd.c_str()) == 0;
}

// Execute configured hooks
void executeHooks(const std::string &wallpaperPath) {
  std::ifstream file(WART_CONFIG);
  std::string line;
  const char *sessionType = getenv("XDG_SESSION_TYPE");

  if (!sessionType) {
    LOG_ERROR("Could not detect session type");
    return;
  }

  std::string session(sessionType);
  std::string absPath = fs::absolute(wallpaperPath).string();

  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    if ((session == "x11" && line.starts_with("x11hooks ")) ||
        (session == "wayland" && line.starts_with("waylandhooks ")) ||
        line.starts_with("hooks ")) {

      // Extract the command part
      std::string cmd = line.substr(line.find(' ') + 1);

      // Replace $WARTPAPER with actual path
      size_t pos = cmd.find("$WARTPAPER");
      while (pos != std::string::npos) {
        cmd.replace(pos, 10, absPath);
        pos = cmd.find("$WARTPAPER");
      }

      logMessage(LogLevel::INFO, "Executing hook: " + cmd);
      if (system(cmd.c_str()) != 0) {
        logMessage(LogLevel::ERROR, "Hook failed: " + cmd);
      }
    }
  }
}

// Preview wallpaper with configured previewer
bool previewWallpaper(const Config &config) {
  if (fetchWallpaper(config)) {
    std::string wallpaperPath = WART_HOME + "wallpaper." + config.get("format");

    const char *sessionType = getenv("XDG_SESSION_TYPE");
    if (!sessionType) {
      LOG_ERROR("Could not detect session type");
      return false;
    }

    std::string session(sessionType);
    std::ifstream file(WART_CONFIG);
    std::string line;
    std::string previewerCmd;

    // Find appropriate previewer
    while (std::getline(file, line)) {
      if (line.empty() || line[0] == '#')
        continue;

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

// Update config parameter
void updateConfigParameter(const std::string &paramName,
                           const std::string &value,
                           std::function<bool(const std::string &)> validator) {
  if (!validator(value)) {
    LOG_ERROR("Invalid " + paramName + ": " + value);
    return;
  }

  std::vector<std::string> lines;
  lines.reserve(20); // Reserve space to avoid reallocations
  std::ifstream file(WART_CONFIG);
  std::string line;
  bool paramFound = false;

  while (std::getline(file, line)) {
    if (!line.empty() && line[0] != '#') { // Skip comments
      std::istringstream iss(line);
      std::string key;
      iss >> key;

      if (key == paramName) {
        lines.push_back(paramName + " " + value);
        paramFound = true;
        continue;
      }
    }
    lines.push_back(line);
  }

  // Add parameter if not found
  if (!paramFound) {
    lines.push_back(paramName + " " + value);
  }

  std::ofstream outFile(WART_CONFIG);
  for (const auto &l : lines) {
    outFile << l << "\n";
  }

  logMessage(LogLevel::INFO, "Updated " + paramName + " to " + value);
}

// Display status information
void showStatus() {
  Config config;
  if (!loadConfig(WART_CONFIG, config)) {
    LOG_ERROR("Failed to load configuration");
    return;
  }

  std::string wallpaperPath = WART_HOME + "wallpaper." + config.get("format");

  if (!fs::exists(wallpaperPath)) {
    std::cout << "No wallpaper has been downloaded yet." << std::endl;
    return;
  }

  std::cout << "Current configuration:" << std::endl;
  std::cout << "Resolution: " << config.get("resolution") << std::endl;
  std::cout << "Format: " << config.get("format") << std::endl;
  std::cout << "Interval: " << config.get("interval") << " seconds"
            << std::endl;
  std::cout << "Clean mode: "
            << (config.getBool("clean") ? "Enabled" : "Disabled") << std::endl;

  // Show wallpaper file info
  std::cout << "Current wallpaper: " << wallpaperPath << std::endl;
  std::cout << "Size: " << fs::file_size(wallpaperPath) << " bytes"
            << std::endl;
  auto ftime = fs::last_write_time(wallpaperPath);
  auto sctp = std::chrono::file_clock::to_sys(ftime);
  auto time = std::chrono::system_clock::to_time_t(sctp);
  std::cout << "Last updated: " << std::ctime(&time);
}

// Main wallpaper update loop
void wartLoop(const Config &config) {
  signal(SIGINT, [](int) { running = false; });
  signal(SIGTERM, [](int) { running = false; });

  while (running) {
    if (config.getBool("clean")) {
      cleanOldWallpapers(config.get("format"));
    }

    std::string wallpaperPath = WART_HOME + "wallpaper." + config.get("format");

    // Backup current wallpaper before fetching new one
    if (fs::exists(wallpaperPath)) {
      backupWallpaper(wallpaperPath);
    }

    bool success = false;
    for (int attempt = 1; attempt <= 3 && !success; ++attempt) {
      if (attempt > 1) {
        logMessage(LogLevel::WARNING,
                   "Retry attempt " + std::to_string(attempt) + "...");
        std::this_thread::sleep_for(std::chrono::seconds(5));
      }

      success = fetchWallpaper(config);
    }

    if (success) {
      if (setWallpaper(wallpaperPath)) {
        logMessage(LogLevel::INFO, "Successfully set wallpaper");
        executeHooks(wallpaperPath);
      } else {
        LOG_ERROR("Failed to set wallpaper");
      }
    } else {
      LOG_ERROR("Failed to fetch wallpaper after multiple attempts");
    }

    // Sleep for the configured interval, checking running flag every second
    logMessage(LogLevel::INFO,
               "Sleeping for " + config.get("interval") + " seconds...");
    for (int i = 0; i < config.getInt("interval") && running; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  logMessage(LogLevel::INFO, "Shutting down gracefully");
}

// Daemonize the process
bool daemonize() {
  pid_t pid = fork();
  if (pid < 0) {
    LOG_ERROR("Failed to fork");
    return false;
  }
  if (pid > 0) {
    exit(0); // Parent exits
  }

  // Child process continues
  setsid(); // Create a new session

  // Redirect std file descriptors with error checking
  if (!std::freopen("/dev/null", "r", stdin) ||
      !std::freopen("/dev/null", "w", stdout) ||
      !std::freopen("/dev/null", "w", stderr)) {
    LOG_ERROR("Failed to redirect standard streams");
    return false;
  }

  return true;
}

// Destroy wart files and configuration
void showHelp() {
  std::cout
      << "Wart - Wallpaper Art\n"
      << "Usage: wart [command] [options]\n\n"
      << "Commands:\n"
      << "  resolution <res>   Set wallpaper resolution (e.g., 1920x1080, "
         "UHD)\n"
      << "  init              Initialize wart configuration\n"
      << "  format <fmt>      Set image format (jpg, webp, png)\n"
      << "  interval <sec>    Set update interval in seconds\n"
      << "  status           Show current configuration and wallpaper status\n"
      << "  preview          Download and preview next wallpaper\n"
      << "  destroy          Remove all wart files and configurations\n"
      << "  daemon, -d       Run in daemon mode\n"
      << "  help, -h         Show this help message\n"
      << "  restore          Restore previous wallpaper\n\n"
      << "Example:\n"
      << "  wart resolution UHD\n"
      << "  wart format webp\n"
      << "  wart preview\n"
      << "  wart -d\n";
}

void printVersion() {
  std::cout << "Running on Wart: " << VERSION << '\n'
            << "libcurl version: "
            << curl_version_info(CURLVERSION_NOW)->version << '\n'
            << "nlohmann json version: " << NLOHMANN_JSON_VERSION_MAJOR << "."
            << NLOHMANN_JSON_VERSION_MINOR << "." << NLOHMANN_JSON_VERSION_PATCH
            << std::endl;
}

void wartDestroy() {
  try {
    fs::remove(WART_CONFIG);
    fs::remove_all(WART_HOME);
    removeLockFile();
    std::cout << "Wart has been destroyed." << std::endl;
  } catch (const fs::filesystem_error &e) {
    LOG_ERROR(std::string("Error during destruction: ") + e.what());
  }
}

// Main function
int main(int argc, char *argv[]) {
  printVersion();

  bool daemon = false;
  Config config;

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "init") {
      return initializeWart() ? 0 : 1;
    } else if (arg == "destroy") {
      wartDestroy();
      return 0;
    } else if (arg == "resolution" && i + 1 < argc) {
      updateConfigParameter("resolution", argv[++i], validateResolution);
      return 0;
    } else if (arg == "format" && i + 1 < argc) {
      updateConfigParameter("format", argv[++i], validateFormat);
      return 0;
    } else if (arg == "interval" && i + 1 < argc) {
      updateConfigParameter("interval", argv[++i], validateInterval);
      return 0;
    } else if (arg == "daemon" || arg == "-d") {
      daemon = true;
    } else if (arg == "help" || arg == "-h" || arg == "--help") {
      showHelp();
      return 0;
    } else if (arg == "status") {
      showStatus();
      return 0;
    } else if (arg == "preview") {
      if (loadConfig(WART_CONFIG, config) && previewWallpaper(config)) {
        return 0;
      }
      return 1;
    } else if (arg == "restore") {
      if (loadConfig(WART_CONFIG, config) && restorePreviousWallpaper(config)) {
        std::cout << "Previous wallpaper restored successfully" << std::endl;
        return 0;
      }
      return 1;
    }
  }

  // Initialize wart if running without commands
  if (!initializeWart()) {
    return 1;
  }

  // Daemonize if requested
  if (daemon && !daemonize()) {
    return 1;
  }

  // Create lock file
  if (!createLockFile()) {
    return 1;
  }

  // Load configuration
  if (!loadConfig(WART_CONFIG, config)) {
    removeLockFile();
    return 1;
  }

  try {
    // Run main loop
    wartLoop(config);
  } catch (const std::exception &e) {
    logMessage(LogLevel::ERROR,
               std::string("Exception in main loop: ") + e.what());
    removeLockFile();
    return 1;
  }

  removeLockFile();
  return 0;
}

} // namespace wart

// Entry point outside namespace
int main(int argc, char *argv[]) { return wart::main(argc, argv); }
