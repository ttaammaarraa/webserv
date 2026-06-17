#include "ResponseBuilder.hpp"
#include <sstream>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <cerrno>
#include <fstream>

#include "Server.hpp"
#include "GetHandler.hpp"
#include "PostDeleteHandler.hpp"
#include "ResponseUtils.hpp"
#include "CGIHandler.hpp"

// Helper utilities moved to ResponseUtils

std::string ResponseBuilder::handle(Connection* conn, const HttpRequest& req)
{
	if (!conn || !conn->serverConfig)
		return "";

	// reset streaming state فقط للـ non-CGI
	if (conn->state != CGI_RUNNING)
	{
		if (conn->file_fd != -1)
		{
			close(conn->file_fd);
			conn->file_fd = -1;
		}
		conn->file_size = 0;
		conn->bytes_sent = 0;
	}

	std::string method = req.getMethod();

	if (method == "GET" || method == "HEAD")
		return GetHandler::handle(conn, req, *conn->serverConfig);

	else if (method == "POST")
		return PostDeleteHandler::handlePost(conn, req, *conn->serverConfig);

	else if (method == "DELETE")
		return PostDeleteHandler::handleDelete(conn, req, *conn->serverConfig);

	return ResponseUtils::buildErrorRes(405, *conn->serverConfig);
}// GET/POST/DELETE specific logic moved to handlers

bool ResponseBuilder::streamGetChunk(Connection* conn, int epoll_fd)
{
	if (!conn)
		return false;
	if (conn->file_fd == -1)
		return true;
	if (conn->bytes_sent >= conn->file_size)
	{
		close(conn->file_fd);
		conn->file_fd = -1;
		conn->file_size = 0;
		conn->bytes_sent = 0;
		return true;
	}

	const size_t kChunkSize = 8192;
	size_t remaining = conn->file_size - conn->bytes_sent;
	size_t toRead = (remaining > kChunkSize) ? kChunkSize : remaining;

	char buffer[8192];
	ssize_t bytesRead = pread(conn->file_fd, buffer, toRead, static_cast<off_t>(conn->bytes_sent));
	if (bytesRead < 0)
		return false;
	if (bytesRead == 0)
	{
		close(conn->file_fd);
		conn->file_fd = -1;
		conn->file_size = 0;
		conn->bytes_sent = 0;
		return true;
	}

	ssize_t sent = send(conn->fd, buffer, static_cast<size_t>(bytesRead), 0);
	if (sent < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			struct epoll_event ev;
			ev.events = EPOLLOUT;
			ev.data.ptr = conn;
			epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
			return true;
		}
		return false;
	}

	conn->bytes_sent += static_cast<size_t>(sent);
	if (conn->bytes_sent >= conn->file_size)
	{
		close(conn->file_fd);
		conn->file_fd = -1;
		conn->file_size = 0;
		conn->bytes_sent = 0;
	}

	return true;
}

std::string ResponseBuilder::handlePost(Connection* conn, const HttpRequest& req, const ServerConfig& conf)
{
	(void)conn;
	(void)req;
	(void)conf;
	return std::string();
}

// ========== ORIGINAL ABEER'S HANDLE METHOD (preserved for reference) ==========
/*
std::string ResponseBuilder::handle(Connection* conn, const HttpRequest& req)
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

	const Location* matchedLocation = conf.matchLocation(req.getPath());
	std::string effectiveRoot = conf.root;
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

	std::string filepath = joinPath(effectiveRoot, suffixPath);

	struct stat st;
	if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
	{
		std::string directoryPath = filepath;
		std::string indexPath = joinPath(directoryPath, effectiveIndex);
		if (stat(indexPath.c_str(), &st) == 0)
			filepath = indexPath;
		else if (autoindexEnabled)
		{
			std::string body = AutoIndexGenerator::generate(directoryPath, req.getPath());
			if (body.empty())
				return buildErrorRes((errno == EACCES) ? 403 : 500, conf);

			std::ostringstream oss;
			oss << "HTTP/1.1 200 OK\\r\\n";
			oss << "Content-Length: " << body.size() << "\\r\\n";
			oss << "Content-Type: text/html\\r\\n";
			oss << "Connection: close\\r\\n";
			oss << "\\r\\n";
			oss << body;
			return oss.str();
		}
		else
			return buildErrorRes(403, conf);
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
	oss << "HTTP/1.1 200 OK\\r\\n";
	oss << "Content-Length: " << static_cast<size_t>(st.st_size) << "\\r\\n";
	oss << "Content-Type: " << getMimeType(filepath) << "\\r\\n";
	oss << "Accept-Ranges: bytes\\r\\n";
	oss << "Connection: close\\r\\n";
	oss << "\\r\\n";
	return oss.str();
}
*/
// ========== END ORIGINAL HANDLE METHOD ==========

std::string ResponseBuilder::buildErrorRes(int code, const ServerConfig& conf)
{
	return ResponseUtils::buildErrorRes(code, conf);
}

std::string ResponseBuilder::getMimeType(const std::string& path)
{
	return ResponseUtils::getMimeType(path);
}
