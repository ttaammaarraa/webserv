#include "PostDeleteHandler.hpp"
#include "ResponseUtils.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

std::string PostDeleteHandler::handlePost(Connection* conn, const HttpRequest& req, const ServerConfig& conf)
{
    // بما أن Server.cpp قام بمعالجة الرفع (Streaming) بالفعل وفتح الملف،
    // كل ما نحتاجه هنا هو إرجاع رد 201 Created لإعلام التستر بأن العملية تمت بنجاح.
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
    // الحماية من الـ Path Traversal
    if (req.getPath().find("..") != std::string::npos)
        return ResponseUtils::buildErrorRes(403, conf);

    const Location* matchedLocation = conf.matchLocation(req.getPath());
    std::string filepath;
    std::string suffixPath = req.getPath();

    // استخراج المسار الصحيح بناءً على الـ Location
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

    // منع حذف المجلدات
    if (S_ISDIR(st.st_mode))
        return ResponseUtils::buildErrorRes(403, conf);

    // التحقق من صلاحيات القراءة
    if (!(st.st_mode & S_IROTH))
        return ResponseUtils::buildErrorRes(403, conf);

    // تنفيذ الحذف
    if (unlink(filepath.c_str()) != 0)
        return ResponseUtils::buildErrorRes((errno == EACCES) ? 403 : 500, conf);

    std::ostringstream oss;
    oss << "HTTP/1.1 204 No Content\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    return oss.str();
}