#include "PostDeleteHandler.hpp"
#include "ResponseUtils.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>
#include <cerrno>

std::string PostDeleteHandler::handlePost(Connection* conn, const HttpRequest& req, const ServerConfig& conf)
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
        if (suffixPath.empty())
            suffixPath = "/";
    }

    if (matchedLocation != NULL && !matchedLocation->upload_path.empty())
    {
        filepath = ResponseUtils::joinPath(matchedLocation->upload_path, suffixPath);
    }
    else
    {
        std::string effectiveRoot = (matchedLocation != NULL && !matchedLocation->root.empty()) 
            ? matchedLocation->root 
            : conf.root;
        filepath = ResponseUtils::joinPath(effectiveRoot, req.getPath());
    }

    struct stat st;
    if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
        if (filepath[filepath.size() - 1] != '/')
            filepath += "/";
        filepath += "upload.bin";
    }

    std::ofstream file(filepath.c_str(), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return ResponseUtils::buildErrorRes(403, conf);

    file.write(req.getBody().c_str(), static_cast<std::streamsize>(req.getBody().size()));
    if (file.fail())
    {
        file.close();
        return ResponseUtils::buildErrorRes(500, conf);
    }
    file.close();

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
        if (suffixPath.empty())
            suffixPath = "/";
    }

    std::string candidate1;
    std::string candidate2;
    std::string candidate3;

    if (matchedLocation != NULL && !matchedLocation->upload_path.empty())
        candidate1 = ResponseUtils::joinPath(matchedLocation->upload_path, suffixPath);

    std::string effectiveRoot = (matchedLocation != NULL && !matchedLocation->root.empty()) ? matchedLocation->root : conf.root;
    candidate2 = ResponseUtils::joinPath(effectiveRoot, req.getPath());
    candidate3 = ResponseUtils::joinPath(effectiveRoot, suffixPath);

    struct stat st;

    if (!candidate1.empty() && stat(candidate1.c_str(), &st) == 0)
        filepath = candidate1;
    else if (stat(candidate2.c_str(), &st) == 0)
        filepath = candidate2;
    else if (stat(candidate3.c_str(), &st) == 0)
        filepath = candidate3;
    else
        return ResponseUtils::buildErrorRes(404, conf);

    if (S_ISDIR(st.st_mode))
        return ResponseUtils::buildErrorRes(403, conf);

    if (!(st.st_mode & S_IROTH))
        return ResponseUtils::buildErrorRes(403, conf);

    if (unlink(filepath.c_str()) != 0)
        return ResponseUtils::buildErrorRes((errno == EACCES) ? 403 : 500, conf);

    std::ostringstream oss;
    oss << "HTTP/1.1 204 No Content\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    return oss.str();
}
