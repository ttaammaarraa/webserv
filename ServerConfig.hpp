#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <string>
#include <map>
#include <vector>

struct Location
{
    std::string path;
    std::string root;
    std::string index;
    bool autoindex;
    std::vector<std::string> allowed_methods;

    Location() : path(""), root(""), index("index.html"), autoindex(false) {}
    Location(const std::string& p) : path(p), root(""), index("index.html"), autoindex(false) {}
};

struct ServerConfig 
{
    int port;
    std::string host;
    std::string root;
    std::map<int, std::string> error_pages;
    std::vector<Location> locations;
    
    ServerConfig() : port(0), host(""), root("") {}
    
    const Location* matchLocation(const std::string& uri) const;
};

#endif
