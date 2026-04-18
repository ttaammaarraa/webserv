#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "ServerConfig.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

class ConfigParser 
{
        private:
                static void trim(std::string& s);
                typedef void (ConfigParser::*HandlerFunc)(ServerConfig&, std::istringstream&, int);
                static void handlePort(ServerConfig& config, std::istringstream& iss, int line_number);
                static void handleHost(ServerConfig& config, std::istringstream& iss, int line_number);
                static void handleServerName(ServerConfig& config, std::istringstream& iss, int line_number);
                static void handleRoot(ServerConfig& config, std::istringstream& iss, int line_number);
                static void handleErrorPages(ServerConfig& config, std::istringstream& iss, int line_number);
                static Location parseLocation(std::ifstream& file, int& line_number);
                static void applyDirective(ServerConfig& config, const std::string& key, std::istringstream& iss, int line_number);
                static ServerConfig parseServerBlock(std::ifstream& file, int& line_number);
        public:
                static std::vector<ServerConfig> parse(const std::string& filename);
}; 

#endif
