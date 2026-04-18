#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <map>
#include <vector>
#include <string>
#include "ServerConfig.hpp"

struct Connection
{
    int fd;
    bool isServer;
    int port;
    std::vector<ServerConfig>* serverConfigs;

    Connection() : fd(-1), isServer(false), port(0), serverConfigs(NULL) {}
};

class Server
{
    private:
        std::vector<ServerConfig> _allConfigs;
        int epoll_fd;
        std::map<int, std::string> _clientBuffers;
        std::map<int, std::string> _clientWriteBuffers;
        std::map<int, Connection*> _connections;
        std::map<int, std::vector<ServerConfig> > _listenerConfigsByFd;
        void cleanup_connection(Connection* conn);
        void handle_accept(Connection* serverConn);
        void handle_client(Connection* conn);
        void handle_client_write(Connection* conn);
        void setupEpoll();
        bool setupListeningSocket(const std::string& host, int port, const std::vector<ServerConfig>& serverConfigs);
        void addServerToEpoll(int serverFd, int port);
        void buildListenerGroups(std::map< std::pair<int, std::string>, std::vector<ServerConfig> >& groups) const;
        bool stopped;

        Server(const Server& other);
        Server& operator=(const Server& other);

    public:
        Server();
        Server(const std::vector<ServerConfig>& configs);

        ~Server();
        void init();
        void run();
        void stop();
};

#endif