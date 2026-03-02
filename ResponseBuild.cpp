#include "ResponseBuild.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <iostream>

std::string ResponseBuild::handle(const HttpRequest& req, const ServerConfig &conf){
	if (req.path.find("..") != std::string::npos)
		return buildErrorRes(403, conf);
	
	std::string filepath = conf.root;

	if (!req.path.empty() && req.path[0] != '/')
		filepath += "/";

	filepath += req.path;

	struct stat st;
	if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
	{
		if (filepath[filepath.size() - 1] != '/')
			filepath += "/";
		filepath += "index.html";
	}
	if (stat(filepath.c_str(), &st) != 0)
		return (buildErrorRes(404, conf));

	if (!(st.st_mode & S_IROTH))
		return (buildErrorRes(403, conf));

	std::string body = readFile(filepath);
	if (body.empty())
		return (buildErrorRes(500, conf));
	std::string contentType = getMimeType(filepath);

	std::ostringstream oss;
	oss << "HTTP/1.1 200 OK\r\n";
	oss << "Content-Length: " << body.size() << "\r\n";
	oss << "Content-Type: " << contentType << "\r\n";
	oss << "\r\n";
	oss << body;

	return (oss.str());
}

std::string ResponseBuild::buildErrorRes(int code, const ServerConfig& conf)
{
	std::string body;
	std::string errorFile;

	if (conf.error_pages.count(code))
		errorFile = conf.error_pages.at(code);

	if (!errorFile.empty())
		body = readFile(errorFile);
	if (body.empty())
		body = "<html><body><h1>Error</h1></body></html>";
	std::ostringstream oss;

	if (code == 404)
		oss << "HTTP/1.1 404 Not Found\r\n";
	else if (code == 403)
		oss << "HTTP/1.1 403 Forbidden\r\n";
	else
		oss << "HTTP/1.1 500 Internal Server Error\r\n";
	
	oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Content-Type: text/html\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;

	return (oss.str());
}

std::string ResponseBuild::readFile(const std::string& path)
{
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
		return ("");
	std::ostringstream ss;
	ss << file.rdbuf();
	return (ss.str());
}

std::string ResponseBuild::getMimeType(const std::string& path)
{
	std::string ext;
	size_t dot = path.find_last_of('.');
	if (dot != std::string::npos)
		ext = path.substr(dot);
	if (ext == ".html")
		return ("text/html");
	if (ext == ".css")
		return ("text/css");
	if (ext == ".js")
		return ("application/javascript");
	if (ext == ".png")
		return ("image/png");
	if (ext == ".jpg" || ext == ".jpeg" )
		return ("image/jpeg");
	if (ext == ".gif")
		return ("image/gif");
	if (ext == ".ico")
		return ("image/x-ico");
	if (ext == ".txt")
		return ("text/plain");
	if (ext == ".pdf")
		return ("application/pdf");
	
	return ("application/octet-stream");
}
