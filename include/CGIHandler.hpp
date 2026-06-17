#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>

class HttpRequest;
struct Connection;
struct ServerConfig;

class CGIHandler {
public:
	static bool isCGI(const std::string& path);

	static std::string handle(Connection *conn,
                           const HttpRequest &req,
                           const ServerConfig &conf,
                           int epoll_fd);
    static std::string buildResponseFromCGI(const std::string &output);

};

#endif