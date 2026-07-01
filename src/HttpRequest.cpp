#include "HttpRequest.hpp"
#include "ChunkedBodyParser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cerrno>

HttpRequest::HttpRequest() : _upload_fd(-1), _contentLength(0), _bodyReceived(0), _complete(false) {}

const std::string& HttpRequest::getMethod() const { return _method; }
const std::string& HttpRequest::getPath() const { return _path; }
const std::string& HttpRequest::getVersion() const { return _version; }
const std::map<std::string, std::string>& HttpRequest::getHeaders() const { return _headers; }
const std::string& HttpRequest::getBody() const { return _body; }

bool HttpRequest::isComplete() const { return _complete; }
int HttpRequest::getUploadFd() const { return _upload_fd; }
size_t HttpRequest::getContentLength() const { return _contentLength; }

void HttpRequest::setMethod(const std::string& method) { _method = method; }
void HttpRequest::setPath(const std::string& path) { _path = path; }
void HttpRequest::setVersion(const std::string& version) { _version = version; }
void HttpRequest::setUploadFd(int fd) { _upload_fd = fd; }
void HttpRequest::setContentLength(size_t length) { _contentLength = length; }
void HttpRequest::setComplete(bool complete) { _complete = complete; }

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
        // Use ChunkedBodyParser for Transfer-Encoding: chunked
        ChunkedBodyParser parser;
        size_t pos = bodyStart;
        
        // Parse chunked data; parser maintains state and appends decoded chunks to req._body
        parser.parse(raw_request, pos, req._body);
        
        // Handle parsing errors
        if (parser.hasError())
        {
            // Malformed chunked encoding; return what we have
            // Server will handle the invalid request appropriately
            return req;
        }
        
        // If parsing is not complete, request needs more data
        if (!parser.isDone())
        {
            // Incomplete chunked data; return partial body
            // Server will wait for more data on subsequent reads
            return req;
        }
        
        // Successfully parsed complete chunked body
        req._complete = true;
        return req;
    }

    // Otherwise, extract body metadata from Content-Length without storing it in _body.
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
            req._contentLength = contentLength;
            if (bodyStart < raw_request.size() && contentLength > 0)
            {
                const size_t available = raw_request.size() - bodyStart;
                req._bodyReceived = (available < contentLength) ? available : contentLength;
                if (req._bodyReceived >= req._contentLength)
                    req._body = raw_request.substr(bodyStart, req._bodyReceived);
                else
                    req._body.clear();
            }
            else
            {
                req._body.clear();
            }
            req._complete = (req._bodyReceived >= req._contentLength);
        }
        else
        {
            req._complete = true;
        }
    }
    else
    {
        req._complete = true;
    }

    return req;
}
