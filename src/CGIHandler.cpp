#include "CGIHandler.hpp"

#include "HttpRequest.hpp"
#include "ResponseUtils.hpp"
#include "ServerConfig.hpp"
#include "Server.hpp"

#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <cstring>
#include <iostream>
#include <cctype>  

bool CGIHandler::isCGI(const std::string &path)
{
    // Strip query string before checking extension
    std::string cleanPath = path;
    size_t q = cleanPath.find('?');
    if (q != std::string::npos)
        cleanPath = cleanPath.substr(0, q);

    size_t dot = cleanPath.rfind('.');
    if (dot == std::string::npos)
        return false;

    std::string ext = cleanPath.substr(dot);
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

static void closePipePair(int stdinPipe[2], int stdoutPipe[2])
{
	close(stdinPipe[0]);
	close(stdinPipe[1]);
	close(stdoutPipe[0]);
	close(stdoutPipe[1]);
}

static bool setNonBlockingFd(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return false;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

static std::string launchCGI(const std::string &scriptPath, const HttpRequest &req, Connection *conn, const std::string &cgiPass)
{
	int stdinPipe[2];
	int stdoutPipe[2];
	if (pipe(stdinPipe) != 0)
		return std::string();
	if (pipe(stdoutPipe) != 0)
	{
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		return std::string();
	}

	pid_t pid = fork();
	if (pid < 0)
	{
		closePipePair(stdinPipe, stdoutPipe);
		return std::string();
	}

	if (pid == 0)
	{
		dup2(stdinPipe[0], STDIN_FILENO);
		dup2(stdoutPipe[1], STDOUT_FILENO);

		close(stdinPipe[0]);
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		close(stdoutPipe[1]);

		// Convert to absolute path BEFORE chdir
		char absPath[4096];
		if (realpath(scriptPath.c_str(), absPath) == NULL)
			_exit(127);
		std::string absoluteScript(absPath);

		// chdir into the script's directory
		std::string scriptDir = absoluteScript;
		size_t slash = scriptDir.rfind('/');
		if (slash != std::string::npos)
			scriptDir = scriptDir.substr(0, slash);
		else
			scriptDir = ".";
		chdir(scriptDir.c_str());

		// parse query string
		std::string requestPath = req.getPath();
		std::string queryString;
		size_t qpos = requestPath.find('?');
		if (qpos != std::string::npos)
		{
			queryString = requestPath.substr(qpos + 1);
			requestPath = requestPath.substr(0, qpos);
		}

		std::vector<std::string> envStrings;
		envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1");
		envStrings.push_back("REQUEST_METHOD=" + req.getMethod());
		std::string scriptFilename = cgiPass.empty() ? absoluteScript : cgiPass;
		envStrings.push_back("SCRIPT_FILENAME=" + scriptFilename);
		envStrings.push_back("SERVER_PROTOCOL=" + req.getVersion());
		envStrings.push_back("REDIRECT_STATUS=200");
		std::ostringstream oss;
		oss << req.getBody().size();
		envStrings.push_back("CONTENT_LENGTH=" + oss.str());

		const std::string &contentType = req.getHeaders().count("Content-Type")
			? req.getHeaders().at("Content-Type")
			: "text/plain";
		envStrings.push_back("CONTENT_TYPE=" + contentType);
		envStrings.push_back("REQUEST_URI=" + req.getPath());
		envStrings.push_back("PATH_INFO=" + requestPath);
		envStrings.push_back("QUERY_STRING=" + queryString);
		envStrings.push_back("SCRIPT_NAME=");

		// HTTP_* headers
		const std::map<std::string, std::string>& hdrs = req.getHeaders();
		for (std::map<std::string, std::string>::const_iterator h = hdrs.begin(); h != hdrs.end(); ++h)
		{
			std::string key = "HTTP_";
			for (size_t k = 0; k < h->first.size(); ++k)
				key += (h->first[k] == '-') ? '_' : static_cast<char>(std::toupper(static_cast<unsigned char>(h->first[k])));
			envStrings.push_back(key + "=" + h->second);
		}

		std::vector<char *> envp;
		for (size_t i = 0; i < envStrings.size(); ++i)
			envp.push_back(const_cast<char *>(envStrings[i].c_str()));
		envp.push_back(0);

		std::vector<char *> argv;
		if (!cgiPass.empty())
		{
			argv.push_back(const_cast<char *>(cgiPass.c_str()));
			argv.push_back(const_cast<char *>(absoluteScript.c_str()));
		}
		else if (hasExtension(absoluteScript, ".py"))
		{
			argv.push_back(const_cast<char *>("/usr/bin/python3"));
			argv.push_back(const_cast<char *>(absoluteScript.c_str()));
		}
		else if (hasExtension(absoluteScript, ".php"))
		{
			argv.push_back(const_cast<char *>("/usr/bin/php-cgi"));
			argv.push_back(const_cast<char *>(absoluteScript.c_str()));
		}
		else
		{
			argv.push_back(const_cast<char *>(absoluteScript.c_str()));
		}
		argv.push_back(0);

		execve(argv[0], &argv[0], &envp[0]);
		_exit(127);
	}

	close(stdinPipe[0]);
	close(stdoutPipe[1]);

	if (!setNonBlockingFd(stdoutPipe[0]) || !setNonBlockingFd(stdinPipe[1]))
	{
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
		return std::string();
	}

	const std::string &body = req.getBody();
	if (!body.empty())
	{
		conn->isCgiStdin = true;
		conn->cgi_stdin_fd = stdinPipe[1];
		conn->cgi_stdin_buffer = body;
		conn->cgi_stdin_sent = 0;
	}
	else
	{
		close(stdinPipe[1]);
		conn->isCgiStdin = false;
		conn->cgi_stdin_fd = -1;
		conn->cgi_stdin_buffer.clear();
		conn->cgi_stdin_sent = 0;
	}

	conn->stream_fd = stdoutPipe[0];
	conn->cgi_pid = pid;
	conn->client_fd = conn->fd;
	conn->isCGI = true;
	return std::string();
}

std::string CGIHandler::handle(Connection *conn, const HttpRequest &req, const ServerConfig &conf, const std::string &cgiPass)
{
    if (req.getPath().find("..") != std::string::npos)
        return ResponseUtils::buildErrorRes(403, conf);

    const Location* loc = conf.matchLocation(req.getPath());
    std::string root = conf.root.empty() ? "./www" : conf.root;
    if (loc && !loc->root.empty())
        root = loc->root;

    // Strip query string before building file path
    std::string uriPath = req.getPath();
    size_t qpos = uriPath.find('?');
    if (qpos != std::string::npos)
        uriPath = uriPath.substr(0, qpos);

    std::string scriptPath = ResponseUtils::joinPath(root, uriPath);

    struct stat st;
    if (stat(scriptPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        return ResponseUtils::buildErrorRes(404, conf);

    return launchCGI(scriptPath, req, conn, cgiPass);
}
