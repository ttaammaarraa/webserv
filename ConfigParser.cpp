#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <iostream>
#include <stdexcept>


ServerConfig ConfigParser::parse(const std::string& filename) 
{
    ServerConfig config;
    bool port_set = false;
    bool host_set = false;
    bool root_set = false;
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Could not open config file");
    std::string line;
    int line_number = 0;
    // Helper for int to string (C++98 compatible)
    #define INT_TO_STRING(x) (static_cast<std::ostringstream&>(std::ostringstream() << (x)).str())
    while (std::getline(file, line)) 
    {
        ++line_number;
        trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        if (key == "port") 
        {
            if (port_set)
                throw std::runtime_error("Duplicate port key in config file at line " + INT_TO_STRING(line_number));
            int port;
            if (!(iss >> port) || iss.fail() || port < 1 || port > 65535)
                throw std::runtime_error("Invalid or missing port value for key 'port' at line " + INT_TO_STRING(line_number));
            config.port = port;
            port_set = true;
        } 
        else if (key == "host") 
        {
            if (host_set)
                throw std::runtime_error("Duplicate host key in config file at line " + INT_TO_STRING(line_number));
            if (!(iss >> config.host) || iss.fail())
                throw std::runtime_error("Missing host value for key 'host' at line " + INT_TO_STRING(line_number));
            host_set = true;
        } 
        else if (key == "root") 
        {
            if (root_set)
                throw std::runtime_error("Duplicate root key in config file at line " + INT_TO_STRING(line_number));
            if (!(iss >> config.root) || iss.fail())
                throw std::runtime_error("Missing root value for key 'root' at line " + INT_TO_STRING(line_number));
            if (config.root.find("..") != std::string::npos || config.root[0] == '/')
                throw std::runtime_error("Invalid root path for key 'root' at line " + INT_TO_STRING(line_number) + ": path traversal or absolute path detected");
            root_set = true;
        } 
        else if (key == "error_pages") 
        {
            int code;
            std::string path;
            if (!(iss >> code) || iss.fail())
                throw std::runtime_error("Missing error page code for key 'error_pages' at line " + INT_TO_STRING(line_number));
            if (!(iss >> path) || iss.fail())
                throw std::runtime_error("Missing error page path for key 'error_pages' at line " + INT_TO_STRING(line_number));
            if (code < 400 || code > 599)
                throw std::runtime_error("Invalid HTTP status code for key 'error_pages' at line " + INT_TO_STRING(line_number));
            if (path.find("..") != std::string::npos || path[0] == '/')
                throw std::runtime_error("Invalid error page path for key 'error_pages' at line " + INT_TO_STRING(line_number) + ": path traversal or absolute path detected");
            config.error_pages[code] = path;
        }
        else 
            throw std::runtime_error("Invalid key '" + key + "' in config file at line " + INT_TO_STRING(line_number));
    }
    if (!port_set)
        throw std::runtime_error("Missing required port value in config file");
    if (!host_set)
        throw std::runtime_error("Missing required host value in config file");
    if (!root_set)
        throw std::runtime_error("Missing required root value in config file");
    return config;
}

void ConfigParser::trim(std::string& s) 
{
    // Left trim
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;
    s.erase(0, start);
    // Right trim
    size_t end = s.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    s.erase(end);
}
