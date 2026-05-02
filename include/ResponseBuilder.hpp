#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <string>
#include <map>
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"

struct Connection;

class ResponseBuild {
private:
	static std::string getMimeType(const std::string& path);
	static std::string buildErrorRes(int code, const ServerConfig& conf); 

public:
	static std::string handle(Connection* conn, const HttpRequest& req);
};

#endif