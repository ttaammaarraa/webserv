#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "ServerConfig.hpp"
#include <string>

class ConfigParser 
{
        private:
                static void trim(std::string& s);
        public:
                static ServerConfig parse(const std::string& filename);
}; 

#endif // CONFIG_PARSER_HPP
