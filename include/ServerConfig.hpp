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
    std::string upload_path;
    bool autoindex;
    std::vector<std::string> allowed_methods;

    Location() : path(""), root(""), index("index.html"), upload_path(""), autoindex(false) {}
    Location(const std::string& p) : path(p), root(""), index("index.html"), upload_path(""), autoindex(false) {}
};

struct ServerConfig 
{
    int port;
    std::string host;
    std::string server_name;
    std::string root;
    std::map<int, std::string> error_pages;
    std::vector<Location> locations;
    
    ServerConfig() : port(0), host(""), server_name(""), root("") {}
    
    const Location* matchLocation(const std::string& uri) const;
};

#endif
