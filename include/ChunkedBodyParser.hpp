#ifndef CHUNKED_BODY_PARSER_HPP
# define CHUNKED_BODY_PARSER_HPP

# include <string>
# include <cstdlib>

class ChunkedBodyParser {
public:
    enum State { READ_SIZE_LINE, PARSE_SIZE, READ_CHUNK_DATA, READ_DATA_CRLF, READ_TRAILERS, DONE };

    ChunkedBodyParser();
    ~ChunkedBodyParser();

    // main function: consumes from `raw` starting at `pos`, appends to `body`.
    // Returns true when parsing finished (either successfully or an unrecoverable error occurred).
    // Returns false when more data is needed (partial parse).
    bool parse(const std::string& raw, size_t& pos, std::string& body);

    bool isDone() const;
    bool hasError() const;
    size_t totalSize() const;

private:
    State       _state;
    size_t      _chunkSize;
    size_t      _bytesRead;
    std::string _lineBuffer; // buffer for partial size line
    bool        _error;
    size_t      _totalSize;

    static const size_t MAX_BODY_SIZE = 10 * 1024 * 1024; // 10MB cap
};

#endif
