#include "ChunkedBodyParser.hpp"
#include <cerrno>
#include <climits>
#include <cstring>

ChunkedBodyParser::ChunkedBodyParser()
: _state(READ_SIZE_LINE), _chunkSize(0), _bytesRead(0), _lineBuffer(), _error(false), _totalSize(0)
{
}

ChunkedBodyParser::~ChunkedBodyParser()
{
}

bool ChunkedBodyParser::isDone() const { return _state == DONE; }
bool ChunkedBodyParser::hasError() const { return _error; }
size_t ChunkedBodyParser::totalSize() const { return _totalSize; }

static inline void skip_leading_spaces(const std::string& s, size_t& idx)
{
    while (idx < s.size() && (s[idx] == ' ' || s[idx] == '\t'))
        ++idx;
}

bool ChunkedBodyParser::parse(const std::string& raw, size_t& pos, std::string& body)
{
    if (_state == DONE || _error)
        return true;

    const size_t rawSize = raw.size();

    while (pos < rawSize)
    {
        switch (_state)
        {
            case READ_SIZE_LINE:
            {
                // Look for CRLF from pos
                size_t crlfPos = raw.find("\r\n", pos);
                if (crlfPos == std::string::npos)
                {
                    // partial size line: append all remaining and wait
                    _lineBuffer.append(raw, pos, rawSize - pos);
                    pos = rawSize;
                    return false;
                }

                // full size line available
                _lineBuffer.append(raw, pos, crlfPos - pos);
                pos = crlfPos + 2; // consume size line and CRLF
                _state = PARSE_SIZE;
                break;
            }

            case PARSE_SIZE:
            {
                // parse hex number from _lineBuffer, ignoring chunk-extensions after ';'
                const std::string& line = _lineBuffer;
                size_t idx = 0;
                skip_leading_spaces(line, idx);
                if (idx >= line.size())
                {
                    _error = true; _state = DONE; return true;
                }

                // find end of hex token (stop at ';' or whitespace)
                size_t endToken = idx;
                while (endToken < line.size())
                {
                    char c = line[endToken];
                    if (c == ';' || c == ' ' || c == '\t') break;
                    ++endToken;
                }

                // create temporary null-terminated buffer for strtoul
                size_t tokenLen = endToken - idx;
                if (tokenLen == 0 || tokenLen > 32) // 32 digits is plenty for safety
                {
                    _error = true; _state = DONE; return true;
                }

                char tmp[64];
                if (tokenLen >= sizeof(tmp)) { _error = true; _state = DONE; return true; }
                std::memcpy(tmp, line.c_str() + idx, tokenLen);
                tmp[tokenLen] = '\0';

                errno = 0;
                char* endptr = NULL;
                unsigned long parsed = std::strtoul(tmp, &endptr, 16);
                if (endptr == tmp || errno != 0)
                {
                    _error = true; _state = DONE; return true;
                }

                // make sure there were only hex digits in token
                // endptr should point to end of tmp (null) since we passed token only
                // check parsed size against limits
                const unsigned long maxAllowed = (unsigned long) ( (ChunkedBodyParser::MAX_BODY_SIZE < (size_t)ULONG_MAX) ? ChunkedBodyParser::MAX_BODY_SIZE : (size_t)ULONG_MAX );
                if (parsed > maxAllowed)
                {
                    _error = true; _state = DONE; return true;
                }

                _chunkSize = static_cast<size_t>(parsed);
                _bytesRead = 0;
                _lineBuffer.clear();

                if (_chunkSize == 0)
                {
                    _state = READ_TRAILERS;
                }
                else
                {
                    // Check for total size overflow before reading
                    if (_totalSize + _chunkSize > MAX_BODY_SIZE)
                    {
                        _error = true; _state = DONE; return true;
                    }
                    _state = READ_CHUNK_DATA;
                }
                break;
            }

            case READ_CHUNK_DATA:
            {
                size_t available = rawSize - pos;
                size_t need = _chunkSize - _bytesRead;
                size_t take = (available < need) ? available : need;
                if (take > 0)
                {
                    body.append(raw, pos, take);
                    pos += take;
                    _bytesRead += take;
                    _totalSize += take;
                }

                if (_bytesRead < _chunkSize)
                {
                    return false; // need more data
                }
                // else we've read the full chunk
                _state = READ_DATA_CRLF;
                break;
            }

            case READ_DATA_CRLF:
            {
                // Need exactly CRLF immediately after chunk data
                if (rawSize - pos < 2)
                {
                    return false; // partial CRLF
                }
                if (raw[pos] != '\r' || raw[pos+1] != '\n')
                {
                    _error = true; _state = DONE; return true;
                }
                pos += 2;
                // reset and continue for next chunk
                _chunkSize = 0;
                _bytesRead = 0;
                _state = READ_SIZE_LINE;
                break;
            }

            case READ_TRAILERS:
            {
                // A zero-sized chunk is followed by either an empty trailer section ("\r\n")
                // or by trailer headers terminated by "\r\n\r\n".
                if (rawSize - pos < 2)
                    return false;

                if (raw[pos] == '\r' && raw[pos + 1] == '\n')
                {
                    pos += 2;
                }
                else
                {
                    size_t endTrailers = raw.find("\r\n\r\n", pos);
                    if (endTrailers == std::string::npos)
                    {
                        return false; // need more data
                    }
                    // optionally parse trailer headers between pos and endTrailers (ignored here)
                    pos = endTrailers + 4;
                }

                _state = DONE;
                return true;
            }

            case DONE:
            default:
                return true;
        }
    }

    // Reached end of available raw data without finishing state; wait for more
    return false;
}
