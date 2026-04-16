#include "../HttpRequest.hpp"
#include <cassert>
#include <iostream>

static void test_basic_exact_length() {
    std::string raw = "POST /submit HTTP/1.1\r\nHost: localhost\r\nContent-Length: 11\r\n\r\nhello world";
    HttpRequest req = HttpRequest::parse(raw);
    assert(req.getMethod() == "POST");
    assert(req.getPath() == "/submit");
    assert(req.getVersion() == "HTTP/1.1");
    assert(req.getHeaders().find("Content-Length") != req.getHeaders().end());
    assert(req.getHeaders().find("Content-Length")->second == "11");
    assert(req.getBody() == "hello world");
}

static void test_truncate_to_content_length() {
    std::string raw = "POST /upload HTTP/1.1\r\nContent-Length: 5\r\n\r\nabcdefg";
    HttpRequest req = HttpRequest::parse(raw);
    assert(req.getBody() == "abcde");
}

static void test_short_incomplete_body() {
    std::string raw = "POST /upload HTTP/1.1\r\nContent-Length: 10\r\n\r\nabc";
    HttpRequest req = HttpRequest::parse(raw);
    assert(req.getBody() == "abc");
}

static void test_missing_content_length() {
    std::string raw = "POST /upload HTTP/1.1\r\nHost: localhost\r\n\r\nabc";
    HttpRequest req = HttpRequest::parse(raw);
    assert(req.getBody().empty());
}

static void test_invalid_content_length() {
    std::string raw = "POST /upload HTTP/1.1\r\nContent-Length: xyz\r\n\r\nabc";
    HttpRequest req = HttpRequest::parse(raw);
    assert(req.getBody().empty());
}

int main() {
    test_basic_exact_length();
    test_truncate_to_content_length();
    test_short_incomplete_body();
    test_missing_content_length();
    test_invalid_content_length();

    std::cout << "All HttpRequest body parsing tests passed." << std::endl;
    return 0;
}
