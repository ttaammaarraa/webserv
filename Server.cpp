#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuilder.hpp"

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
    shutdown(conn->fd, SHUT_RDWR);
    close(conn->fd);
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
        std::string response = ResponseBuild::handle(request, config);
        _clientWriteBuffers[conn->fd] = response;

        // Modify epoll to watch for EPOLLOUT
        struct epoll_event ev;
        ev.events = EPOLLOUT;
        ev.data.ptr = conn;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
    }
}

void Server::handle_client_write(Connection* conn)
{
    std::map<int, std::string>::iterator it = _clientWriteBuffers.find(conn->fd);
    if (it == _clientWriteBuffers.end()) {
        // Nothing to write
        return;
    }
    std::string& buffer = it->second;
    ssize_t sent = send(conn->fd, buffer.c_str(), buffer.size(), 0);
    if (sent < 0) {
        // Error, cleanup
        cleanup_connection(conn);
        std::cout << "Send error, client disconnected\n";
        _clientWriteBuffers.erase(it);
        return;
    }
    if ((size_t)sent < buffer.size()) {
        // Not all sent, keep remainder
        buffer = buffer.substr(sent);
    } else {
        // All sent, cleanup write buffer and connection
        _clientWriteBuffers.erase(it);
        std::cout << "Response sent\n";
        cleanup_connection(conn);
    }
}

void Server::run()
{
    struct epoll_event events[1024];

    while (g_keepRunning)
    {
        int nfds = epoll_wait(epoll_fd, events, 1024, 1000);
        if (nfds < 0)
        {
            if (errno == EINTR) continue;
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
            if (conn->isServer) 
            {
                handle_accept();
            } 
            else if (events[i].events & EPOLLOUT) 
            {
                handle_client_write(conn);
            } 
            else if (events[i].events & EPOLLIN) 
            {
                handle_client(conn);
            }
        }
    }
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