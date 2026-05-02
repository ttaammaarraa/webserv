#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest {
public:
    HttpRequest();
    static HttpRequest parse(const std::string& raw_request);

    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getVersion() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::string& getBody() const;

    void setMethod(const std::string& method);
    void setPath(const std::string& path);
    void setVersion(const std::string& version);

private:
    std::string _method;
    std::string _path;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
};

#endif 
