#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <string>
#include <map>
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"

class ResponseBuild {
private:
	static std::string getMimeType(const std::string& path);
	static std::string readFile(const std::string& path);
	static std::string buildErrorRes(int code, const ServerConfig& conf); 

public:
	static std::string handle(const HttpRequest& req, const ServerConfig &conf);
	static void sendResponse(int client_fd ,const HttpRequest& req, const ServerConfig &conf);
};

#endif
