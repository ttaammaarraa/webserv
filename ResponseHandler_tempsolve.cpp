#include "ResponseHandler_tempsolve.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <cerrno>

ResponseHandler_tempsolve::ResponseHandler_tempsolve() {}
ResponseHandler_tempsolve::~ResponseHandler_tempsolve() {}

// Return 0 if OK, 403 if forbidden, 404 if not found
int ResponseHandler_tempsolve::validateFile(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0)
        return 404;
    if (!(buffer.st_mode & S_IRUSR))
        return 403;
    if (!S_ISREG(buffer.st_mode))
        return 403;
    return 0;
}

bool ResponseHandler_tempsolve::readFile(const std::string& path, std::vector<unsigned char>& buffer) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return false; }
    buffer.resize(st.st_size);
    ssize_t total = 0;
    while (total < st.st_size) {
        ssize_t r = read(fd, &buffer[total], st.st_size - total);
        if (r <= 0) { close(fd); return false; }
        total += r;
    }
    close(fd);
    return true;
}

std::string ResponseHandler_tempsolve::getErrorPage(int code) {
    if (code == 404)
        return "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>";
    if (code == 403)
        return "<html><head><title>403 Forbidden</title></head><body><h1>403 Forbidden</h1></body></html>";
    return "<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>";
}

std::string ResponseHandler_tempsolve::getMimeType(const std::string& path) {
    static std::map<std::string, std::string> mimeTypes;
    if (mimeTypes.empty()) {
        mimeTypes[".html"] = "text/html";
        mimeTypes[".htm"] = "text/html";
        mimeTypes[".jpg"] = "image/jpeg";
        mimeTypes[".jpeg"] = "image/jpeg";
        mimeTypes[".png"] = "image/png";
        mimeTypes[".gif"] = "image/gif";
        mimeTypes[".css"] = "text/css";
        mimeTypes[".js"] = "application/javascript";
        mimeTypes[".txt"] = "text/plain";
    }
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(dot);
    if (mimeTypes.count(ext)) return mimeTypes[ext];
    return "application/octet-stream";
}

std::vector<unsigned char> ResponseHandler_tempsolve::constructHttpResponse(const std::string& filePath) {
    std::vector<unsigned char> response;
    std::vector<unsigned char> body;
    std::string statusLine;
    std::string contentType;
    int code = validateFile(filePath);
    if (code == 0 && readFile(filePath, body)) {
        statusLine = "HTTP/1.1 200 OK\r\n";
        contentType = getMimeType(filePath);
    } else {
        if (code == 0) code = 500;
        statusLine = (code == 404) ? "HTTP/1.1 404 Not Found\r\n" : (code == 403) ? "HTTP/1.1 403 Forbidden\r\n" : "HTTP/1.1 500 Internal Server Error\r\n";
        contentType = "text/html";
        std::string errPage = getErrorPage(code);
        body.assign(errPage.begin(), errPage.end());
    }
    std::ostringstream headers;
    headers << statusLine;
    headers << "Content-Type: " << contentType << "\r\n";
    headers << "Content-Length: " << body.size() << "\r\n";
    headers << "Connection: close\r\n";
    headers << "\r\n";
    std::string headerStr = headers.str();
    response.insert(response.end(), headerStr.begin(), headerStr.end());
    response.insert(response.end(), body.begin(), body.end());
    return response;
}

bool ResponseHandler_tempsolve::sanityCheck(const std::string& filePath) {
    struct stat buffer;
    return (stat(filePath.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
}
