#include <vector>
#include <iostream>
#include <csignal>
#include "ServerConfig.hpp"
#include "ConfigParser.hpp"
#include "Server.hpp"

extern volatile sig_atomic_t g_keepRunning;

void signalHandler(int signum)
{
    (void)signum;
    g_keepRunning = 0;
}

int main(int argc, char **argv) 
{
    try 
    {
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

        std::vector<ServerConfig> configs = ConfigParser::parse(configFile);
        if (configs.empty())
            throw std::runtime_error("No server blocks were parsed from the configuration file");

        Server server(configs);
        server.init();

        std::signal(SIGINT, signalHandler);
        server.run();
        server.stop();

    }
    catch (const std::exception& e)
    {
        std::cerr << "Configuration Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
