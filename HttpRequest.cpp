#include "HttpRequest.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

HttpRequest::HttpRequest() {}

static std::string trim(const std::string& s) 
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

HttpRequest HttpRequest::parse(const std::string& raw_request) 
{
    HttpRequest req;
    std::istringstream stream(raw_request);
    std::string line;

    // Parse request line
    if (std::getline(stream, line)) 
    {
        std::istringstream req_line(line);
        req_line >> req.method >> req.path >> req.version;
    }

    // Parse headers
    while (std::getline(stream, line)) {
        if (line == "\r" || trim(line).empty()) 
            break; // End of headers
        size_t colon = line.find(":");
        if (colon != std::string::npos) {
            std::string key = trim(line.substr(0, colon));
            std::string value = trim(line.substr(colon + 1));
            req.headers[key] = value;
        }
    }
    return req;
}
