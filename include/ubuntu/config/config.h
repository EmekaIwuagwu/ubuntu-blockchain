#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ubuntu {
namespace config {

/**
 * @brief Configuration manager
 *
 * Manages node configuration from files and command-line arguments.
 * Supports ubuntu.conf format with key=value pairs.
 */
class Config {
public:
    Config();
    ~Config();

    /**
     * @brief Load configuration from file
     *
     * @param filename Path to configuration file
     * @return true if loaded successfully
     */
    bool loadFromFile(const std::string& filename);

    /**
     * @brief Parse command-line arguments
     *
     * @param argc Argument count
     * @param argv Argument vector
     */
    void parseCommandLine(int argc, char* argv[]);

    /**
     * @brief Save configuration to file
     *
     * @param filename Path to save configuration
     * @return true if saved successfully
     */
    bool saveToFile(const std::string& filename) const;

    /**
     * @brief Get string value
     *
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

    /**
     * @brief Get integer value
     *
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value
     */
    int64_t getInt(const std::string& key, int64_t defaultValue = 0) const;

    /**
     * @brief Get boolean value
     *
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;

    /**
     * @brief Get double value
     *
     * @param key Configuration key
     * @param defaultValue Default value if not found
     * @return Configuration value
     */
    double getDouble(const std::string& key, double defaultValue = 0.0) const;

    /**
     * @brief Check if key exists
     *
     * @param key Configuration key
     * @return true if key exists
     */
    bool has(const std::string& key) const;

    /**
     * @brief Set string value
     *
     * @param key Configuration key
     * @param value Value to set
     */
    void set(const std::string& key, const std::string& value);

    /**
     * @brief Set integer value
     *
     * @param key Configuration key
     * @param value Value to set
     */
    void set(const std::string& key, int64_t value);

    /**
     * @brief Set boolean value
     *
     * @param key Configuration key
     * @param value Value to set
     */
    void set(const std::string& key, bool value);

    /**
     * @brief Get all configuration keys
     *
     * @return Vector of all keys
     */
    std::vector<std::string> getKeys() const;

    /**
     * @brief Get default configuration file path
     *
     * @return Default config path
     */
    static std::string getDefaultConfigPath();

    /**
     * @brief Get default data directory
     *
     * @return Default data directory path
     */
    static std::string getDefaultDataDir();

private:
    std::map<std::string, std::string> values_;

    /**
     * @brief Parse a single configuration line
     *
     * @param line Line to parse
     */
    void parseLine(const std::string& line);

    /**
     * @brief Trim whitespace from string
     *
     * @param str String to trim
     * @return Trimmed string
     */
    static std::string trim(const std::string& str);
};

}  // namespace config
}  // namespace ubuntu
