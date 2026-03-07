#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuild.hpp"

#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>

volatile sig_atomic_t g_keepRunning = 1;

Server::Server(int port, const ServerConfig& config)
    : _server_fd(-1), _port(port), epoll_fd(-1), config(config)
{
    std::memset(&_address, 0, sizeof(_address));

    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY;
    _address.sin_port = htons(_port);
}

Server::~Server()
{
    if (_server_fd != -1)
        close(_server_fd);

    if (epoll_fd != -1)
        close(epoll_fd);
}

void Server::init()
{
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (_server_fd < 0)
        throw std::runtime_error("socket failed");

    int opt = 1;
    setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int flags = fcntl(_server_fd, F_GETFL, 0);
    fcntl(_server_fd, F_SETFL, flags | O_NONBLOCK);

    if (bind(_server_fd, (sockaddr*)&_address, sizeof(_address)) < 0)
        throw std::runtime_error("bind failed");

    if (listen(_server_fd, 128) < 0)
        throw std::runtime_error("listen failed");

    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
        throw std::runtime_error("epoll_create1 failed");

    struct epoll_event ev;

    Connection* conn = new Connection();
    conn->fd = _server_fd;
    conn->isServer = true;

    ev.events = EPOLLIN;
    ev.data.ptr = conn;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, _server_fd, &ev);

    std::cout << "Server started on port " << _port << std::endl;
}

void Server::run()
{
    struct epoll_event events[1024];

    while (g_keepRunning)
    {
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
            int fd = conn->fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);

                delete conn;
                _clientBuffers.erase(fd);

                continue;
            }

            if (conn->isServer)
            {
                sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);

                int client_fd = accept(_server_fd,
                                       (sockaddr*)&client_addr,
                                       &addrlen);

                if (client_fd < 0)
                    continue;

                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                struct epoll_event ev;

                Connection* clientConn = new Connection();
                clientConn->fd = client_fd;
                clientConn->isServer = false;

                ev.events = EPOLLIN;
                ev.data.ptr = clientConn;

                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

                _clientBuffers[client_fd] = "";

                std::cout << "Client connected\n";
            }
            else if (events[i].events & EPOLLIN)
            {
                char buffer[4096];

                int bytes = recv(fd, buffer, sizeof(buffer), 0);

                if (bytes <= 0)
                {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);

                    delete conn;
                    _clientBuffers.erase(fd);

                    std::cout << "Client disconnected\n";

                    continue;
                }

                _clientBuffers[fd].append(buffer, bytes);

                std::string& client_buffer = _clientBuffers[fd];

                size_t header_end = client_buffer.find("\r\n\r\n");

                if (header_end != std::string::npos)
                {
                    HttpRequest request =
                        HttpRequest::parse(client_buffer);

                    std::string response =
                        ResponseBuild::handle(request, config);

                    int total_sent = 0;
                    int size = response.size();

                    while (total_sent < size)
                    {
                        int sent = send(fd,
                                        response.c_str() + total_sent,
                                        size - total_sent,
                                        0);

                        if (sent <= 0)
                            break;

                        total_sent += sent;
                    }

                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);

                    shutdown(fd, SHUT_RDWR);
                    close(fd);

                    delete conn;
                    _clientBuffers.erase(fd);

                    std::cout << "Response sent\n";
                }
            }
        }
    }
}

void Server::stop()
{
    for (std::map<int,std::string>::iterator it =
         _clientBuffers.begin();
         it != _clientBuffers.end(); ++it)
    {
        int fd = it->first;

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
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