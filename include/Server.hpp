#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <sys/types.h>
#include <map>
#include <vector>
#include <string>
#include <ctime>
#include "ServerConfig.hpp"

#define CLIENT_TIMEOUT 60

struct Connection
{
    int fd;
    bool isServer;
    ServerConfig* serverConfig;
    
    // ⭐ Razan's Additions (Merged)
    time_t last_activity;
    
    // CGI & Streaming placeholders
    bool isCGI;
    int client_fd; // Fix: To remember who requested the CGI!
    bool isStreaming;
    int stream_fd;
    pid_t cgi_pid;

    // ⭐ File Streaming State Variables
    int file_fd;
    size_t file_size;
    size_t bytes_sent;

    Connection() : fd(-1), isServer(false), serverConfig(NULL), 
                   last_activity(time(NULL)), isCGI(false), 
                   client_fd(-1), isStreaming(false), stream_fd(-1), cgi_pid(-1),
                   file_fd(-1), file_size(0), bytes_sent(0) {}
};

class Server
{
    private:
        std::vector<ServerConfig> _allConfigs;
        int epoll_fd;
        std::map<int, std::string> _clientBuffers;
        std::map<int, std::string> _clientWriteBuffers;
        std::map<int, Connection*> _connections;
        std::map<int, ServerConfig> _listenerConfigsByFd;
        
        void cleanup_connection(Connection* conn);
        void handle_accept(Connection* serverConn);
        void handle_client(Connection* conn);
        void handle_client_write(Connection* conn);
        void handle_cgi(Connection* conn);
        void register_cgi_connection(Connection* clientConn);
        void setupEpoll();
        bool setupListeningSocket(const ServerConfig& serverConfig);
        void addServerToEpoll(int serverFd);
        void buildListenerGroups(std::map< std::pair<int, std::string>, ServerConfig >& groups) const;
        
        // ⭐ Razan's Timeout Function
        void check_timeouts();
        
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