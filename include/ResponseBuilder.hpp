#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <string>
#include <map>
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"

struct Connection;

class ResponseBuilder {
private:
	static std::string getMimeType(const std::string& path);
	static std::string buildErrorRes(int code, const ServerConfig& conf);
	static std::string handleGet(Connection* conn, const HttpRequest& req, const ServerConfig& conf);
	static std::string handlePost(Connection* conn, const HttpRequest& req, const ServerConfig& conf);
	static std::string handleDelete(Connection* conn, const HttpRequest& req, const ServerConfig& conf);


public:
	static std::string handle(Connection* conn, const HttpRequest& req);
	static bool streamGetChunk(Connection* conn, int epoll_fd);
};

#endif