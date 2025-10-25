#include "ubuntu/config/config.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ubuntu {
namespace config {

Config::Config() = default;

Config::~Config() = default;

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        spdlog::warn("Failed to open config file: {}", filename);
        return false;
    }

    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        ++lineNum;

        // Skip empty lines and comments
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        parseLine(line);
    }

    spdlog::info("Loaded configuration from {}", filename);
    return true;
}

void Config::parseCommandLine(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Skip help flags
        if (arg == "-h" || arg == "--help") {
            continue;
        }

        // Parse -key=value format
        if (arg[0] == '-') {
            arg = arg.substr(1);  // Remove leading dash

            size_t eqPos = arg.find('=');
            if (eqPos != std::string::npos) {
                std::string key = arg.substr(0, eqPos);
                std::string value = arg.substr(eqPos + 1);
                values_[key] = value;
            } else {
                // Boolean flag without value
                values_[arg] = "1";
            }
        }
    }

    spdlog::debug("Parsed {} command-line arguments", values_.size());
}

bool Config::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) {
        spdlog::error("Failed to open config file for writing: {}", filename);
        return false;
    }

    file << "# Ubuntu Blockchain Configuration File\n";
    file << "# Generated automatically\n\n";

    for (const auto& [key, value] : values_) {
        file << key << "=" << value << "\n";
    }

    file.close();
    spdlog::info("Saved configuration to {}", filename);
    return true;
}

std::string Config::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = values_.find(key);
    if (it != values_.end()) {
        return it->second;
    }
    return defaultValue;
}

int64_t Config::getInt(const std::string& key, int64_t defaultValue) const {
    auto it = values_.find(key);
    if (it != values_.end()) {
        try {
            return std::stoll(it->second);
        } catch (...) {
            spdlog::warn("Invalid integer value for key '{}': {}", key, it->second);
        }
    }
    return defaultValue;
}

bool Config::getBool(const std::string& key, bool defaultValue) const {
    auto it = values_.find(key);
    if (it != values_.end()) {
        const std::string& value = it->second;
        if (value == "1" || value == "true" || value == "yes" || value == "on") {
            return true;
        }
        if (value == "0" || value == "false" || value == "no" || value == "off") {
            return false;
        }
    }
    return defaultValue;
}

double Config::getDouble(const std::string& key, double defaultValue) const {
    auto it = values_.find(key);
    if (it != values_.end()) {
        try {
            return std::stod(it->second);
        } catch (...) {
            spdlog::warn("Invalid double value for key '{}': {}", key, it->second);
        }
    }
    return defaultValue;
}

bool Config::has(const std::string& key) const {
    return values_.find(key) != values_.end();
}

void Config::set(const std::string& key, const std::string& value) {
    values_[key] = value;
}

void Config::set(const std::string& key, int64_t value) {
    values_[key] = std::to_string(value);
}

void Config::set(const std::string& key, bool value) {
    values_[key] = value ? "1" : "0";
}

std::vector<std::string> Config::getKeys() const {
    std::vector<std::string> keys;
    keys.reserve(values_.size());
    for (const auto& [key, _] : values_) {
        keys.push_back(key);
    }
    return keys;
}

std::string Config::getDefaultConfigPath() {
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.ubuntu-blockchain/ubuntu.conf";
    }
    return "./ubuntu.conf";
}

std::string Config::getDefaultDataDir() {
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.ubuntu-blockchain";
    }
    return "./.ubuntu-blockchain";
}

void Config::parseLine(const std::string& line) {
    size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) {
        spdlog::warn("Invalid config line (no '='): {}", line);
        return;
    }

    std::string key = trim(line.substr(0, eqPos));
    std::string value = trim(line.substr(eqPos + 1));

    // Remove quotes from value if present
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }

    values_[key] = value;
}

std::string Config::trim(const std::string& str) {
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

}  // namespace config
}  // namespace ubuntu
