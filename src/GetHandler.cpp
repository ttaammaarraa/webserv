#include "GetHandler.hpp"
#include "ResponseUtils.hpp"
#include "Server.hpp"
#include "AutoIndexGenerator.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

std::string GetHandler::handle(Connection* conn, const HttpRequest& req, const ServerConfig& conf)
{
    if (req.getPath().find("..") != std::string::npos)
        return ResponseUtils::buildErrorRes(403, conf);

    const Location* matchedLocation = conf.matchLocation(req.getPath());
    if (matchedLocation != NULL && !matchedLocation->redirect.empty())
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 301 Moved Permanently\r\n";
        oss << "Location: " << matchedLocation->redirect << "\r\n";
        oss << "Content-Length: 0\r\n";
        oss << "Connection: close\r\n";
        oss << "\r\n";
        return oss.str();
    }
    std::string effectiveRoot = (matchedLocation && !matchedLocation->root.empty()) ? matchedLocation->root : conf.root;
    std::string fullPath = ResponseUtils::joinPath(effectiveRoot, req.getPath());

    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && !req.getPath().empty() && req.getPath()[req.getPath().size() - 1] != '/') {
        std::ostringstream redirect;
        redirect << "HTTP/1.1 301 Moved Permanently\r\n"
                 << "Location: " << req.getPath() << "/\r\n"
                 << "Connection: close\r\n\r\n";
        return redirect.str();
    }

    std::string effectiveIndex = "index.html";
    bool autoindexEnabled = false;
    std::string suffixPath = req.getPath();

    if (matchedLocation != NULL)
    {
        if (!matchedLocation->root.empty())
            effectiveRoot = matchedLocation->root;
        if (!matchedLocation->index.empty())
            effectiveIndex = matchedLocation->index;
        autoindexEnabled = matchedLocation->autoindex;

        if (!matchedLocation->root.empty() && !matchedLocation->path.empty() && matchedLocation->path != "/"
            && suffixPath.compare(0, matchedLocation->path.size(), matchedLocation->path) == 0)
        {
            suffixPath = suffixPath.substr(matchedLocation->path.size());
            if (suffixPath.empty())
                suffixPath = "/";
        }
    }

    std::string filepath = ResponseUtils::joinPath(effectiveRoot, suffixPath);
if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
        std::string directoryPath = filepath;
        std::string indexPath = ResponseUtils::joinPath(directoryPath, effectiveIndex);

        if (stat(indexPath.c_str(), &st) == 0)
            filepath = indexPath;
        else if (autoindexEnabled)
        {
            std::string body = AutoIndexGenerator::generate(directoryPath, req.getPath());
            if (body.empty())
                return ResponseUtils::buildErrorRes(500, conf);

            std::ostringstream oss;
            oss << "HTTP/1.1 200 OK\r\n";
            oss << "Content-Length: " << body.size() << "\r\n";
            oss << "Content-Type: text/html\r\n";
            oss << "Connection: close\r\n";
            oss << "\r\n";
            oss << body;
            return oss.str();
        }
        else
        {
            return ResponseUtils::buildErrorRes(404, conf);
        }
    }

    if (stat(filepath.c_str(), &st) != 0)
        return ResponseUtils::buildErrorRes(404, conf);
    if (!(st.st_mode & S_IROTH))
        return ResponseUtils::buildErrorRes(403, conf);

    int file_fd = open(filepath.c_str(), O_RDONLY);
    if (file_fd < 0)
        return ResponseUtils::buildErrorRes(404, conf);

    if (fstat(file_fd, &st) != 0)
    {
        close(file_fd);
        return ResponseUtils::buildErrorRes(500, conf);
    }

    if (conn)
    {
        conn->file_fd = file_fd;
        conn->file_size = static_cast<size_t>(st.st_size);
        conn->bytes_sent = 0;
        conn->isStreaming = true;
    }
    else
    {
        close(file_fd);
        return ResponseUtils::buildErrorRes(500, conf);
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Length: " << static_cast<size_t>(st.st_size) << "\r\n";
    oss << "Content-Type: " << ResponseUtils::getMimeType(filepath) << "\r\n";
    oss << "Accept-Ranges: bytes\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    return oss.str();
}
