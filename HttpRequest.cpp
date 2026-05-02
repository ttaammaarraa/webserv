#include "HttpRequest.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cerrno>

// Maximum decoded body size accepted (10 MB) – guards against memory exhaustion.
static const size_t MAX_BODY_SIZE = 10 * 1024 * 1024;

// Maximum number of hex digits allowed in a single chunk-size field (8 hex digits = 4 GB).
// We cap it lower than 8 so that adding up chunks stays well inside size_t on 32-bit builds.
static const size_t MAX_CHUNK_SIZE_HEX_DIGITS = 7; // up to 0xFFFFFFF = 268 MB per chunk

HttpRequest::HttpRequest() : _chunkedError(false) {}

const std::string& HttpRequest::getMethod()  const { return _method;  }
const std::string& HttpRequest::getPath()    const { return _path;    }
const std::string& HttpRequest::getVersion() const { return _version; }
const std::map<std::string, std::string>& HttpRequest::getHeaders() const { return _headers; }
const std::string& HttpRequest::getBody()    const { return _body;    }
bool               HttpRequest::isChunkedError() const { return _chunkedError; }

void HttpRequest::setMethod (const std::string& method)  { _method  = method;  }
void HttpRequest::setPath   (const std::string& path)    { _path    = path;    }
void HttpRequest::setVersion(const std::string& version) { _version = version; }

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static std::string trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

static bool parseContentLength(const std::string& value, size_t& outLength)
{
    errno = 0;
    char* end      = NULL;
    const char* begin = value.c_str();
    long parsed    = std::strtol(begin, &end, 10);

    if (begin == end || errno != 0 || parsed < 0)
        return false;
    // Reject trailing non-whitespace
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
        ++end;
    if (*end != '\0')
        return false;

    outLength = static_cast<size_t>(parsed);
    return true;
}

/**
 * parseChunkedBody – decode an HTTP/1.1 chunked-encoded body.
 *
 * @param raw        Full raw request string.
 * @param bodyStart  Byte offset where the body (chunk stream) begins.
 * @param body       Output: decoded body data.
 * @return           false if a hard protocol error was detected (invalid hex,
 *                   missing CRLF, body exceeds MAX_BODY_SIZE, etc.).
 *                   true  on success or on a clean end-of-data (partial chunk
 *                   stream with no detected protocol error).
 *
 * Security properties enforced here:
 *   - Chunk-size fields longer than MAX_CHUNK_SIZE_HEX_DIGITS are rejected.
 *   - Non-hexadecimal characters in the chunk-size field are rejected.
 *   - A running total that would exceed MAX_BODY_SIZE is rejected.
 *   - Each chunk must be followed by \r\n; absence is a hard error.
 *   - Chunk extensions (semicolon-delimited) are silently stripped per RFC 7230.
 */
static bool parseChunkedBody(const std::string& raw, size_t bodyStart, std::string& body)
{
    body.clear();
    size_t pos = bodyStart;
    const size_t rawLen = raw.size();

    while (pos < rawLen)
    {
        // --- locate end of chunk-size line (must be terminated by \r\n) ---
        size_t lineEnd = raw.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            return false; // truncated: no CRLF at all – hard error

        std::string sizeLine = raw.substr(pos, lineEnd - pos);

        // Strip chunk extensions (everything from ';' onward)
        size_t extPos = sizeLine.find(';');
        if (extPos != std::string::npos)
            sizeLine = sizeLine.substr(0, extPos);

        // Trim horizontal whitespace from the size token
        size_t first = sizeLine.find_first_not_of(" \t");
        if (first == std::string::npos)
            return false; // blank size line
        sizeLine = sizeLine.substr(first);
        size_t last = sizeLine.find_last_not_of(" \t");
        if (last != std::string::npos)
            sizeLine = sizeLine.substr(0, last + 1);

        if (sizeLine.empty())
            return false; // empty chunk-size token

        // Guard: reject unreasonably long hex strings (potential integer overflow attack)
        if (sizeLine.size() > MAX_CHUNK_SIZE_HEX_DIGITS)
            return false;

        // Validate: every character must be a hex digit
        for (size_t i = 0; i < sizeLine.size(); ++i)
        {
            if (!std::isxdigit(static_cast<unsigned char>(sizeLine[i])))
                return false;
        }

        // Parse chunk size
        errno = 0;
        char* endPtr = NULL;
        unsigned long chunkSize = std::strtoul(sizeLine.c_str(), &endPtr, 16);
        if (errno != 0 || endPtr != sizeLine.c_str() + sizeLine.size())
            return false;

        pos = lineEnd + 2; // advance past the \r\n following chunk-size

        // --- last chunk ---
        if (chunkSize == 0)
        {
            // Consume optional trailing headers and final \r\n (RFC 7230 §4.1.2)
            // We simply stop decoding here; the connection-level parser handles the epilogue.
            return true;
        }

        // --- guard: accumulated body must not exceed the limit ---
        if (body.size() + chunkSize > MAX_BODY_SIZE)
            return false;

        // --- read chunk-data ---
        if (pos + chunkSize > rawLen)
        {
            // Stream ends in the middle of chunk data; append what arrived.
            body.append(raw, pos, rawLen - pos);
            return true; // partial but no detected error
        }

        body.append(raw, pos, chunkSize);
        pos += chunkSize;

        // --- mandatory CRLF after chunk-data ---
        if (pos + 2 > rawLen)
            return false; // truncated: missing trailing CRLF

        if (raw[pos] != '\r' || raw[pos + 1] != '\n')
            return false; // protocol violation: expected \r\n after chunk data

        pos += 2;
    }

    // Reached end of input without a terminating zero-chunk; not a hard error
    // (stream may still be arriving), but we return true so the caller can use
    // whatever data was decoded.
    return true;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

HttpRequest HttpRequest::parse(const std::string& raw_request)
{
    HttpRequest req;

    const size_t headerEnd = raw_request.find("\r\n\r\n");
    const std::string head = (headerEnd == std::string::npos)
        ? raw_request
        : raw_request.substr(0, headerEnd);

    std::istringstream stream(head);
    std::string line;

    // Parse request line
    if (std::getline(stream, line))
    {
        std::istringstream reqLine(line);
        reqLine >> req._method >> req._path >> req._version;
    }

    // Parse headers
    while (std::getline(stream, line))
    {
        if (line == "\r" || trim(line).empty())
            break;
        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            std::string key   = trim(line.substr(0, colon));
            std::string value = trim(line.substr(colon + 1));
            req._headers[key] = value;
        }
    }

    // Without the header/body separator there is no body to decode.
    if (headerEnd == std::string::npos)
        return req;

    const size_t bodyStart = headerEnd + 4; // skip \r\n\r\n

    // -----------------------------------------------------------------------
    // Determine body encoding: prefer Transfer-Encoding over Content-Length
    // per RFC 7230 §3.3.3 rule 3.
    // -----------------------------------------------------------------------

    std::map<std::string, std::string>::const_iterator teIt =
        req._headers.find("Transfer-Encoding");

    if (teIt != req._headers.end())
    {
        // Normalise to lower-case for comparison
        std::string te = teIt->second;
        for (size_t i = 0; i < te.size(); ++i)
            te[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(te[i])));

        if (te.find("chunked") != std::string::npos)
        {
            if (!parseChunkedBody(raw_request, bodyStart, req._body))
            {
                req._chunkedError = true;
                req._body.clear(); // discard any partial data on hard error
            }
            return req;
        }
    }

    // Fall back to Content-Length
    std::map<std::string, std::string>::const_iterator clIt =
        req._headers.find("Content-Length");

    if (clIt != req._headers.end())
    {
        size_t contentLength = 0;
        if (parseContentLength(clIt->second, contentLength))
        {
            if (bodyStart < raw_request.size() && contentLength > 0)
            {
                const size_t available = raw_request.size() - bodyStart;
                const size_t toRead    = (available < contentLength) ? available : contentLength;
                req._body.append(raw_request, bodyStart, toRead);
            }
        }
    }

    return req;
}
