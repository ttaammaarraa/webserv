#ifndef POSTDELETEHANDLER_HPP
#define POSTDELETEHANDLER_HPP

#include <string>
class HttpRequest;
struct Connection;
struct ServerConfig;

class PostDeleteHandler {
public:
	static std::string handlePost(Connection* conn, const HttpRequest& req, const ServerConfig& conf);
	static std::string handleDelete(Connection* conn, const HttpRequest& req, const ServerConfig& conf);
};

#endif
