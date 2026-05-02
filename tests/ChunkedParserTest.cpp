#include "ChunkedBodyParser.hpp"
#include <cassert>
#include <iostream>
#include <string>

static void test_complete_chunked_body()
{
    ChunkedBodyParser p;
    size_t pos = 0;
    std::string body;

    const std::string raw = "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n";
    bool done = p.parse(raw, pos, body);

    assert(done);
    assert(!p.hasError());
    assert(p.isDone());
    assert(body == "Wikipedia");
    assert(p.totalSize() == 9);
    assert(pos == raw.size());
}

static void test_split_buffer_parsing()
{
    ChunkedBodyParser p;
    std::string body;
    size_t pos = 0;

    const std::string first = "4\r\nWi";
    bool done = p.parse(first, pos, body);
    assert(!done);
    assert(!p.hasError());
    assert(body == "Wi");
    assert(p.totalSize() == 2);
    assert(pos == first.size());

    pos = 0;
    const std::string second = "ki\r\n5\r\npedia\r\n0\r\n\r\n";
    done = p.parse(second, pos, body);

    assert(done);
    assert(!p.hasError());
    assert(p.isDone());
    assert(body == "Wikipedia");
    assert(p.totalSize() == 9);
    assert(pos == second.size());
}

static void test_chunk_extensions()
{
    ChunkedBodyParser p;
    size_t pos = 0;
    std::string body;

    const std::string raw = "4;name=value\r\nWiki\r\n0\r\n\r\n";
    bool done = p.parse(raw, pos, body);

    assert(done);
    assert(!p.hasError());
    assert(body == "Wiki");
    assert(p.totalSize() == 4);
}

static void test_malformed_chunk_size()
{
    ChunkedBodyParser p;
    size_t pos = 0;
    std::string body;

    const std::string raw = "G\r\nABC\r\n0\r\n\r\n";
    bool done = p.parse(raw, pos, body);

    assert(done);
    assert(p.hasError());
    assert(body.empty());
}

int main()
{
    test_complete_chunked_body();
    test_split_buffer_parsing();
    test_chunk_extensions();
    test_malformed_chunk_size();

    std::cout << "All ChunkedBodyParser unit tests passed." << std::endl;

    return 0;
}
