#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuilder.hpp"

#include <csignal>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <map>
#include <utility>

volatile sig_atomic_t g_keepRunning = 1;

Server::Server() : epoll_fd(-1), stopped(false) {}

Server::Server(const std::vector<ServerConfig>& configs) : _allConfigs(configs), epoll_fd(-1), stopped(false) {}

Server::~Server()
{
    stop();
}

void Server::setupEpoll()
{
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        throw std::runtime_error("epoll_create1 failed");
}

void Server::buildListenerGroups(std::map< std::pair<int, std::string>, ServerConfig >& groups) const
{
    for (size_t i = 0; i < _allConfigs.size(); ++i)
    {
        std::pair<int, std::string> key(_allConfigs[i].port, _allConfigs[i].host);
        if (groups.find(key) == groups.end())
            groups[key] = _allConfigs[i];
    }
}

void Server::addServerToEpoll(int serverFd)
{
    struct epoll_event ev;
    Connection* serverConn = new Connection();
    serverConn->fd = serverFd;
    serverConn->isServer = true;
    serverConn->serverConfig = &_listenerConfigsByFd[serverFd];

    _connections[serverFd] = serverConn;
    ev.events = EPOLLIN;
    ev.data.ptr = serverConn;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverFd, &ev) < 0)
    {
        _connections.erase(serverFd);
        delete serverConn;
        close(serverFd);
        _listenerConfigsByFd.erase(serverFd);
        throw std::runtime_error("epoll_ctl add listener failed");
    }
}

bool Server::setupListeningSocket(const ServerConfig& serverConfig)
{
    const std::string& host = serverConfig.host;
    int port = serverConfig.port;

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        std::cerr << "[warn] socket failed for " << host << ":" << port << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int flags = fcntl(serverFd, F_GETFL, 0);
    if (flags == -1)
    {
        close(serverFd);
        std::cerr << "[warn] fcntl F_GETFL failed for " << host << ":" << port << std::endl;
        return false;
    }

    if (fcntl(serverFd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        close(serverFd);
        std::cerr << "[warn] fcntl F_SETFL failed for " << host << ":" << port << std::endl;
        return false;
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (host == "127.0.0.1" || host == "localhost")
        address.sin_addr.s_addr = inet_addr("127.0.0.1");
    else
        address.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverFd, (sockaddr*)&address, sizeof(address)) < 0)
    {
        std::cerr << "[warn] bind failed for " << host << ":" << port << std::endl;
        close(serverFd);
        return false;
    }

    if (listen(serverFd, 128) < 0)
    {
        std::cerr << "[warn] listen failed for " << host << ":" << port << std::endl;
        close(serverFd);
        return false;
    }

    _listenerConfigsByFd[serverFd] = serverConfig;
    addServerToEpoll(serverFd);

    std::cout << "Server started on " << host << ":" << port << std::endl;
    return true;
}

void Server::init()
{
    if (_allConfigs.empty())
        throw std::runtime_error("No server blocks were provided");

    setupEpoll();

    std::map< std::pair<int, std::string>, ServerConfig > groups;
    buildListenerGroups(groups);

    int createdListeners = 0;
    std::map< std::pair<int, std::string>, ServerConfig >::iterator it;
    for (it = groups.begin(); it != groups.end(); ++it)
    {
        if (setupListeningSocket(it->second))
            ++createdListeners;
    }

    if (createdListeners == 0)
        throw std::runtime_error("No listening sockets could be created");
}

void Server::cleanup_connection(Connection* conn)
{
    if (!conn) return;

    int fd = conn->fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);

    if (conn->file_fd != -1)
    {
        close(conn->file_fd);
        conn->file_fd = -1;
    }

    if (!conn->isServer)
    {
        _clientBuffers.erase(fd);
        _clientWriteBuffers.erase(fd);
    }
    else
    {
        _listenerConfigsByFd.erase(fd);
    }

    std::map<int, Connection*>::iterator it = _connections.find(fd);
    if (it != _connections.end())
    {
        delete it->second;
        _connections.erase(it);
    }
}

// ⭐ Razan's Timeout logic (Cleaned up)
void Server::check_timeouts()
{
    time_t now = time(NULL);
    std::map<int, Connection*>::iterator it = _connections.begin();
    
    while (it != _connections.end())
    {
        Connection* conn = it->second;
        ++it; // Advance before potentially modifying _connections

        if (conn->isServer)
            continue;

        if (now - conn->last_activity > CLIENT_TIMEOUT)
        {
            std::cout << "[Timeout] Client disconnected automatically\n";
            cleanup_connection(conn);
        }
    }
}

void Server::handle_accept(Connection* serverConn)
{
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(serverConn->fd, (sockaddr*)&client_addr, &addrlen);

    if (client_fd < 0) return;

    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags < 0)
    {
        close(client_fd);
        return;
    }

    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        close(client_fd);
        return;
    }

    Connection* clientConn = new Connection();
    clientConn->fd = client_fd;
    clientConn->isServer = false;
    clientConn->serverConfig = serverConn->serverConfig;
    clientConn->last_activity = time(NULL); // ⭐ Track time
    
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
    conn->last_activity = time(NULL); // ⭐ Update time on read

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
        if (!conn->serverConfig)
        {
            cleanup_connection(conn);
            return;
        }

        HttpRequest request = HttpRequest::parse(_clientBuffers[conn->fd]);
        
        // ⭐ Routing Integration: Match location and override config if found
        const Location* matchedLocation = conn->serverConfig->matchLocation(request.getPath());
        ServerConfig effectiveConfig = *conn->serverConfig;
        
        if (matchedLocation != NULL)
        {
            // Override server config with location-specific settings
            if (!matchedLocation->root.empty())
                effectiveConfig.root = matchedLocation->root;
            // Note: index, autoindex, and allowed_methods from Location
            // can be accessed directly when ResponseBuilder is updated to support them
        }

        ServerConfig *previousConfig = conn->serverConfig;
        conn->serverConfig = &effectiveConfig;
        std::string headers = ResponseBuild::handle(conn, request);
        conn->serverConfig = previousConfig;
        _clientWriteBuffers[conn->fd] = headers;

        struct epoll_event ev;
        ev.events = EPOLLOUT;
        ev.data.ptr = conn;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
    }
}

void Server::handle_client_write(Connection* conn)
{
    conn->last_activity = time(NULL); // ⭐ Update time on write

    std::map<int, std::string>::iterator it = _clientWriteBuffers.find(conn->fd);
    if (it != _clientWriteBuffers.end() && !it->second.empty())
    {
        std::string& buffer = it->second;
        ssize_t sent = send(conn->fd, buffer.c_str(), buffer.size(), 0);
        if (sent < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            cleanup_connection(conn);
            std::cout << "Send error, client disconnected\n";
            _clientWriteBuffers.erase(it);
            return;
        }
        if (sent > 0)
        {
            if ((size_t)sent < buffer.size())
            {
                buffer.erase(0, static_cast<size_t>(sent));
                return;
            }
            _clientWriteBuffers.erase(it);
        }
    }

    if (conn->file_fd != -1)
    {
        off_t offset = static_cast<off_t>(conn->bytes_sent);
        size_t remaining = conn->file_size - conn->bytes_sent;
        ssize_t sent = sendfile(conn->fd, conn->file_fd, &offset, remaining);
        if (sent < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            cleanup_connection(conn);
            std::cout << "File streaming error, client disconnected\n";
            return;
        }
        if (sent == 0)
        {
            cleanup_connection(conn);
            return;
        }

        conn->bytes_sent += static_cast<size_t>(sent);
        if (conn->bytes_sent >= conn->file_size)
        {
            close(conn->file_fd);
            conn->file_fd = -1;
            conn->file_size = 0;
            conn->bytes_sent = 0;
            conn->isStreaming = false;
            std::cout << "Response sent\n";
            cleanup_connection(conn);
        }
    }
}

void Server::run()
{
    struct epoll_event events[1024];

    while (g_keepRunning)
    {
        check_timeouts(); // ⭐ Check timeouts in every loop

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
                handle_accept(conn);
            } 
            else if (conn->isCGI) 
            {
                // ⭐ Placeholder for CGI logic
                // handle_cgi(conn);
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
        ++it;
        cleanup_connection(conn);
    }

    _clientBuffers.clear();
    _clientWriteBuffers.clear();
    _listenerConfigsByFd.clear();

    if (epoll_fd != -1)
    {
        close(epoll_fd);
        epoll_fd = -1;
    }

    std::cout << "Server stopped safely\n";
}