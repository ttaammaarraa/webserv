#include "ServerUtils.hpp"

#include <stdexcept>
#include <cctype>

static std::string trimCopy(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;

    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;

    return s.substr(start, end - start);
}

std::string ServerUtils::extractHostFromRawRequest(const std::string& rawRequest)
{
    size_t lineStart = 0;
    while (lineStart < rawRequest.size())
    {
        size_t lineEnd = rawRequest.find("\r\n", lineStart);
        if (lineEnd == std::string::npos)
            break;

        std::string line = rawRequest.substr(lineStart, lineEnd - lineStart);
        if (line.empty())
            break;

        if (line.size() >= 5 && line.compare(0, 5, "Host:") == 0)
        {
            std::string hostValue = trimCopy(line.substr(5));
            size_t colonPos = hostValue.find(':');
            if (colonPos != std::string::npos)
                hostValue = hostValue.substr(0, colonPos);
            return hostValue;
        }

        lineStart = lineEnd + 2;
    }

    return "";
}

ServerConfig& ServerUtils::matchServer(const std::string& hostHeader, int port, std::vector<ServerConfig>& configs)
{
    if (configs.empty())
        throw std::runtime_error("No server configurations available");

    std::vector<ServerConfig>::iterator it;
    std::vector<ServerConfig>::iterator defaultIt = configs.end();

    for (it = configs.begin(); it != configs.end(); ++it)
    {
        if (it->port == port)
        {
            if (defaultIt == configs.end())
                defaultIt = it;
            if (!hostHeader.empty() && it->server_name == hostHeader)
                return *it;
        }
    }

    if (defaultIt != configs.end())
        return *defaultIt;

    return configs[0];
}
