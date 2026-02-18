#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>


ServerConfig ConfigParser::parse(const std::string& filename) 
{
    ServerConfig config;
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Could not open config file");
    std::string line;
    while (std::getline(file, line)) 
    {
        trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        if (key == "port") 
        {
            iss >> config.port;
        } 
        else if (key == "host") 
        {
            iss >> config.host;
        } 
        else if (key == "root") 
        {
            iss >> config.root;
        } 
        else if (key == "error_pages") 
        {
            int code;
            std::string path;
            iss >> code >> path;
            config.error_pages[code] = path;
        }
        else 
         throw std::runtime_error("Invalid key in config file");
    }
    return config;
}

void ConfigParser::trim(std::string& s) 
{
    // Left trim
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;
    s = s.substr(start);
    // Right trim
    size_t end = s.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    s = s.substr(0, end);
}
