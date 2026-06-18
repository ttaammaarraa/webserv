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
    std::string cgi_pass;
    bool autoindex;
    size_t client_max_body_size;
    std::vector<std::string> allowed_methods;

    Location()
        : path(""), root(""), index("index.html"), upload_path(""), cgi_pass(""), autoindex(false), client_max_body_size(0) {}
    Location(const std::string& p)
        : path(p), root(""), index("index.html"), upload_path(""), cgi_pass(""), autoindex(false), client_max_body_size(0) {}
};

struct ServerConfig
{
    int port;
    std::string host;
    std::string server_name;
    std::string root;
    std::map<int, std::string> error_pages;
    std::vector<Location> locations;
    size_t client_max_body_size;

    ServerConfig() : port(0), host(""), server_name(""), root(""), client_max_body_size(104857600) {}

    const Location* matchLocation(const std::string& uri) const;
    const Location* matchLocationForRequest(const std::string& uri, const std::string& method) const;
};

#endif
