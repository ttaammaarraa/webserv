#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuild.hpp"

#include <cstdlib>
#include <csignal>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>
#include <map>

volatile sig_atomic_t g_keepRunning = 1;


Connection::Connection()
    : fd(-1),
      isServer(false),
      last_activity(time(NULL)),
      isCGI(false),
      isCGIConn(false),
      client(NULL),
      headers_sent(false),
      cgi_fd(-1),
      stream_fd(-1),
      isStreaming(false),
      stream_offset(0)
{}

Server::Server() : _server_fd(-1), _port(0), epoll_fd(-1), _serverConn(NULL), stopped(false)
{
    std::memset(&_address, 0, sizeof(_address));
}

Server::Server(int port, const ServerConfig& config)
    : _server_fd(-1), _port(port), epoll_fd(-1), config(config), _serverConn(NULL), stopped(false)
{
    std::memset(&_address, 0, sizeof(_address));

    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY;
    _address.sin_port = htons(_port);
}

Server::Server(const Server& other)
{
    _server_fd = other._server_fd;
    _port = other._port;
    epoll_fd = other.epoll_fd;
    _address = other._address;
    _clientBuffers = other._clientBuffers;
    config = other.config;
    stopped = false;

    if (other._serverConn)
        _serverConn = new Connection(*other._serverConn);
    else
        _serverConn = NULL;
}

Server& Server::operator=(const Server& other)
{
    if (this != &other)
    {
        _server_fd = other._server_fd;
        _port = other._port;
        epoll_fd = other.epoll_fd;
        _address = other._address;
        _clientBuffers = other._clientBuffers;
        config = other.config;
        stopped = false;

        if (_serverConn)
            delete _serverConn;

        if (other._serverConn)
            _serverConn = new Connection(*other._serverConn);
        else
            _serverConn = NULL;
    }
    return *this;
}

Server::~Server()
{
    stop(); // Ensure all connections and FDs are cleaned up
}

void Server::check_timeouts()
{
    time_t now = time(NULL);

    std::map<int, Connection*>::iterator it = _connections.begin();
    while (it != _connections.end())
    {
        Connection* conn = it->second;
        ++it;

        if (conn->isServer)
            continue;

        if (now - conn->last_activity > CLIENT_TIMEOUT)
        {
            std::cout << "Client timeout\n";
            cleanup_connection(conn);
        }
    }
}

void Server::setupServerSocket()
{
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0)
        throw std::runtime_error("socket failed");

    int opt = 1;
    setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int flags = fcntl(_server_fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl F_GETFL failed");
    fcntl(_server_fd, F_SETFL, flags | O_NONBLOCK);

    if (bind(_server_fd, (sockaddr*)&_address, sizeof(_address)) < 0)
        throw std::runtime_error("bind failed");

    if (listen(_server_fd, 128) < 0)
        throw std::runtime_error("listen failed");
}

void Server::setupEpoll()
{
    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
        throw std::runtime_error("epoll_create1 failed");
}

void Server::addServerToEpoll()
{
    struct epoll_event ev;

    _serverConn = new Connection();
    _serverConn->fd = _server_fd;
    _serverConn->isServer = true;
    _connections[_server_fd] = _serverConn;

    ev.events = EPOLLIN;
    ev.data.ptr = _serverConn;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, _server_fd, &ev);
}

void Server::init()
{
    setupServerSocket();
    setupEpoll();
    addServerToEpoll();

    std::cout << "Server started on port " << _port << std::endl;
}

void Server::cleanup_connection(Connection* conn)
{
    if (!conn) return;

    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
    if (!conn->isCGIConn)
        shutdown(conn->fd, SHUT_RDWR);
    close(conn->fd);
    // if (conn->isCGIConn) double close
    // {
    //     close(conn->fd);
    // }
     if (conn->stream_fd != -1)
        close(conn->stream_fd);
    
    _clientBuffers.erase(conn->fd);
    _clientWriteBuffers.erase(conn->fd);
    std::map<int, Connection*>::iterator it = _connections.find(conn->fd);
    if (it != _connections.end()) {
        delete it->second;
        _connections.erase(it);
    }
}

void Server::handle_accept()
{
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(_server_fd, (sockaddr*)&client_addr, &addrlen);
    
    if (client_fd < 0) return;

    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    Connection* clientConn = new Connection();
    clientConn->fd = client_fd;
    clientConn->isServer = false;
    clientConn->last_activity = time(NULL); //*2*//
    _connections[client_fd] = clientConn;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = clientConn;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

    _clientBuffers[client_fd] = "";
    std::cout << "Client connected\n";
}

void Server::handle_client(Connection* conn)
{
    conn->last_activity = time(NULL);//2

    char buffer[4096];
    int bytes = recv(conn->fd, buffer, sizeof(buffer), 0);
    
    if (bytes <= 0)
    {
        cleanup_connection(conn);
        std::cout << "Client disconnected\n";
        return;
    }

    _clientBuffers[conn->fd].append(buffer, bytes);
    size_t header_end = _clientBuffers[conn->fd].find("\r\n\r\n");
    if (header_end != std::string::npos)
    {
        HttpRequest request = HttpRequest::parse(_clientBuffers[conn->fd]);

        if (request.path.find("/cgi") != std::string::npos)
        {
            start_cgi(conn, "./cgi_script");
            return;
        }
        std::string filePath = "." + request.path;

        if (filePath == "./")
            filePath = "./index.html";

        int file_fd = open(filePath.c_str(), O_RDONLY);

        if (file_fd < 0)
        {
            std::string response = 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n\r\n"
                "File not found";

            _clientWriteBuffers[conn->fd] = response;
        }
        else
        {
            std::string headers =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n\r\n";

            _clientWriteBuffers[conn->fd] = headers;

            conn->stream_fd = file_fd;
            conn->isStreaming = true;
        }
        // Modify epoll to watch for EPOLLOUT
        struct epoll_event ev;
        ev.events = EPOLLOUT;
        ev.data.ptr = conn;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
    }
}

void Server::handle_client_write(Connection* conn)
{
    conn->last_activity = time(NULL);

    std::map<int, std::string>::iterator it = _clientWriteBuffers.find(conn->fd);
    if (it != _clientWriteBuffers.end())
    {
        std::string& buffer = it->second;

        ssize_t sent = send(conn->fd, buffer.c_str(), buffer.size(), 0);

        if (sent < 0)
        {
            cleanup_connection(conn);
            _clientWriteBuffers.erase(it);
            return;
        }

        if ((size_t)sent < buffer.size())
        {
            buffer = buffer.substr(sent);
            return;
        }

        
        _clientWriteBuffers.erase(it);

        if (!conn->isStreaming)
        {
            cleanup_connection(conn);
            return;
        }
    }

    //Streaming 
    if (conn->isStreaming)
    {
        char buffer[4096];

        int bytes = read(conn->stream_fd, buffer, sizeof(buffer));

        if (bytes <= 0)
        {
            close(conn->stream_fd);
            conn->stream_fd = -1;
            conn->isStreaming = false;
            conn->stream_offset = 0; // reset 
            std::cout << "File sent\n";
            cleanup_connection(conn);
            return;
        }

        ssize_t sent = send(conn->fd, buffer, bytes, 0);

        if (sent <= 0)
        {
            close(conn->stream_fd);
            conn->stream_fd = -1;
            cleanup_connection(conn);
            return;
        }

        // offset tracking (important for robustness)
        conn->stream_offset += sent;
    }
}

void Server::run()
{
    struct epoll_event events[1024];

    while (g_keepRunning)
    {
        check_timeouts();

        int nfds = epoll_wait(epoll_fd, events, 1024, 1000);

        if (nfds < 0)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("epoll_wait failed");
        }

        for (int i = 0; i < nfds; i++)
        {
            Connection* conn = (Connection*)events[i].data.ptr;

            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                cleanup_connection(conn);
                continue;
            }

            // SERVER SOCKET
            if (conn->isServer)
            {
                handle_accept();
            }

            // CGI PIPE
            else if (conn->isCGI && (events[i].events & EPOLLIN))
            {
                handle_cgi(conn);
            }

            // CLIENT WRITE (sending response)
            else if (events[i].events & EPOLLOUT)
            {
                handle_client_write(conn);
            }

            // CLIENT READ
            else if (events[i].events & EPOLLIN)
            {
                handle_client(conn);
            }
        }
    }
}

void Server::start_cgi(Connection* clientConn, std::string path)
{
    int pipefd[2];
    if (pipe(pipefd) < 0)
        return;

    pid_t pid = fork();

    if (pid == 0)
    {
        // CHILD (CGI process)
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        char *args[] = {(char*)path.c_str(), NULL};

        execve(path.c_str(), args, NULL);

        exit(1);
    }
    else
    {
        // PARENT (server)
        close(pipefd[1]);

        int flags = fcntl(pipefd[0], F_GETFL, 0);
        fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

        Connection* cgiConn = new Connection();

        cgiConn->fd = pipefd[0];
        cgiConn->isCGI = true;
        cgiConn->client = clientConn;
        cgiConn->isServer = false;
        cgiConn->isCGIConn = true;

        _connections[cgiConn->fd] = cgiConn;

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = cgiConn;

        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgiConn->fd, &ev);
    }
}

void Server::handle_cgi(Connection* conn)
{
    conn->last_activity = time(NULL);

    if (!conn->client)
    {
        cleanup_connection(conn);
        return;
    }

    if (!conn->headers_sent)
    {
        std::string headers =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n\r\n";

        send(conn->client->fd, headers.c_str(), headers.size(), 0);

        conn->headers_sent = true;
    }

    char buffer[4096];

    int bytes = read(conn->fd, buffer, sizeof(buffer));

    if (bytes <= 0)
    {
        if (conn->client)
            cleanup_connection(conn->client);
        cleanup_connection(conn);
        return;
    }

    //  FIXED
    send(conn->client->fd, buffer, bytes, 0);
}

void Server::stop()
{
    if (stopped) return;
    stopped = true;

    std::map<int, Connection*>::iterator it = _connections.begin();
    while (it != _connections.end()) {
        Connection* conn = it->second;
        ++it; // Advance before erase
        cleanup_connection(conn);
    }

    _clientBuffers.clear();

    if (_server_fd != -1)
    {
        close(_server_fd);
        _server_fd = -1;
    }

    if (epoll_fd != -1)
    {
        close(epoll_fd);
        epoll_fd = -1;
    }

    std::cout << "Server stopped safely\n";
}