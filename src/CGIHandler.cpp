#include "CGIHandler.hpp"

#include "HttpRequest.hpp"
#include "ResponseUtils.hpp"
#include "ServerConfig.hpp"
#include "Server.hpp"

#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <vector>
#include <cstring>
#include <sys/epoll.h>
#include <fcntl.h>

bool CGIHandler::isCGI(const std::string &path)
{
	size_t dot = path.rfind('.');
	if (dot == std::string::npos)
		return false;

	std::string ext = path.substr(dot);
	return ext == ".py" || ext == ".php" || ext == ".cgi";
}

static bool hasExtension(const std::string &path, const std::string &ext)
{
	return path.size() >= ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

std::string CGIHandler::buildResponseFromCGI(const std::string &output)
{
	std::string headers;
	std::string body;
	size_t sep = output.find("\r\n\r\n");

	if (sep != std::string::npos)
	{
		headers = output.substr(0, sep);
		body = output.substr(sep + 4);
	}
	else
	{
		body = output;
	}

	std::istringstream headerStream(headers);
	std::string line;
	std::string responseHeaders;
	bool hasContentType = false;
	std::string statusLine = "HTTP/1.1 200 OK";

	while (std::getline(headerStream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.end() - 1);

		if (line.empty())
			continue;

		if (line.find("Status:") == 0)
		{
			std::string value = line.substr(7);
			while (!value.empty() && value[0] == ' ')
				value.erase(value.begin());
			if (!value.empty())
				statusLine = "HTTP/1.1 " + value;
			continue;
		}

		if (line.find("Content-Type:") == 0)
			hasContentType = true;

		responseHeaders += line + "\r\n";
	}

	if (!hasContentType)
		responseHeaders += "Content-Type: text/html\r\n";

	std::ostringstream oss;
	oss << statusLine << "\r\n";
	oss << responseHeaders;
	oss << "Content-Length: " << body.size() << "\r\n";
	oss << "\r\n";
	oss << body;
	return oss.str();
}

static bool executeCGI(const std::string &scriptPath, const HttpRequest &req, int &out_Fd, pid_t &out_pid)
{
	int stdinPipe[2];
	int stdoutPipe[2];
	if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0)
		return false;

	pid_t pid = fork();
	if (pid < 0)
	{
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		close(stdoutPipe[1]);
		return false;
	}

	if (pid == 0)
	{
		dup2(stdinPipe[0], STDIN_FILENO);
		dup2(stdoutPipe[1], STDOUT_FILENO);

		close(stdinPipe[0]);
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		close(stdoutPipe[1]);

		std::vector<std::string> envStrings;
		envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1");
		envStrings.push_back("REQUEST_METHOD=" + req.getMethod());
		envStrings.push_back("SCRIPT_FILENAME=" + scriptPath);
		envStrings.push_back("SERVER_PROTOCOL=" + req.getVersion());
		envStrings.push_back("REDIRECT_STATUS=200");
		std::ostringstream oss;
		oss << req.getBody().size();
		envStrings.push_back("CONTENT_LENGTH=" + oss.str());

		const std::string &contentType = req.getHeaders().count("Content-Type")
											 ? req.getHeaders().at("Content-Type")
											 : "text/plain";
		envStrings.push_back("CONTENT_TYPE=" + contentType);

		std::vector<char *> envp;
		for (size_t i = 0; i < envStrings.size(); ++i)
			envp.push_back(const_cast<char *>(envStrings[i].c_str()));
		envp.push_back(0);

		std::vector<char *> argv;
		if (hasExtension(scriptPath, ".py"))
		{
			argv.push_back(const_cast<char *>("/usr/bin/python3"));
			argv.push_back(const_cast<char *>(scriptPath.c_str()));
		}
		else if (hasExtension(scriptPath, ".php"))
		{
			argv.push_back(const_cast<char *>("/usr/bin/php-cgi"));
			argv.push_back(const_cast<char *>(scriptPath.c_str()));
		}
		else
		{
			argv.push_back(const_cast<char *>(scriptPath.c_str()));
		}
		argv.push_back(0);

		execve(argv[0], &argv[0], &envp[0]);
		_exit(127);
	}

	close(stdinPipe[0]);
	close(stdoutPipe[1]);

	 if (!req.getBody().empty())
        write(stdinPipe[1], req.getBody().c_str(), req.getBody().size());

    close(stdinPipe[1]);

	out_Fd = stdoutPipe[0];
	out_pid = pid;
	return true;
}

static bool startCGI(Connection* conn,
                     const std::string &scriptPath,
                     const HttpRequest &req,
                     int epoll_fd)
{
    if (!executeCGI(scriptPath, req, conn->cgi_fd, conn->cgi_pid))
        return false;

    conn->state = CGI_RUNNING;
    conn->cgi_buffer.clear();

	struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = conn;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn->cgi_fd, &ev) < 0)
        return false;

    int flags = fcntl(conn->cgi_fd, F_GETFL, 0);
    fcntl(conn->cgi_fd, F_SETFL, flags | O_NONBLOCK);

    return true;
}

std::string CGIHandler::handle(Connection *conn,
                               const HttpRequest &req,
                               const ServerConfig &conf,
                               int epoll_fd)
{
	if (req.getPath().find("..") != std::string::npos)
		return ResponseUtils::buildErrorRes(403, conf);

	std::string scriptPath = ResponseUtils::joinPath(
		conf.root.empty() ? "./www" : conf.root,
		req.getPath()
	);

	struct stat st;
	if (stat(scriptPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
		return ResponseUtils::buildErrorRes(404, conf);

	if (!startCGI(conn, scriptPath, req, epoll_fd))
		return ResponseUtils::buildErrorRes(500, conf);

	return ResponseUtils::buildErrorRes(500, conf);}
