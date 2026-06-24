#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuilder.hpp"
#include "CGIHandler.hpp"
#include "ResponseUtils.hpp"

#include <csignal>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
    signal(SIGPIPE, SIG_IGN);
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

    if (conn->isCGI && conn->cgi_pid > 0 && !conn->cgi_reaped)
    {
        int status = 0;
        pid_t waited = waitpid(conn->cgi_pid, &status, WNOHANG);
        if (waited == -1 && errno != ECHILD)
            std::cerr << "CGI cleanup waitpid failed for pid " << conn->cgi_pid << ": " << strerror(errno) << "\n";
    }

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
void Server::check_timeouts()
{
    const int CGI_TIMEOUT = 10;

    time_t now = time(NULL);
    std::map<int, Connection*>::iterator it = _connections.begin();

    while (it != _connections.end())
    {
        Connection* conn = it->second;
        ++it;

        if (conn->isServer)
            continue;

        if ((conn->isCGI || conn->isCgiStdin) && conn->cgi_pid > 0
            && now - conn->last_activity > CGI_TIMEOUT)
        {
            std::cerr << "[Timeout] CGI process " << conn->cgi_pid << " killed\n";
            kill(conn->cgi_pid, SIGKILL);
            waitpid(conn->cgi_pid, NULL, WNOHANG);

            // Send 500 to the client if still connected
            int clientFd = conn->client_fd;
            std::map<int, Connection*>::iterator cit = _connections.find(clientFd);
            if (cit != _connections.end() && cit->second->serverConfig)
            {
                _clientWriteBuffers[clientFd] = ResponseUtils::buildErrorRes(500, *cit->second->serverConfig);
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLOUT;
                ev.data.ptr = cit->second;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, clientFd, &ev);
            }

            cleanup_connection(conn);
            continue;
        }

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
    conn->last_activity = time(NULL);

    char buffer[4096];
    int bytes = recv(conn->fd, buffer, sizeof(buffer), 0);

    if (bytes <= 0)
    {
        cleanup_connection(conn);
        std::cout << "Client disconnected\n";
        return;
    }

    if (conn->isUpload)
    {
        size_t incoming = static_cast<size_t>(bytes);
        size_t remaining = conn->upload_expected - conn->upload_received;
        size_t toWrite = (incoming < remaining) ? incoming : remaining;

        ssize_t written = write(conn->upload_fd, buffer, toWrite);
        if (written < 0)
        {
            // if (erno == EINTR) return; r
            // if (erno == EAGAIN || erno == EWOULDBLOCK) rr
            // {
            //     if (toWrite > 0) conn->upload_buffer.append(buffer, toWrite);
            //     return;
            // }
            cleanup_connection(conn);
            return;
        }

        conn->upload_received += static_cast<size_t>(written);
        if (conn->upload_received >= conn->upload_expected)
        {
            close(conn->upload_fd);
            conn->upload_fd = -1;
            conn->isUpload = false;
            conn->hasPendingRequest = false;

            _clientWriteBuffers[conn->fd] = ResponseBuilder::handle(conn, conn->pendingRequest);
            epoll_event ev; ev.events = EPOLLIN | EPOLLOUT; ev.data.ptr = conn;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
        }
        return;
    }

    _clientBuffers[conn->fd].append(buffer, bytes);
    size_t header_end = _clientBuffers[conn->fd].find("\r\n\r\n");
    if (header_end == std::string::npos) return;

    HttpRequest request = HttpRequest::parse(_clientBuffers[conn->fd]);
    const Location* matchedLocation = conn->serverConfig->matchLocationForRequest(request.getPath(), request.getMethod());
    bool is_cgi = (matchedLocation != NULL && !matchedLocation->cgi_pass.empty()) || CGIHandler::isCGI(request.getPath());

    size_t maxBody = conn->serverConfig->client_max_body_size;
    if (matchedLocation != NULL && matchedLocation->client_max_body_size > 0)
        maxBody = matchedLocation->client_max_body_size;

    if (request.getContentLength() > maxBody)
    {
        _clientWriteBuffers[conn->fd] = ResponseUtils::buildErrorRes(413, *conn->serverConfig);
        epoll_event ev; ev.events = EPOLLIN | EPOLLOUT; ev.data.ptr = conn;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
        return;
    }

    if (!request.isComplete())
        return;
   // if(request.getMethod() == "POST" && is_cgi)


    if (matchedLocation && !matchedLocation->allowed_methods.empty())
    {
        bool allowed = false;
        for (size_t i = 0; i < matchedLocation->allowed_methods.size(); ++i)
            if (matchedLocation->allowed_methods[i] == request.getMethod()) { allowed = true; break; }

        if (!allowed)
        {
            _clientWriteBuffers[conn->fd] = ResponseUtils::buildErrorRes(405, *conn->serverConfig);
            epoll_event ev; ev.events = EPOLLIN | EPOLLOUT; ev.data.ptr = conn;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
            return;
        }
    }

    if (is_cgi)
    {
        conn->pendingRequest = request;
        std::string err = CGIHandler::handle(conn, request, *conn->serverConfig,
            matchedLocation ? matchedLocation->cgi_pass : std::string());
        if (!err.empty())
        {
            _clientWriteBuffers[conn->fd] = err;
            epoll_event ev; ev.events = EPOLLIN | EPOLLOUT; ev.data.ptr = conn;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
            return;
        }
        register_cgi_connection(conn);
        return;
    }

    if (request.getMethod() == "POST")
    {
        if (request.getPath().find("..") != std::string::npos)
        {
            _clientWriteBuffers[conn->fd] = ResponseUtils::buildErrorRes(403, *conn->serverConfig);
            epoll_event ev; ev.events = EPOLLIN | EPOLLOUT; ev.data.ptr = conn;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
            return;
        }
        //if (request.getPath().find("..") != std::string::npos)
        //return buildErrorRes(403);
        std::string filepath;
        if (matchedLocation && !matchedLocation->upload_path.empty())
        {
            // Extract just the filename from the URI — avoid doubling the path
            std::string uriPath = request.getPath();
            size_t slash = uriPath.rfind('/');
            std::string filename = (slash != std::string::npos) ? uriPath.substr(slash + 1) : uriPath;
            filepath = ResponseUtils::joinPath(matchedLocation->upload_path, filename);
        }
        else
        filepath = ResponseUtils::joinPath(conn->serverConfig->root, request.getPath());

        int fd = open(filepath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0644);
        if (fd < 0)
        {
            _clientWriteBuffers[conn->fd] = ResponseUtils::buildErrorRes(403, *conn->serverConfig);
            epoll_event ev; ev.events = EPOLLIN | EPOLLOUT; ev.data.ptr = conn;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
            return;
        }

        // ── determine expected size: chunked has no Content-Length ──
        const std::string& body = request.getBody();
        size_t expectedSize = request.getContentLength();
        if (expectedSize == 0 && !body.empty())
            expectedSize = body.size(); // chunked: body already decoded

        conn->upload_fd = fd;
        conn->upload_expected = expectedSize;
        conn->upload_received = 0;
        conn->isUpload = true;
        conn->pendingRequest = request;

        // ── drain already-buffered body (small requests + chunked) ──
        if (!body.empty())
        {
            size_t toWrite = (expectedSize > 0 && body.size() > expectedSize)
                ? expectedSize : body.size();

            ssize_t written = write(conn->upload_fd, body.c_str(), toWrite);
            if (written > 0)
                conn->upload_received += static_cast<size_t>(written);
        }

        // ── if fully received already, respond now ──
        if (expectedSize == 0 || conn->upload_received >= conn->upload_expected)
        {
            close(conn->upload_fd);
            conn->upload_fd = -1;
            conn->isUpload = false;
            conn->hasPendingRequest = false;

            _clientWriteBuffers[conn->fd] = ResponseBuilder::handle(conn, conn->pendingRequest);
            epoll_event ev; ev.events = EPOLLIN | EPOLLOUT; ev.data.ptr = conn;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
        }
        return;
    }

    // 6.(GET/DELETE)
    std::string headers = ResponseBuilder::handle(conn, request);
    _clientWriteBuffers[conn->fd] = headers;
    epoll_event ev; ev.events = EPOLLIN | EPOLLOUT; ev.data.ptr = conn;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
}

void Server::register_cgi_connection(Connection* clientConn)
{
    if (!clientConn || clientConn->stream_fd < 0)
        return;

    Connection* cgiConn = new Connection();
    cgiConn->fd = clientConn->stream_fd;
    cgiConn->isServer = false;
    cgiConn->isCGI = true;
    cgiConn->client_fd = clientConn->fd;
    cgiConn->cgi_pid = clientConn->cgi_pid;
    cgiConn->serverConfig = clientConn->serverConfig;
    cgiConn->last_activity = time(NULL);

    _connections[cgiConn->fd] = cgiConn;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = cgiConn;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgiConn->fd, &ev) < 0)
    {
        kill(clientConn->cgi_pid, SIGKILL);
        cleanup_connection(cgiConn);
        return;
    }

    if (clientConn->isCgiStdin && clientConn->cgi_stdin_fd >= 0)
    {
        Connection* stdinConn = new Connection();
        stdinConn->fd = clientConn->cgi_stdin_fd;
        stdinConn->isServer = false;
        stdinConn->isCgiStdin = true;
        stdinConn->client_fd = clientConn->fd;
        stdinConn->serverConfig = clientConn->serverConfig;
        stdinConn->last_activity = time(NULL);
        stdinConn->cgi_stdin_buffer = clientConn->cgi_stdin_buffer;
        stdinConn->cgi_stdin_sent = 0;

        _connections[stdinConn->fd] = stdinConn;
        struct epoll_event stdinEv;
        stdinEv.events = EPOLLOUT;
        stdinEv.data.ptr = stdinConn;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stdinConn->fd, &stdinEv) < 0)
        {
            kill(clientConn->cgi_pid, SIGKILL);
            cleanup_connection(stdinConn);
            cleanup_connection(cgiConn);
            return;
        }
    }

    clientConn->isCGI = false;
    clientConn->isCgiStdin = false;
    clientConn->stream_fd = -1;
    clientConn->cgi_stdin_fd = -1;
}

void Server::handle_cgi(Connection* conn, uint32_t events)
{
    if (!conn || !conn->isCGI)
        return;

    conn->last_activity = time(NULL);

    char buffer[4096];
    if (_connections.find(conn->client_fd) == _connections.end()) {
        cleanup_connection(conn);
        return;
    }

    std::string &clientBuffer = _clientWriteBuffers[conn->client_fd];
    bool sawEOF = false;

    while (true)
    {
        ssize_t bytes = read(conn->fd, buffer, sizeof(buffer));
        if (bytes > 0)
        {
            clientBuffer.append(buffer, static_cast<size_t>(bytes));
            continue;
        }
        if (bytes == 0)
        {
            sawEOF = true;
            break;
        }
        // bytes < 0: pipe not ready or closed, stop reading
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // If epoll also signaled HUP/ERR, treat as EOF
            if (events & (EPOLLHUP | EPOLLERR))
                sawEOF = true;
            break;
        }
        sawEOF = true; // other read error = treat as EOF
        break;
    }

    if (!sawEOF)
        return;

    if (conn->cgi_pid > 0 && !conn->cgi_reaped)
    {
        pid_t waited = waitpid(conn->cgi_pid, NULL, WNOHANG);
        if (waited == -1 && errno != ECHILD)
            std::cerr << "CGI waitpid failed for pid " << conn->cgi_pid << ": " << strerror(errno) << "\n";
        else
            conn->cgi_reaped = true;
    }

    if (clientBuffer.empty())
    {
        clientBuffer = ResponseUtils::buildErrorRes(500, *conn->serverConfig);
    }
    else
    {
        clientBuffer = CGIHandler::buildResponseFromCGI(clientBuffer);
    }
    int clientFd = conn->client_fd;
    cleanup_connection(conn);

    std::map<int, Connection*>::iterator it = _connections.find(clientFd);
    if (it != _connections.end())
    {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT;        ev.data.ptr = it->second;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, clientFd, &ev);
    }
}

void Server::handle_cgi_stdin(Connection* conn)
{
    if (!conn || !conn->isCgiStdin)
        return;
    conn->last_activity = time(NULL);
    const std::string &body = conn->cgi_stdin_buffer;
    size_t remaining = body.size() - conn->cgi_stdin_sent;

    if (remaining == 0)
    {
        cleanup_connection(conn);
        return;
    }

    ssize_t written = write(conn->fd, body.c_str() + conn->cgi_stdin_sent, remaining);
    if (written > 0)
    {
        conn->cgi_stdin_sent += static_cast<size_t>(written);
        if (conn->cgi_stdin_sent >= body.size())
            cleanup_connection(conn);
        // else: not done yet, epoll EPOLLOUT will fire again
    }
    else if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        return; // pipe buffer full, wait for next EPOLLOUT
    }
    else
    {
        // real error or written == 0 (pipe closed on child side)
        cleanup_connection(conn);
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
             cleanup_connection(conn);
            std::cout << "Send error, client disconnected\n";
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

    if (_clientWriteBuffers.find(conn->fd) == _clientWriteBuffers.end())
    {
        if (conn->file_fd == -1)
        {
            cleanup_connection(conn);
            std::cout << "Response sent\n";
            return;
        }
    }

    if (conn->file_fd != -1)
    {
        if (!ResponseBuilder::streamGetChunk(conn, epoll_fd))
        {
            cleanup_connection(conn);
            std::cout << "File streaming error, client disconnected\n";
            return;
        }

        if (!conn->isStreaming)
        {
            std::cout << "Response sent\n";
            cleanup_connection(conn);
            return;
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
            continue;

        for (int i = 0; i < nfds; i++)
        {
            Connection* conn = (Connection*)events[i].data.ptr;

            if (conn->isCgiStdin)
            {
                handle_cgi_stdin(conn);
                continue;
            }

            if (conn->isCGI)
            {
                handle_cgi(conn, events[i].events);
                continue;
            }

            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                std::cout << "Connection error/hangup, cleaning up\n";
                cleanup_connection(conn);
                continue;
            }

            if (conn->isServer)
            {
                handle_accept(conn);
            }
            else
            {
                int clientFd = conn->fd;

                if (events[i].events & EPOLLIN)
                {
                    handle_client(conn);
                    if (_connections.find(clientFd) == _connections.end())
                        continue;
                    conn = _connections.find(clientFd)->second;
                }

                if (events[i].events & EPOLLOUT)
                {
                    if (_connections.find(clientFd) == _connections.end())
                        continue;
                    conn = _connections.find(clientFd)->second;
                    handle_client_write(conn);
                }
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
