/**
 * HttpRequestChunkedTest.cpp
 *
 * Stress tests and edge-case validation for the HTTP Chunked Transfer-Encoding
 * parser. Each test corresponds to a class of malformed or unusual raw HTTP
 * requests that could crash, hang, or corrupt memory in a naive implementation.
 *
 * Raw request strings used here can be piped directly into the server for
 * black-box testing (e.g.  printf '<request>' | nc 127.0.0.1 8080).
 */

#include "../HttpRequest.hpp"
#include <cassert>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Helper: build a raw POST request with the given chunked body payload.
// ---------------------------------------------------------------------------
static std::string makeChunkedRequest(const std::string& chunkedPayload)
{
    return "POST /upload HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Transfer-Encoding: chunked\r\n"
           "\r\n" + chunkedPayload;
}

// ---------------------------------------------------------------------------
// 1. Well-formed chunked request – baseline / sanity check
//    Raw: "POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
//         "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n"
// ---------------------------------------------------------------------------
static void test_valid_chunked_simple()
{
    std::string raw = makeChunkedRequest("4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n");
    HttpRequest req = HttpRequest::parse(raw);

    assert(!req.isChunkedError());
    assert(req.getBody() == "Wikipedia");
}

// ---------------------------------------------------------------------------
// 2. Chunk with extension – semicolon-delimited data after chunk size must be
//    stripped silently (RFC 7230 §4.1.1).
//    Raw chunk-size line: "4;name=value\r\nWiki\r\n0\r\n\r\n"
// ---------------------------------------------------------------------------
static void test_chunk_extension_ignored()
{
    std::string raw = makeChunkedRequest("4;name=value\r\nWiki\r\n0\r\n\r\n");
    HttpRequest req = HttpRequest::parse(raw);

    assert(!req.isChunkedError());
    assert(req.getBody() == "Wiki");
}

// ---------------------------------------------------------------------------
// 3. MALFORMED – Invalid hexadecimal characters in chunk size.
//    A chunk-size line containing non-hex characters (e.g. "ZZ") must be
//    rejected and isChunkedError() must be true; the body must be empty.
//    Raw: "POST /upload HTTP/1.1\r\n...\r\n\r\nZZ\r\ndata\r\n0\r\n\r\n"
// ---------------------------------------------------------------------------
static void test_invalid_hex_in_chunk_size()
{
    // 'ZZ' and 'GG' are not valid hex digits
    std::string raw = makeChunkedRequest("ZZ\r\ndata\r\n0\r\n\r\n");
    HttpRequest req = HttpRequest::parse(raw);

    assert(req.isChunkedError());
    assert(req.getBody().empty());

    // Also test with mixed valid/invalid characters
    std::string raw2 = makeChunkedRequest("4G\r\ndata\r\n0\r\n\r\n");
    HttpRequest req2 = HttpRequest::parse(raw2);
    assert(req2.isChunkedError());
    assert(req2.getBody().empty());
}

// ---------------------------------------------------------------------------
// 4. MALFORMED – Missing \r\n terminator after chunk data.
//    The byte immediately after the chunk payload must be \r\n. Without it the
//    parser must signal a hard error rather than silently reading garbage as the
//    next chunk-size.
//    Raw: "POST /upload HTTP/1.1\r\n...\r\n\r\n4\r\nWiki5\r\npedia\r\n0\r\n\r\n"
//    (The \r\n between "Wiki" and "5\r\n" is absent.)
// ---------------------------------------------------------------------------
static void test_missing_crlf_after_chunk_data()
{
    // "4\r\nWiki" should be followed by \r\n but immediately has "5\r\n"
    std::string raw = makeChunkedRequest("4\r\nWiki5\r\npedia\r\n0\r\n\r\n");
    HttpRequest req = HttpRequest::parse(raw);

    assert(req.isChunkedError());
    assert(req.getBody().empty());
}

// ---------------------------------------------------------------------------
// 5. MALFORMED – Extremely large chunk size (potential integer overflow / DoS).
//    A chunk-size field with more hex digits than MAX_CHUNK_SIZE_HEX_DIGITS
//    must be rejected immediately before any allocation is attempted.
//    Raw chunk-size line: "FFFFFFFFFFFFFFFF\r\n..." (16 hex digits)
// ---------------------------------------------------------------------------
static void test_chunk_size_too_large_hex_string()
{
    // 16-digit hex number – far exceeds MAX_CHUNK_SIZE_HEX_DIGITS (7)
    std::string raw = makeChunkedRequest("FFFFFFFFFFFFFFFF\r\ndata\r\n0\r\n\r\n");
    HttpRequest req = HttpRequest::parse(raw);

    assert(req.isChunkedError());
    assert(req.getBody().empty());
}

// ---------------------------------------------------------------------------
// 6. MALFORMED – Non-terminated (truncated) stream: chunk data arrives but the
//    stream ends before the zero-length terminator chunk.  The parser must not
//    crash or loop indefinitely; it should return whatever partial data it has
//    decoded and must NOT set isChunkedError (partial delivery is not a protocol
//    error from the parser's perspective).
//    Raw: "POST /upload HTTP/1.1\r\n...\r\n\r\n4\r\nWiki\r\n5\r\nped"
//    (stream ends mid-chunk; "ia\r\n0\r\n\r\n" never arrives)
// ---------------------------------------------------------------------------
static void test_non_terminated_stream()
{
    std::string raw = makeChunkedRequest("4\r\nWiki\r\n5\r\nped");
    HttpRequest req = HttpRequest::parse(raw);

    // Parser must not crash.  Body contains whatever was decoded before truncation.
    assert(!req.isChunkedError()); // truncation is NOT a hard protocol error
    // "Wiki" was a complete chunk; "ped" is partial data from the second chunk
    // Our implementation appends what it can when the stream ends mid-chunk.
    assert(req.getBody().find("Wiki") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 7. MALFORMED – Zero-length chunk-size token (blank line where chunk size
//    should appear).
//    Raw: "POST /upload HTTP/1.1\r\n...\r\n\r\n\r\ndata\r\n0\r\n\r\n"
// ---------------------------------------------------------------------------
static void test_blank_chunk_size_line()
{
    // The chunk-size line is empty (just \r\n before "data")
    std::string raw = makeChunkedRequest("\r\ndata\r\n0\r\n\r\n");
    HttpRequest req = HttpRequest::parse(raw);

    assert(req.isChunkedError());
    assert(req.getBody().empty());
}

// ---------------------------------------------------------------------------
// 8. Transfer-Encoding header value is case-insensitive ("CHUNKED" vs "chunked")
// ---------------------------------------------------------------------------
static void test_chunked_case_insensitive()
{
    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: CHUNKED\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "0\r\n\r\n";
    HttpRequest req = HttpRequest::parse(raw);

    assert(!req.isChunkedError());
    assert(req.getBody() == "hello");
}

// ---------------------------------------------------------------------------
// 9. Transfer-Encoding takes priority over Content-Length (RFC 7230 §3.3.3)
// ---------------------------------------------------------------------------
static void test_transfer_encoding_takes_priority_over_content_length()
{
    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 99\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "0\r\n\r\n";
    HttpRequest req = HttpRequest::parse(raw);

    // Should use chunked decoding, not Content-Length
    assert(!req.isChunkedError());
    assert(req.getBody() == "hello");
}

// ---------------------------------------------------------------------------
// 10. Empty chunked body – zero-chunk sent immediately
// ---------------------------------------------------------------------------
static void test_empty_chunked_body()
{
    std::string raw = makeChunkedRequest("0\r\n\r\n");
    HttpRequest req = HttpRequest::parse(raw);

    assert(!req.isChunkedError());
    assert(req.getBody().empty());
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main()
{
    test_valid_chunked_simple();
    test_chunk_extension_ignored();
    test_invalid_hex_in_chunk_size();
    test_missing_crlf_after_chunk_data();
    test_chunk_size_too_large_hex_string();
    test_non_terminated_stream();
    test_blank_chunk_size_line();
    test_chunked_case_insensitive();
    test_transfer_encoding_takes_priority_over_content_length();
    test_empty_chunked_body();

    std::cout << "All HttpRequest chunked encoding tests passed." << std::endl;
    return 0;
}
