#include "ResponseBuilder.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

void ResponseBuild::sendResponse(int client_fd ,const HttpRequest& req, const ServerConfig &conf){
	if (req.path.find("..") != std::string::npos)
	{
		std::string err = buildErrorRes(403, conf);
		send(client_fd, err.c_str(), err.size(), 0);
		return ;
	}
	std::string filepath = conf.root;

	if (!req.path.empty() && req.path[0] != '/')
		filepath += "/";

	filepath += req.path;

	struct stat st;
		//  إذا directory → index.html
	if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
	{
		if (filepath[filepath.size() - 1] != '/')
			filepath += "/";
		filepath += "index.html";
	}
		// file مش موجود
	if (stat(filepath.c_str(), &st) != 0)
	{
		std::string err = buildErrorRes(404, conf);
		send(client_fd, err.c_str(), err.size(), 0);
		return ;
	}
		// ما عنده permission
	if (!(st.st_mode & S_IROTH))
	{
		std::string err = buildErrorRes(403, conf);
		send(client_fd, err.c_str(), err.size(), 0);
		return;
	}

/*
	if (filepath.find(".py") != std::string::npos)
	{
	 handle CGI
	}
*/
	int fd = open(filepath.c_str(), O_RDONLY);
	if (fd < 0)
	{
		std::string err = buildErrorRes(500, conf);
		send(client_fd, err.c_str(), err.size(), 0);
		return;
	}

	std::ostringstream oss;
	oss << "HTTP/1.1 200 OK\r\n";
	oss << "Content-Length: " << st.st_size << "\r\n";
	oss << "Content-Type: " << getMimeType(filepath) << "\r\n";
	oss << "Connection: close\r\n";
	oss << "\r\n";

	std::string header = oss.str();
	ssize_t sent = 0;
	while (sent < (ssize_t)header.size())
	{
		ssize_t n = send(client_fd, header.c_str() + sent, header.size() - sent, 0);
		if (n <= 0)
			break;
		sent += n;
	}
	char buff[8192];
	ssize_t byt;

	while ((byt = read(fd, buff, sizeof(buff))) > 0)
	{
		ssize_t sent = 0;
		while (sent < byt)
		{
			ssize_t n = send(client_fd, buff + sent, byt - sent, 0);
			if (n <= 0)
				break;
			sent += n;
		}
	}
	if (byt < 0)
	{
		//perror("read error");
		close(fd);
		return;
	}
	close(fd);
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
const std::map<std::string, std::string> ResponseBuild::mimeTypes =
{
	{".html", "text/html"},
	{".css", "text/css"},
	{".js", "application/javascript"},
	{".png", "image/png"},
	{".jpg", "image/jpeg"},
	{".jpeg", "image/jpeg"},
	{".gif", "image/gif"},
	{".ico", "image/x-ico"},
	{".txt", "text/plain"},
	{".pdf", "application/pdf"},
};

std::string ResponseBuild::getMimeType(const std::string& path)
{
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos)
		return ("application/octet-stream");
	std::string ext = path.substr(dot);
	std::map<std::string, std::string>::const_iterator it = mimeTypes.find(ext);
	if (it != mimeTypes.end())
		return it->second;
	return ("application/octet-stream");
}
