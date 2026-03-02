#include <vector>
#include <iostream>
#include <cassert>
#include "ServerConfig.hpp"
#include "ConfigParser.hpp"
#include "HttpRequest.hpp"
#include "Server.hpp"

int main(int argc, char **argv) 
{
    try 
    {
        //Production
        std::string configFile;
        if (argc == 2)
            configFile = argv[1];
        else if (argc == 1) 
            configFile = "default.conf";
        else 
        {
            std::cerr << "Error: Usage: " << argv[0] << " [config_file]" << std::endl;
            return 1;
        }

        ServerConfig config = ConfigParser::parse(configFile);
        std::cout << "Port: " << config.port << std::endl;
        std::cout << "Host: " << config.host << std::endl;
        std::cout << "Root: " << config.root << std::endl;
        for (std::map<int, std::string>::iterator it = config.error_pages.begin(); it != config.error_pages.end(); ++it) 
            std::cout << "Error page " << it->first << ": " << it->second << std::endl;

        // Start server with config
        Server myServer(config);
        myServer.init();
        myServer.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Configuration Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}