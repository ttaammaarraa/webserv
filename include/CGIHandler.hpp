#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>

class HttpRequest;
struct Connection;
struct ServerConfig;

class CGIHandler {
public:
	static bool isCGI(const std::string& path);
	static std::string buildResponseFromCGI(const std::string& output);

	static std::string handle(
		Connection* conn,
		const HttpRequest& req,
		const ServerConfig& conf,
		const std::string& cgiPass = std::string()
	);
};

#endif
