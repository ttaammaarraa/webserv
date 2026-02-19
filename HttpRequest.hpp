#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;

    HttpRequest();
    static HttpRequest parse(const std::string& raw_request);
};

#endif 
