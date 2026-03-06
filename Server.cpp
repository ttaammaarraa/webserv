#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuild.hpp"
#include "ServerConfig.hpp"
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <vector>
#include <sys/epoll.h>
#include <iostream>
#include <unistd.h>

Server::Server(int port, const ServerConfig& config) 
    : _server_fd(-1), _port(port), epoll_fd(-1), config(config) 
{
    std::memset(&_address, 0, sizeof(_address));
    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY;
    _address.sin_port = htons(_port);
}

void Server::init()
{
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0)
        throw std::runtime_error("Socket creation failed");

    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt failed");

    int flags = fcntl(_server_fd, F_GETFL, 0);
    if (fcntl(_server_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl failed");

    if (bind(_server_fd, (struct sockaddr *)&_address, sizeof(_address)) < 0)
        throw std::runtime_error("bind failed");

    if (listen(_server_fd, 128) < 0)
        throw std::runtime_error("listen failed");

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        throw std::runtime_error("epoll_create1 failed");

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = _server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, _server_fd, &ev) == -1)
        throw std::runtime_error("epoll_ctl failed");

    std::cout << "Server started on port " << _port << std::endl;
}

Server::~Server()
{
    if (_server_fd != -1)
        close(_server_fd);
    if (epoll_fd != -1)
        close(epoll_fd);
}

void Server::run()
{
    struct epoll_event events[1024];

    while (true)
    {
        int nfds = epoll_wait(epoll_fd, events, 1024, 1000);
        if (nfds < 0)
        {
            if (errno == EINTR) continue;
            throw std::runtime_error("epoll_wait failed");
        }

        for (int i = 0; i < nfds; ++i)
        {
            int fd = events[i].data.fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                _clientBuffers.erase(fd);
                continue;
            }

            if (fd == _server_fd)
            {
                while (true)
                {
                    sockaddr_in client_addr;
                    socklen_t len = sizeof(client_addr);
                    int client_fd = accept(_server_fd, (sockaddr *)&client_addr, &len);

                    if (client_fd < 0)
                        break;

                    int c_flags = fcntl(client_fd, F_GETFL, 0);
                    fcntl(client_fd, F_SETFL, c_flags | O_NONBLOCK);

                    struct epoll_event ev_client;
                    ev_client.events = EPOLLIN | EPOLLET;
                    ev_client.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev_client);

                    _clientBuffers[client_fd] = "";
                    std::cout << "New client connected\n";
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                char buffer[1024];
                int bytes = recv(fd, buffer, sizeof(buffer), 0);

                if (bytes <= 0)
                {
                    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                        continue;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    _clientBuffers.erase(fd);
                    std::cout << "Client disconnected\n";
                }
                else
                {
                    _clientBuffers[fd].append(buffer, bytes);
                    std::string& client_buffer = _clientBuffers[fd];
                    size_t header_end = client_buffer.find("\r\n\r\n");
                    
                    if (header_end != std::string::npos)
                    {
                        std::cout << "Complete request received, processing...\n";
                        HttpRequest request = HttpRequest::parse(client_buffer);
                        std::string response = ResponseBuild::handle(request, config);
                        
                        int total_sent = 0;
                        int response_size = response.size();
                        while (total_sent < response_size)
                        {
                            int sent = send(fd, response.c_str() + total_sent, 
                                          response_size - total_sent, 0);
                            if (sent < 0)
                            {
                                std::cerr << "Send failed\n";
                                break;
                            }
                            total_sent += sent;
                        }
                        
                        std::cout << "Response sent, closing connection\n";
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        _clientBuffers.erase(fd);
                    }
                }
            }
        }
    }
}