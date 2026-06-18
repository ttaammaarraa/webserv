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
    bool isComplete() const;
    int getUploadFd() const;
    size_t getContentLength() const;
    size_t getBodyReceived() const;
    size_t getBodyStart() const;
    bool isChunked() const;

    void setMethod(const std::string& method);
    void setPath(const std::string& path);
    void setVersion(const std::string& version);
    void setUploadFd(int fd);
    void setContentLength(size_t length);
    void setBodyReceived(size_t received);
    void setBodyStart(size_t start);
    void setChunked(bool chunked);
    void setComplete(bool complete);

private:
    std::string _method;
    std::string _path;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
    int _upload_fd;
    size_t _contentLength;
    size_t _bodyReceived;
    size_t _bodyStart;
    bool _chunked;
    bool _complete;
};

#endif 
