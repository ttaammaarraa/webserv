#include "ServerUtils.hpp"

#include <stdexcept>

ServerConfig& ServerUtils::matchServer(int port, std::vector<ServerConfig>& configs)
{
    if (configs.empty())
        throw std::runtime_error("No server configurations available");

    std::vector<ServerConfig>::iterator it;
    for (it = configs.begin(); it != configs.end(); ++it)
    {
        if (it->port == port)
            return *it;
    }

    return configs[0];
}
