#include "Config.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>

using namespace std;
using namespace chatServer;

std::string parseKey(const string& str, int& index) {
    while (index < str.size() && str[index] == ' ')
        ++index;
    string key;
    while (index < str.size() && str[index] != ' ')
        key.push_back(str[index++]);
    return key;
}

std::string parseValue(const string& str, int& index) {
    while (index < str.size() && str[index] == ' ')
        ++index;
    string value;
    if (index != str.size())
        value = str.substr(index);
    return value;
}

bool validLine(const string& str, int& index) {
    while (index < str.size() && str[index] == ' ')
        ++index;
    if (index < str.size() && str[index] == '#')
        return false;
    return true;
}

// TODO: make config parser better
std::pair<string, string> parseLine(const string& str) {
    int index = 0;
    if (!validLine(str, index))
        return {};
    auto key = parseKey(str, index);
    auto value = parseValue(str, index);
    return {key, value};
}

Config::Config(const string& file) {
    if (!fileExist(file)) {
        fail_flag_ = true;
        return;
    }
    fail_flag_ = false;
    // read the config file
    ifstream ifs(file);
    string   line_str;
    while (getline(ifs, line_str)) {
        if (line_str.empty())
            continue;
        auto [key, value] = parseLine(line_str);
        if (key.empty())
            continue;
        configs_[key] = value;
    }
}

bool Config::exists(const string& key) {
    return configs_.count(key);
}

bool Config::getInt(const string& key, int& val) {
    if (!exists(key))
        return false;
    auto value = configs_[key];
    try {
        val = stoi(value);
    }
    catch (exception& e) {
        return false;
    }
    return true;
}

bool Config::getBool(const string& key, bool& val) {
    if (!exists(key))
        return false;
    auto value = configs_[key];
    if (value == "yes")
        val = true;
    else if (value == "no")
        val = false;
    else
        return false;
    return true;
}

bool Config::getString(const string& key, string& str) {
    if (!exists(key))
        return false;
    str = configs_[key];
    return true;
}

bool Config::getStringVec(const string& key, strVec& vec) {
    if (!exists(key))
        return false;
    stringstream is(configs_[key]);
    string       str;
    while (is >> str) {
        vec.push_back(str);
    }
    return true;
}

bool Config::fileExist(const std::string& file) {
    struct stat buffer{};
    const auto  ret = ::stat(file.c_str(), &buffer);
    if (ret < 0) {
        return false;
    }
    return true;
}
