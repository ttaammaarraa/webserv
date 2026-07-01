#include "PostDeleteHandler.hpp"
#include "ResponseUtils.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>

std::string PostDeleteHandler::handlePost(Connection* conn, const HttpRequest& req, const ServerConfig& conf)
{
    (void)conn;
    (void)req;
    (void)conf;

    std::ostringstream oss;
    oss << "HTTP/1.1 201 Created\r\n";
    oss << "Content-Length: 0\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    return oss.str();
}

std::string PostDeleteHandler::handleDelete(Connection* conn, const HttpRequest& req, const ServerConfig& conf)
{
    (void)conn;
    if (req.getPath().find("..") != std::string::npos)
        return ResponseUtils::buildErrorRes(403, conf);

    const Location* matchedLocation = conf.matchLocation(req.getPath());
    std::string filepath;
    std::string suffixPath = req.getPath();

    if (matchedLocation != NULL && !matchedLocation->path.empty() && matchedLocation->path != "/"
        && suffixPath.compare(0, matchedLocation->path.size(), matchedLocation->path) == 0)
    {
        suffixPath = suffixPath.substr(matchedLocation->path.size());
        if (suffixPath.empty()) suffixPath = "/";
    }

    std::string effectiveRoot = (matchedLocation != NULL && !matchedLocation->root.empty()) ? matchedLocation->root : conf.root;
    filepath = ResponseUtils::joinPath(effectiveRoot, req.getPath());

    struct stat st;
    if (stat(filepath.c_str(), &st) != 0)
        return ResponseUtils::buildErrorRes(404, conf);

    if (S_ISDIR(st.st_mode))
        return ResponseUtils::buildErrorRes(403, conf);

    if (!(st.st_mode & S_IROTH))
        return ResponseUtils::buildErrorRes(403, conf);

    if (unlink(filepath.c_str()) != 0)
        return ResponseUtils::buildErrorRes(403, conf);

    std::ostringstream oss;
    oss << "HTTP/1.1 204 No Content\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    return oss.str();
}
