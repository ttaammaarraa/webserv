#ifndef SERVER_HPP
#define SERVER_HPP


#include <netinet/in.h>
#include <map>
#include <string>
#include "ServerConfig.hpp"

class Server
{
    private:
        int _server_fd;
        int _port;
        int epoll_fd;
        sockaddr_in _address;
        std::map<int, std::string> _clientBuffers;
        ServerConfig config;
    public:
    Server(int port, const ServerConfig &config);
    ~Server();
        
        void init();
        void run();
};

#endif