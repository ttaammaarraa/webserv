#ifndef SERVER_UTILS_HPP
#define SERVER_UTILS_HPP

#include <string>
#include <vector>
#include "ServerConfig.hpp"

namespace ServerUtils
{
    std::string extractHostFromRawRequest(const std::string& rawRequest);
    ServerConfig& matchServer(const std::string& hostHeader, int port, std::vector<ServerConfig>& configs);
}

#endif
