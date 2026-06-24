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
	{
		return "";
	}
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

	std::string method = req.getMethod();
    const Location* loc = conn->serverConfig->matchLocationForRequest(req.getPath(), method);
    if (loc != NULL && !loc->allowed_methods.empty())
    {
        bool allowed = false;
        for (size_t i = 0; i < loc->allowed_methods.size(); ++i)
        {
            if (loc->allowed_methods[i] == method)
            {
                allowed = true;
                break;
            }
        }
        if (!allowed)
            return ResponseUtils::buildErrorRes(405, *conn->serverConfig);
    }
	bool isCgiRoute = (loc != NULL && !loc->cgi_pass.empty()) || CGIHandler::isCGI(req.getPath());
	if (isCgiRoute)
		return CGIHandler::handle(conn, req, *conn->serverConfig, loc ? loc->cgi_pass : std::string());
	if (method == "GET" || method == "HEAD")
		return GetHandler::handle(conn, req, *conn->serverConfig);
	else if (method == "POST")
		return PostDeleteHandler::handlePost(conn, req, *conn->serverConfig);
	else if (method == "DELETE")
		return PostDeleteHandler::handleDelete(conn, req, *conn->serverConfig);
	else
		return ResponseUtils::buildErrorRes(405, *conn->serverConfig);
}
// GET/POST/DELETE specific logic moved to handlers

bool ResponseBuilder::streamGetChunk(Connection* conn, int epoll_fd)
{
	(void)epoll_fd;
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
		conn->isStreaming = false;
		return true;
	}

	const size_t kChunkSize = 8192;
	size_t remaining = conn->file_size - conn->bytes_sent;
	size_t toRead = (remaining > kChunkSize) ? kChunkSize : remaining;

	char buffer[8192];
	ssize_t bytesRead = read(conn->file_fd, buffer, toRead);
	if (bytesRead < 0)
		return false;
	if (bytesRead == 0)
	{
		close(conn->file_fd);
		conn->file_fd = -1;
		conn->file_size = 0;
		conn->bytes_sent = 0;
		conn->isStreaming = false;
		return true;
	}

	ssize_t sent = send(conn->fd, buffer, static_cast<size_t>(bytesRead), 0);
	if (sent < 0)
	{
		// if (errno == EAGAIN || errno == EWOULDBLOCK)
		// {
		// 	struct epoll_event ev;
		// 	ev.events = EPOLLOUT;
		// 	ev.data.ptr = conn;
		// 	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
		// 	return true;
		// }
		return false;
	}

	conn->bytes_sent += static_cast<size_t>(sent);
	if (conn->bytes_sent >= conn->file_size)
	{
		close(conn->file_fd);
		conn->file_fd = -1;
		conn->file_size = 0;
		conn->bytes_sent = 0;
		conn->isStreaming = false;
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

std::string ResponseBuilder::buildErrorRes(int code, const ServerConfig& conf)
{
	return ResponseUtils::buildErrorRes(code, conf);
}

std::string ResponseBuilder::getMimeType(const std::string& path)
{
	return ResponseUtils::getMimeType(path);
}
