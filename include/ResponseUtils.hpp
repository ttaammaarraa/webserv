#ifndef RESPONSEUTILS_HPP
#define RESPONSEUTILS_HPP

#include <string>

struct ServerConfig;

class ResponseUtils {
public:
	static std::string joinPath(const std::string& base, const std::string& suffix);
	static std::string readFileDescriptor(const std::string& path);
	static std::string getMimeType(const std::string& path);
	static std::string buildErrorRes(int code, const ServerConfig& conf);
};

#endif
