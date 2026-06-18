#include "ResponseUtils.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <sys/stat.h>
#include <map>
#include "ServerConfig.hpp"
#include <sstream>

std::string ResponseUtils::joinPath(const std::string& base, const std::string& suffix)
{
    if (base.empty())
        return suffix;
    if (suffix.empty())
        return base;
    if (base[base.size() - 1] == '/' && suffix[0] == '/')
        return base + suffix.substr(1);
    if (base[base.size() - 1] != '/' && suffix[0] != '/')
        return base + "/" + suffix;
    return base + suffix;
}

std::string ResponseUtils::readFileDescriptor(const std::string& path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return "";

    std::string content;
    char buffer[4096];
    ssize_t bytesRead = 0;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
        content.append(buffer, static_cast<size_t>(bytesRead));
    close(fd);
    if (bytesRead < 0)
        return "";
    return content;
}

std::string ResponseUtils::getMimeType(const std::string& path)
{
    static std::map<std::string, std::string> mimeTypes;

    if (mimeTypes.empty())
    {
        mimeTypes[".html"] = "text/html";
        mimeTypes[".css"] = "text/css";
        mimeTypes[".js"] = "application/javascript";
        mimeTypes[".png"] = "image/png";
        mimeTypes[".jpg"] = "image/jpeg";
        mimeTypes[".jpeg"] = "image/jpeg";
        mimeTypes[".gif"] = "image/gif";
        mimeTypes[".ico"] = "image/x-icon";
        mimeTypes[".txt"] = "text/plain";
        mimeTypes[".pdf"] = "application/pdf";
    }

    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "application/octet-stream";

    std::string ext = path.substr(dot);
    std::map<std::string, std::string>::const_iterator it = mimeTypes.find(ext);
    if (it != mimeTypes.end())
        return it->second;
    return "application/octet-stream";
}

std::string ResponseUtils::buildErrorRes(int code, const ServerConfig& conf)
{
    std::string body;
    std::string errorFile;

    std::map<int, std::string>::const_iterator it = conf.error_pages.find(code);
    if (it != conf.error_pages.end())
        errorFile = it->second;

    if (!errorFile.empty())
        body = ResponseUtils::readFileDescriptor(errorFile);
    if (body.empty())
        body = "<html><body><h1>Error</h1></body></html>";
    std::ostringstream oss;

    if (code == 400)
        oss << "HTTP/1.1 400 Bad Request\r\n";
    else if (code == 403)
        oss << "HTTP/1.1 403 Forbidden\r\n";
    else if (code == 404)
        oss << "HTTP/1.1 404 Not Found\r\n";
    else if (code == 405)
        oss << "HTTP/1.1 405 Method Not Allowed\r\n";
    else if (code == 413)
        oss << "HTTP/1.1 413 Payload Too Large\r\n";
    else
        oss << "HTTP/1.1 500 Internal Server Error\r\n";

    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Content-Type: text/html\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;

    return oss.str();
}
