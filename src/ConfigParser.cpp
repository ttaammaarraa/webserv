#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <iostream>
#include <stdexcept>

static std::string intToString(int x)
{
    std::ostringstream oss;
    oss << x;
    return oss.str();
}

const Location* ServerConfig::matchLocation(const std::string& uri) const
{
    const Location* best = NULL;
    size_t bestLength = 0;
    
    for (size_t i = 0; i < locations.size(); ++i)
    {
        const Location& loc = locations[i];
        size_t pathLen = loc.path.length();
        
        if (pathLen > 0 && pathLen <= uri.length() && uri.compare(0, pathLen, loc.path) == 0)
        {
            if (pathLen > bestLength)
            {
                best = &loc;
                bestLength = pathLen;
            }
        }
    }
    return best;
}

Location ConfigParser::parseLocation(std::ifstream& file, int& line_number)
{
    Location loc;
    int braceDepth = 1;
    std::string line;
    
    while (std::getline(file, line) && braceDepth > 0)
    {
        ++line_number;
        {
            std::string temp = line;
            size_t start = 0;
            while (start < temp.size() && std::isspace(static_cast<unsigned char>(temp[start])))
                ++start;
            line.erase(0, start);
            size_t end = line.size();
            while (end > 0 && std::isspace(static_cast<unsigned char>(line[end - 1])))
                --end;
            line.erase(end);
        }
        
        if (line.empty() || line[0] == '#')
            continue;
        
        for (size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == '{') ++braceDepth;
            if (line[i] == '}') --braceDepth;
        }
        
        if (braceDepth <= 0)
            break;
        
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        
        if (key == "root")
        {
            if (!(iss >> loc.root))
                throw std::runtime_error("Missing value for 'root' at line " + intToString(line_number));
        }
        else if (key == "index")
        {
            if (!(iss >> loc.index))
                throw std::runtime_error("Missing value for 'index' at line " + intToString(line_number));
        }
        else if (key == "autoindex")
        {
            std::string val;
            if (!(iss >> val))
                throw std::runtime_error("Missing value for 'autoindex' at line " + intToString(line_number));
            loc.autoindex = (val == "on" || val == "true");
        }
        else if (key == "allow_methods")
        {
            std::string method;
            while (iss >> method)
                loc.allowed_methods.push_back(method);
        }
    }
    return loc;
}

void ConfigParser::applyDirective(ServerConfig& config, const std::string& key, std::istringstream& iss, int line_number)
{
    typedef void (*StaticHandlerFunc)(ServerConfig&, std::istringstream&, int);

    struct HandlerEntry 
    {
        std::string key;
        StaticHandlerFunc func;
    };

    HandlerEntry handlers[] = {
        {"port", &ConfigParser::handlePort},
        {"host", &ConfigParser::handleHost},
        {"server_name", &ConfigParser::handleServerName},
        {"root", &ConfigParser::handleRoot},
        {"error_pages", &ConfigParser::handleErrorPages}
    };
    const int handler_count = sizeof(handlers) / sizeof(handlers[0]);

    bool found = false;
    for (int i = 0; i < handler_count; ++i)
    {
        if (handlers[i].key == key)
        {
            handlers[i].func(config, iss, line_number);
            found = true;
            break;
        }
    }

    if (!found)
        throw std::runtime_error("Invalid key '" + key + "' in config file at line " + intToString(line_number));
}

ServerConfig ConfigParser::parseServerBlock(std::ifstream& file, int& line_number)
{
    ServerConfig config;
    bool port_set = false;
    bool host_set = false;
    bool root_set = false;
    bool closed = false;
    std::string line;

    while (std::getline(file, line))
    {
        ++line_number;
        trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        if (line[0] == '}')
        {
            std::string remainder = line.substr(1);
            trim(remainder);
            if (!remainder.empty() && remainder[0] != '#')
                throw std::runtime_error("Unexpected content after '}' at line " + intToString(line_number));
            closed = true;
            break;
        }

        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "location")
        {
            std::string path;
            if (!(iss >> path))
                throw std::runtime_error("Missing path for 'location' at line " + intToString(line_number));

            Location loc(path);
            loc = parseLocation(file, line_number);
            loc.path = path;
            config.locations.push_back(loc);
            continue;
        }

        applyDirective(config, key, iss, line_number);

        if (key == "port")
            port_set = true;
        if (key == "host")
            host_set = true;
        if (key == "root")
            root_set = true;
    }

    if (!closed)
        throw std::runtime_error("Missing closing '}' for server block");
    if (!port_set)
        throw std::runtime_error("Missing required port value in server block");
    if (!host_set)
        throw std::runtime_error("Missing required host value in server block");
    if (!root_set)
        throw std::runtime_error("Missing required root value in server block");

    return config;
}

std::vector<ServerConfig> ConfigParser::parse(const std::string& filename) 
{
    std::vector<ServerConfig> configs;
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Could not open config file");
    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) 
    {
        ++line_number;
        trim(line);
        
        if (line.empty() || line[0] == '#')
            continue;
        
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key != "server")
            throw std::runtime_error("Expected 'server' block at line " + intToString(line_number));

        size_t brace_pos = line.find('{');
        if (brace_pos == std::string::npos)
            throw std::runtime_error("Missing '{' after 'server' at line " + intToString(line_number));

        std::string trailing = line.substr(brace_pos + 1);
        trim(trailing);
        if (!trailing.empty() && trailing[0] != '#')
            throw std::runtime_error("Unexpected content after 'server {' at line " + intToString(line_number));

        configs.push_back(parseServerBlock(file, line_number));
    }
    
    if (configs.empty())
        throw std::runtime_error("No server blocks found in config file");

    for (size_t i = 0; i < configs.size(); ++i)
    {
        for (size_t j = 0; j < i; ++j)
        {
            if (configs[i].port == configs[j].port && configs[i].server_name == configs[j].server_name)
                throw std::runtime_error("Duplicate server_name '" + configs[i].server_name + "' on port " + intToString(configs[i].port));
        }
    }
    
    return configs;
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

void ConfigParser::handlePort(ServerConfig& config, std::istringstream& iss, int line_number)
{
    int port;
    if (!(iss >> port) || iss.fail() || port < 1 || port > 65535)
        throw std::runtime_error("Invalid or missing port value for key 'port' at line " + intToString(line_number));
    config.port = port;
}

void ConfigParser::handleHost(ServerConfig& config, std::istringstream& iss, int line_number)
{
    if (!(iss >> config.host) || iss.fail())
        throw std::runtime_error("Missing host value for key 'host' at line " + intToString(line_number));
}

void ConfigParser::handleServerName(ServerConfig& config, std::istringstream& iss, int line_number)
{
    if (!(iss >> config.server_name) || iss.fail())
        throw std::runtime_error("Missing server_name value for key 'server_name' at line " + intToString(line_number));
}

void ConfigParser::handleRoot(ServerConfig& config, std::istringstream& iss, int line_number)
{
    if (!(iss >> config.root) || iss.fail())
        throw std::runtime_error("Missing root value for key 'root' at line " + intToString(line_number));
    if (config.root.find("..") != std::string::npos || config.root[0] == '/')
        throw std::runtime_error("Invalid root path for key 'root' at line " + intToString(line_number) + ": path traversal or absolute path detected");
}

void ConfigParser::handleErrorPages(ServerConfig& config, std::istringstream& iss, int line_number)
{
    int code;
    std::string path;
    if (!(iss >> code) || iss.fail())
        throw std::runtime_error("Missing error page code for key 'error_pages' at line " + intToString(line_number));
    if (!(iss >> path) || iss.fail())
        throw std::runtime_error("Missing error page path for key 'error_pages' at line " + intToString(line_number));
    if (code < 400 || code > 599)
        throw std::runtime_error("Invalid HTTP status code for key 'error_pages' at line " + intToString(line_number));
    if (path.find("..") != std::string::npos || path[0] == '/')
        throw std::runtime_error("Invalid error page path for key 'error_pages' at line " + intToString(line_number) + ": path traversal or absolute path detected");
    config.error_pages[code] = path;
}
