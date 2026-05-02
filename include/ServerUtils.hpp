#ifndef SERVER_UTILS_HPP
#define SERVER_UTILS_HPP

#include <string>
#include <vector>
#include "ServerConfig.hpp"

class ServerUtils
{
    public:
        static ServerConfig& matchServer(int port, std::vector<ServerConfig>& configs);
};

#endif
