#ifndef GETHANDLER_HPP
#define GETHANDLER_HPP

#include <string>
class HttpRequest;
struct Connection;
struct ServerConfig;

class GetHandler {
public:
	static std::string handle(Connection* conn, const HttpRequest& req, const ServerConfig& conf);
};

#endif
