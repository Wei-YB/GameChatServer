#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace base {
class Config {
public:
    using string = std::string;
    using strVec = std::vector<std::string>;

    Config(const std::string& file);

    bool success() const {
        return !fail_flag_;
    }

    bool exists(const string& key);
    bool getInt(const string& key, int& val);
    bool getBool(const string& key, bool& val);
    bool getString(const string& key, string& str);
    bool getStringVec(const string& key, strVec& vec);

    static bool fileExist(const std::string& file);

private:
    std::unordered_map<string, string> configs_;
    bool                               fail_flag_;
};
}
