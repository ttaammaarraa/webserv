#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <string>
#include <map>
#include <vector>

struct ServerConfig 
{
    int port;
    std::string host;
    std::string root;
    std::map<int, std::string> error_pages;
    ServerConfig() : port(0), host(""), root("") {}

};

#endif
