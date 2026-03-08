#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <map>
#include <string>
#include "ServerConfig.hpp"

struct Connection
{
    int fd;
    bool isServer;
};

class Server
{
    private:
        int _server_fd;
        int _port;
        int epoll_fd;
        sockaddr_in _address;
        std::map<int, std::string> _clientBuffers;
        ServerConfig config;
        Connection* _serverConn;

        void cleanup_connection(Connection* conn);
        void handle_accept();
        void handle_client(Connection* conn);
        void setupServerSocket();
        void setupEpoll();
        void addServerToEpoll();
    public:
        Server();
        Server(int port, const ServerConfig &config);
        Server(const Server& other);
        Server& operator=(const Server& other);
        ~Server();
        void init();
        void run();
        void stop();
};

#endif