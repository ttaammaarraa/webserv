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
    while (std::getline(file, line)) 
    {
        trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        if (key == "port") 
        {
            int port;
            if (!(iss >> port) || iss.fail() || port < 1 || port > 65535)
                throw std::runtime_error("Invalid or missing port value in config file");
            config.port = port;
            port_set = true;
        } 
        else if (key == "host") 
        {
            if (!(iss >> config.host) || iss.fail())
                throw std::runtime_error("Missing host value in config file");
            host_set = true;
        } 
        else if (key == "root") 
        {
            if (!(iss >> config.root) || iss.fail())
                throw std::runtime_error("Missing root value in config file");
            if (config.root.find("..") != std::string::npos || config.root[0] == '/')
                throw std::runtime_error("Invalid root path in config file: path traversal or absolute path detected");
            root_set = true;
        } 
        else if (key == "error_pages") 
        {
            int code;
            std::string path;
            if (!(iss >> code) || iss.fail())
                throw std::runtime_error("Missing error page code in config file");
            if (!(iss >> path) || iss.fail())
                throw std::runtime_error("Missing error page path in config file");
            if (code < 400 || code > 599)
                throw std::runtime_error("Invalid HTTP status code in error_pages");
            if (path.find("..") != std::string::npos || path[0] == '/')
                throw std::runtime_error("Invalid error page path in config file: path traversal or absolute path detected");
            config.error_pages[code] = path;
        }
        else 
         throw std::runtime_error("Invalid key in config file");
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
    s = s.substr(start);
    // Right trim
    size_t end = s.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    s = s.substr(0, end);
}
