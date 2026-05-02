#include "ResponseBuilder.hpp"
#include <sstream>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cerrno>

#include "Server.hpp"

static std::string readFileDescriptor(const std::string& path)
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

std::string ResponseBuild::handle(Connection* conn, const HttpRequest& req)
{
	if (!conn || !conn->serverConfig)
		return "";

	const ServerConfig& conf = *conn->serverConfig;

	if (conn)
	{
		if (conn->file_fd != -1)
		{
			close(conn->file_fd);
			conn->file_fd = -1;
		}
		conn->file_size = 0;
		conn->bytes_sent = 0;
		conn->isStreaming = false;
	}

	if (req.getPath().find("..") != std::string::npos)
		return buildErrorRes(403, conf);

	std::string filepath = conf.root;
	if (!req.getPath().empty() && req.getPath()[0] != '/')
		filepath += "/";
	filepath += req.getPath();

	struct stat st;
	if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
	{
		if (filepath[filepath.size() - 1] != '/')
			filepath += "/";
		filepath += "index.html";
	}
	if (stat(filepath.c_str(), &st) != 0)
		return buildErrorRes(404, conf);
	if (!(st.st_mode & S_IROTH))
		return buildErrorRes(403, conf);

	int file_fd = open(filepath.c_str(), O_RDONLY);
	if (file_fd < 0)
		return buildErrorRes((errno == EACCES) ? 403 : 404, conf);

	if (fstat(file_fd, &st) != 0)
	{
		close(file_fd);
		return buildErrorRes(500, conf);
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
		return buildErrorRes(500, conf);
	}

	std::ostringstream oss;
	oss << "HTTP/1.1 200 OK\r\n";
	oss << "Content-Length: " << static_cast<size_t>(st.st_size) << "\r\n";
	oss << "Content-Type: " << getMimeType(filepath) << "\r\n";
	oss << "Accept-Ranges: bytes\r\n";
	oss << "Connection: close\r\n";
	oss << "\r\n";
	return oss.str();
}

std::string ResponseBuild::buildErrorRes(int code, const ServerConfig& conf)
{
	std::string body;
	std::string errorFile;

	std::map<int, std::string>::const_iterator it = conf.error_pages.find(code);
	if (it != conf.error_pages.end())
		errorFile = it->second;

	if (!errorFile.empty())
		body = readFileDescriptor(errorFile);
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

std::string ResponseBuild::getMimeType(const std::string& path)
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
