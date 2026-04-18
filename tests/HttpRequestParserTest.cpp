#include "../HttpRequest.hpp"

#include <cassert>
#include <iostream>

static void test_basic_get()
{
    std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nUser-Agent: test-agent\r\n\r\n";
    HttpRequest req = HttpRequest::parse(raw);

    assert(req.getMethod() == "GET");
    assert(req.getPath() == "/index.html");
    assert(req.getVersion() == "HTTP/1.1");
    assert(req.getHeaders().find("Host") != req.getHeaders().end());
    assert(req.getHeaders().find("User-Agent") != req.getHeaders().end());
    assert(req.getHeaders().find("Host")->second == "localhost");
    assert(req.getHeaders().find("User-Agent")->second == "test-agent");
}

static void test_header_whitespace()
{
    std::string raw = "POST /submit HTTP/1.0\r\nContent-Type:   text/plain  \r\nX-Test:foo\r\n\r\n";
    HttpRequest req = HttpRequest::parse(raw);

    assert(req.getMethod() == "POST");
    assert(req.getPath() == "/submit");
    assert(req.getVersion() == "HTTP/1.0");
    assert(req.getHeaders().find("Content-Type") != req.getHeaders().end());
    assert(req.getHeaders().find("X-Test") != req.getHeaders().end());
    assert(req.getHeaders().find("Content-Type")->second == "text/plain");
    assert(req.getHeaders().find("X-Test")->second == "foo");
}

static void test_transfer_encoding_without_content_length()
{
    std::string raw = "POST /u HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
                      "4\r\nWiki\r\n"
                      "5\r\npedia\r\n"
                      "0\r\n\r\n";
    HttpRequest req = HttpRequest::parse(raw);

    assert(req.getMethod() == "POST");
    assert(req.getPath() == "/u");
    assert(req.getBody().empty());
}

int main()
{
    test_basic_get();
    test_header_whitespace();
    test_transfer_encoding_without_content_length();

    std::cout << "All HttpRequest parser tests passed." << std::endl;
    return 0;
}
