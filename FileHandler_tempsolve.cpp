#include "FileHandler_tempsolve.hpp"
#include <sys/stat.h>
#include <fstream>
#include <sstream>

bool FileHandler_tempsolve::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
}

bool FileHandler_tempsolve::readFile(const std::string& path, std::vector<unsigned char>& buffer) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    buffer.resize(size);
    if (!file.read(reinterpret_cast<char*>(&buffer[0]), size)) return false;
    return true;
}

std::string FileHandler_tempsolve::get404Page() {
    return "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>";
}

std::string FileHandler_tempsolve::getMimeType(const std::string& path) {
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


std::string FileHandler_tempsolve::constructHttpResponse(const std::string& filePath) {
    std::ostringstream response;
    std::vector<unsigned char> buffer;
    std::string statusLine;
    std::string body;
    std::string contentType;
    if (fileExists(filePath) && readFile(filePath, buffer)) {
        statusLine = "HTTP/1.1 200 OK\r\n";
        contentType = getMimeType(filePath);
        body.assign(buffer.begin(), buffer.end());
    } else {
        statusLine = "HTTP/1.1 404 Not Found\r\n";
        contentType = "text/html";
        body = get404Page();
    }
    response << statusLine;
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    return response.str();
}
