#ifndef SERVER_HPP
#define SERVER_HPP



#include <netinet/in.h>

#include <map>
#include <string>
#include "ServerConfig.hpp"



// Integration-ready Server class
class Server {
    private:
        int _server_fd;
        int _port;
        sockaddr_in _address;
        std::map<int, std::string> _client_buffers;
        // Store config for use in run()
        ServerConfig _config;

    public:
        Server(const ServerConfig& config);
        ~Server();

        void init();      
        void run();
};

#endif