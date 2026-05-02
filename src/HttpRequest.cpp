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

static std::string toLower(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
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

    // Handle Transfer-Encoding: chunked first (case-insensitive header names)
    std::string transferEnc;
    for (std::map<std::string, std::string>::const_iterator h = req._headers.begin(); h != req._headers.end(); ++h)
    {
        if (toLower(h->first) == "transfer-encoding")
        {
            transferEnc = toLower(h->second);
            break;
        }
    }

    const size_t bodyStart = headerEnd + 4;
    if (!transferEnc.empty() && transferEnc.find("chunked") != std::string::npos)
    {
        // Parse chunked transfer encoding
        size_t pos = bodyStart;
        
        while (pos < raw_request.size())
        {
            // Find the end of the chunk size line (CRLF)
            size_t crlf_pos = raw_request.find("\r\n", pos);
            if (crlf_pos == std::string::npos)
            {
                // Incomplete chunk size line; wait for more data
                break;
            }
            
            // Extract and trim the chunk size line (handles chunk extensions like "size;name=value")
            std::string size_line = trim(raw_request.substr(pos, crlf_pos - pos));
            if (size_line.empty())
            {
                // Malformed chunk, stop processing
                break;
            }
            
            // Parse hexadecimal chunk size (base 16)
            errno = 0;
            char *end = NULL;
            const char *begin = size_line.c_str();
            long chunk_size = std::strtol(begin, &end, 16);
            
            // Validate hex parse: must have consumed at least one character and no error
            if (begin == end || errno != 0 || chunk_size < 0)
            {
                // Invalid chunk size; malformed chunked body
                break;
            }
            
            pos = crlf_pos + 2;  // Move past the CRLF
            
            if (chunk_size == 0)
            {
                // Zero-sized chunk signals end of chunked body
                break;
            }
            
            // Safely cast and validate we have enough data
            size_t chunk_size_u = static_cast<size_t>(chunk_size);
            
            // Check if we have the complete chunk: chunk_data + trailing CRLF
            if (pos > raw_request.size() || 
                chunk_size_u > raw_request.size() - pos ||
                pos + chunk_size_u + 2 > raw_request.size())
            {
                // Not enough data for this chunk; partial buffer, wait for more
                break;
            }
            
            // Append chunk data to request body
            req._body.append(raw_request, pos, chunk_size_u);
            
            // Move position past chunk data and its trailing CRLF
            pos += chunk_size_u + 2;
        }
        return req;
    }

    // Otherwise, Extract body up to Content-Length, even when the body arrived only partially.
    std::map<std::string, std::string>::const_iterator it = req._headers.end();
    for (std::map<std::string, std::string>::const_iterator h = req._headers.begin(); h != req._headers.end(); ++h)
    {
        if (toLower(h->first) == "content-length") { it = h; break; }
    }
    if (it != req._headers.end())
    {
        size_t contentLength = 0;
        if (parseContentLength(it->second, contentLength))
        {
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
