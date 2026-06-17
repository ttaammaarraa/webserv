#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <map>
#include <vector>
#include <string>
#include <ctime>
#include "ServerConfig.hpp"

#define CLIENT_TIMEOUT 60

enum ConnState {
    NORMAL,
    CGI_RUNNING,
    STREAMING_FILE
};

struct Connection
{
    int fd;
    bool isServer;
    ServerConfig* serverConfig;
    
    // ⭐ Razan's Additions (Merged)
    time_t last_activity;
    
    // CGI & Streaming placeholders
    ConnState state;
    int client_fd; // Fix: To remember who requested the CGI!
    int stream_fd;

    int cgi_fd; // For CGI output reading
    pid_t cgi_pid; // To track CGI process for cleanup
    std::string cgi_buffer; // To accumulate CGI output

    // ⭐ File Streaming State Variables
    int file_fd;
    size_t file_size;
    size_t bytes_sent;

    Connection() :  fd(-1), isServer(false), serverConfig(NULL), 
                    last_activity(time(NULL)),
                    state(NORMAL),
                    client_fd(-1),
                    stream_fd(-1),
                    cgi_fd(-1),
                    cgi_pid(-1),
                    file_fd(-1),
                    file_size(0),
                    bytes_sent(0) 
    {}
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
        void setupEpoll();
        bool setupListeningSocket(const ServerConfig& serverConfig);
        void addServerToEpoll(int serverFd);
        void buildListenerGroups(std::map< std::pair<int, std::string>, ServerConfig >& groups) const;
        void handle_cgi(Connection* conn);
        
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