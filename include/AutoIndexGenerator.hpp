#ifndef AUTOINDEXGENERATOR_HPP
#define AUTOINDEXGENERATOR_HPP

#include <string>

class AutoIndexGenerator
{
public:
    static std::string generate(const std::string& dirPath, const std::string& requestPath);

private:
    static std::string joinPath(const std::string& base, const std::string& suffix);
    static std::string escapeHtml(const std::string& value);
};

#endif
