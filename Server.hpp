#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <map>
#include <string>
#include "ServerConfig.hpp"

#define CLIENT_TIMEOUT 60

struct Connection
{
    Connection();

    int fd;
    bool isServer;

    // ⭐ Task 2: timeout
    time_t last_activity;

    // ⭐ Task 1: CGI
    bool isCGI;
    int cgi_fd;
    int cgi_pipe[2];
    bool isCGIConn;

    // ⭐ Task 3: streaming
    int stream_fd;
    bool isStreaming;
    size_t stream_offset;

};

class Server
{
    private:
        int _server_fd;
        int _port;
        int epoll_fd;
        sockaddr_in _address;
        std::map<int, std::string> _clientBuffers;
        std::map<int, std::string> _clientWriteBuffers; // Write buffers for clients
        ServerConfig config;
        Connection* _serverConn;
        std::map<int, Connection*> _connections; // Track all active connections

        void cleanup_connection(Connection* conn);
        void handle_accept();
        void handle_client(Connection* conn);
        void handle_client_write(Connection* conn);
        void setupServerSocket();
        void setupEpoll();
        void addServerToEpoll();
        void check_timeouts();
        void start_cgi(Connection* conn, std::string path);
        void handle_cgi(Connection* conn);
        bool stopped;
    
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