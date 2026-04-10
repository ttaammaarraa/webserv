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
	static const std::map<std::string, std::string> mimeTypes;

public:
	static void sendResponse(int client_fd ,const HttpRequest& req, const ServerConfig &conf);
};

#endif