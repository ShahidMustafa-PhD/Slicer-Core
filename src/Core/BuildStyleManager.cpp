// ==============================================================================
// MarcSLM - Build Style Manager Implementation
// ==============================================================================
// Minimal JSON parser for BuildStyle arrays.
// ==============================================================================

#include "MarcSLM/Core/BuildStyleManager.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace MarcSLM {

namespace {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n\"");
    auto end   = s.find_last_not_of(" \t\r\n\"");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

bool tryGetString(const std::string& json, const std::string& key, std::string& out) {
    std::string searchKey = "\"" + key + "\"";
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    auto valStart = json.find('"', pos + 1);
    if (valStart == std::string::npos) return false;
    auto valEnd = json.find('"', valStart + 1);
    if (valEnd == std::string::npos) return false;
    out = json.substr(valStart + 1, valEnd - valStart - 1);
    return true;
}

bool tryGetDouble(const std::string& json, const std::string& key, double& out) {
    std::string searchKey = "\"" + key + "\"";
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    auto valStart = json.find_first_not_of(" \t\r\n", pos + 1);
    if (valStart == std::string::npos) return false;
    auto valEnd = json.find_first_of(",}\r\n", valStart);
    std::string valStr = trim(json.substr(valStart, valEnd - valStart));
    try { out = std::stod(valStr); return true; }
    catch (...) { return false; }
}

bool tryGetInt(const std::string& json, const std::string& key, int& out) {
    double d = 0.0;
    if (tryGetDouble(json, key, d)) { out = static_cast<int>(d); return true; }
    return false;
}

BuildStyle parseStyleBlock(const std::string& block) {
    BuildStyle s;
    tryGetInt(block,    "id",                s.id);
    tryGetString(block, "name",              s.name);
    tryGetString(block, "description",       s.description);
    tryGetInt(block,    "laserId",           s.laserId);
    tryGetInt(block,    "laserMode",         s.laserMode);
    tryGetDouble(block, "laserPower",        s.laserPower);
    tryGetDouble(block, "laserFocus",        s.laserFocus);
    tryGetDouble(block, "laserSpeed",        s.laserSpeed);
    tryGetDouble(block, "hatchSpacing",      s.hatchSpacing);
    tryGetDouble(block, "layerThickness",    s.layerThickness);
    tryGetDouble(block, "pointDistance",      s.pointDistance);
    tryGetInt(block,    "pointDelay",        s.pointDelay);
    tryGetInt(block,    "pointExposureTime", s.pointExposureTime);
    tryGetDouble(block, "jumpSpeed",         s.jumpSpeed);
    tryGetDouble(block, "jumpDelay",         s.jumpDelay);
    tryGetDouble(block, "hatchSize",         s.hatchSize);
    return s;
}

} // anonymous namespace

bool BuildStyleManager::loadFromFile(const std::filesystem::path& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "BuildStyleManager: Failed to open " << filepath << std::endl;
            return false;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string json = ss.str();
        file.close();

        // Find the "buildStyles" array
        auto arrayStart = json.find("\"buildStyles\"");
        if (arrayStart == std::string::npos) {
            std::cerr << "BuildStyleManager: No 'buildStyles' key found" << std::endl;
            return false;
        }

        auto arrBegin = json.find('[', arrayStart);
        if (arrBegin == std::string::npos) return false;

        styles.clear();

        // Parse each object in the array
        size_t pos = arrBegin + 1;
        while (pos < json.size()) {
            auto objStart = json.find('{', pos);
            if (objStart == std::string::npos) break;

            // Find matching closing brace (simple non-nested)
            int braceCount = 1;
            size_t objEnd = objStart + 1;
            while (objEnd < json.size() && braceCount > 0) {
                if (json[objEnd] == '{') ++braceCount;
                else if (json[objEnd] == '}') --braceCount;
                ++objEnd;
            }

            std::string block = json.substr(objStart, objEnd - objStart);
            styles.push_back(parseStyleBlock(block));
            pos = objEnd;
        }

        std::cout << "BuildStyleManager: Loaded " << styles.size()
                  << " build styles" << std::endl;
        return !styles.empty();
    } catch (const std::exception& e) {
        std::cerr << "BuildStyleManager: " << e.what() << std::endl;
        return false;
    }
}

bool BuildStyleManager::saveToFile(const std::filesystem::path& filepath) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        file << "{\n  \"buildStyles\": [\n";
        for (size_t i = 0; i < styles.size(); ++i) {
            const auto& s = styles[i];
            file << "    {\n"
                 << "      \"id\": " << s.id << ",\n"
                 << "      \"name\": \"" << s.name << "\",\n"
                 << "      \"description\": \"" << s.description << "\",\n"
                 << "      \"laserId\": " << s.laserId << ",\n"
                 << "      \"laserMode\": " << s.laserMode << ",\n"
                 << "      \"laserPower\": " << s.laserPower << ",\n"
                 << "      \"laserFocus\": " << s.laserFocus << ",\n"
                 << "      \"laserSpeed\": " << s.laserSpeed << ",\n"
                 << "      \"hatchSpacing\": " << s.hatchSpacing << ",\n"
                 << "      \"layerThickness\": " << s.layerThickness << ",\n"
                 << "      \"pointDistance\": " << s.pointDistance << ",\n"
                 << "      \"pointDelay\": " << s.pointDelay << ",\n"
                 << "      \"pointExposureTime\": " << s.pointExposureTime << ",\n"
                 << "      \"jumpSpeed\": " << s.jumpSpeed << ",\n"
                 << "      \"jumpDelay\": " << s.jumpDelay << ",\n"
                 << "      \"hatchSize\": " << s.hatchSize << "\n"
                 << "    }";
            if (i + 1 < styles.size()) file << ",";
            file << "\n";
        }
        file << "  ]\n}\n";
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "BuildStyleManager: " << e.what() << std::endl;
        return false;
    }
}

const BuildStyle* BuildStyleManager::findById(int id) const noexcept {
    for (const auto& s : styles) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

} // namespace MarcSLM
