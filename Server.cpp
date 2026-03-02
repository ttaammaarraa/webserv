#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ResponseHandler_tempsolve.hpp"
#include <cstring>    //memset 
#include <errno.h>
#include <fcntl.h>    //non-blocking
#include <vector>
#include <poll.h>
#include <iostream>
#include <unistd.h>

//#include <stdexcept>


#include "ServerConfig.hpp"

Server::Server(const ServerConfig& config)
    : _server_fd(-1), _port(config.port), _config(config)
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

    std::cout << "Server started on port " << _port << std::endl;
}

Server::~Server()
{
    if (_server_fd != -1)
        close(_server_fd);
}

void Server::run()
{
    std::vector<pollfd> fds;

    pollfd server_fd_poll;
    server_fd_poll.fd = _server_fd;
    server_fd_poll.events = POLLIN;
    server_fd_poll.revents = 0;
    fds.push_back(server_fd_poll);

    while (true)
    {
        int ready = poll(&fds[0], fds.size(), 1000);
        if (ready < 0)
            throw std::runtime_error("poll failed");

        for (size_t i = 0; i < fds.size(); ++i)
        {
            if (fds[i].revents == 0)
                continue;

            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                close(fds[i].fd);
                fds.erase(fds.begin() + i);
                // Remove buffer for this client
                _client_buffers.erase(fds[i].fd);
                --i;
                continue;
            }

            if (fds[i].fd == _server_fd && (fds[i].revents & POLLIN))
            {
                while (true)
                {
                    sockaddr_in client_addr;
                    socklen_t len = sizeof(client_addr);

                    int client_fd = accept(_server_fd, (sockaddr *)&client_addr, &len);

                    if (client_fd < 0)
                        break;

                    int flags = fcntl(client_fd, F_GETFL, 0);
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                    pollfd client_poll;
                    client_poll.fd = client_fd;
                    client_poll.events = POLLIN | POLLOUT;
                    client_poll.revents = 0;

                    fds.push_back(client_poll);
                    // Initialize buffer for new client
                    _client_buffers[client_fd] = std::string();
                    std::cout << "New client connected\n";
                }
            }
            else if (fds[i].revents & POLLIN)
            {
                char buffer[1024];
                int bytes = recv(fds[i].fd, buffer, sizeof(buffer), 0);

                if (bytes <= 0)
                {
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    _client_buffers.erase(fds[i].fd);
                    std::cout << "Client disconnected\n";
                    --i;
                }
                else
                {
                    // Append received data to client buffer
                    _client_buffers[fds[i].fd].append(buffer, bytes);

                    // Try to parse a complete HTTP request
                    size_t header_end = _client_buffers[fds[i].fd].find("\r\n\r\n");
                    if (header_end != std::string::npos) {
                        std::string raw_request = _client_buffers[fds[i].fd].substr(0, header_end + 4);
                        HttpRequest req = HttpRequest::parse(raw_request);
                        // Only proceed if method, path, and version are not empty
                        if (!req.method.empty() && !req.path.empty() && !req.version.empty()) {
                            // Join root and path
                            std::string full_path = _config.root;
                            if (!full_path.empty() && full_path[full_path.size()-1] == '/' && req.path.size() > 0 && req.path[0] == '/')
                                full_path += req.path.substr(1);
                            else
                                full_path += req.path;
                            std::vector<unsigned char> response = ResponseHandler_tempsolve::constructHttpResponse(full_path);
                            if (!response.empty()) {
                                send(fds[i].fd, &response[0], response.size(), 0);
                                // Close socket after sending response
                                close(fds[i].fd);
                                fds.erase(fds.begin() + i);
                                _client_buffers.erase(fds[i].fd);
                                --i;
                                continue;
                            }
                            // Remove processed request from buffer
                            _client_buffers[fds[i].fd].erase(0, header_end + 4);
                        }
                    }

                    std::cout << "Received: "
                              << std::string(buffer, bytes)
                              << std::endl;
                }
            }
        }
    }
}