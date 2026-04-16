#include "HttpRequest.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cerrno>

HttpRequest::HttpRequest() {}

const std::string& HttpRequest::getMethod() const { return _method; }
const std::string& HttpRequest::getPath() const { return _path; }
const std::string& HttpRequest::getVersion() const { return _version; }
const std::map<std::string, std::string>& HttpRequest::getHeaders() const { return _headers; }
const std::string& HttpRequest::getBody() const { return _body; }

void HttpRequest::setMethod(const std::string& method) { _method = method; }
void HttpRequest::setPath(const std::string& path) { _path = path; }
void HttpRequest::setVersion(const std::string& version) { _version = version; }

static std::string trim(const std::string& s) 
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

static bool parseContentLength(const std::string& value, size_t& outLength)
{
    errno = 0;
    char *end = NULL;
    const char *begin = value.c_str();
    long parsed = std::strtol(begin, &end, 10);

    if (begin == end || errno != 0 || parsed < 0)
        return false;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
        ++end;
    if (*end != '\0')
        return false;

    outLength = static_cast<size_t>(parsed);
    return true;
}

HttpRequest HttpRequest::parse(const std::string& raw_request) 
{
    HttpRequest req;
    req._body.clear();

    const size_t headerEnd = raw_request.find("\r\n\r\n");
    const std::string head = (headerEnd == std::string::npos)
        ? raw_request
        : raw_request.substr(0, headerEnd);

    std::istringstream stream(head);
    std::string line;

    // Parse request line
    if (std::getline(stream, line)) 
    {
        std::istringstream req_line(line);
        req_line >> req._method >> req._path >> req._version;
    }

    // Parse headers
    while (std::getline(stream, line)) {
        if (line == "\r" || trim(line).empty()) 
            break; // End of headers
        size_t colon = line.find(":");
        if (colon != std::string::npos) {
            std::string key = trim(line.substr(0, colon));
            std::string value = trim(line.substr(colon + 1));
            req._headers[key] = value;
        }
    }

    // If headers are incomplete, body start marker is missing; keep body empty safely.
    if (headerEnd == std::string::npos)
        return req;

    // Extract body up to Content-Length, even when the body arrived only partially.
    std::map<std::string, std::string>::const_iterator it = req._headers.find("Content-Length");
    if (it != req._headers.end())
    {
        size_t contentLength = 0;
        if (parseContentLength(it->second, contentLength))
        {
            const size_t bodyStart = headerEnd + 4;
            if (bodyStart < raw_request.size() && contentLength > 0)
            {
                const size_t available = raw_request.size() - bodyStart;
                const size_t toRead = (available < contentLength) ? available : contentLength;
                req._body.append(raw_request, bodyStart, toRead);
            }
        }
    }

    return req;
}
