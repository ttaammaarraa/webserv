#include "CGIHandler.hpp"

#include "HttpRequest.hpp"
#include "ResponseUtils.hpp"
#include "ServerConfig.hpp"

#include <sstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>

bool CGIHandler::isCGI(const std::string& path)
{
    size_t dot = path.rfind('.');

    if (dot == std::string::npos)
        return false;

    std::string ext = path.substr(dot);

    return (
        ext == ".py" ||
        ext == ".php" ||
        ext == ".cgi"
    );
}

std::string runCGI(const std::string& path)
{
	int fd[2];
	if (pipe(fd) == -1)
		return ("HTTP/1.1 500\r\n\r\nPipe error");

	pid_t pid = fork();
	if (pid == -1)
		return ("HTTP/1.1 500\r\n\r\nFork error");

	if (pid == 0)
	{
		dup2(fd[1], STDOUT_FILENO);
		close(fd[0]);
		close(fd[1]);

		char* const argv[] = {
			(char*)"python3",
			(char*)path.c_str(),
			NULL
		};

		char* const envp[] = { NULL };
		execve("/usr/bin/python3", argv, envp);
		exit(1);
	}
	else
	{
		close(fd[1]);

		char buff[4096];
		std::string result;
		ssize_t byt;

		while((byt = read(fd[0], buff, sizeof(buff))) > 0)
			result.append(buff, byt);
		close(fd[0]);
		waitpid(pid, NULL,0);

		return result;
	}
	return "";
}

std::string CGIHandler::handle(
    Connection* conn,
    const HttpRequest& req,
    const ServerConfig& conf
)
{
	(void)conf;
    (void)conn;
  
	std::string path = req.getPath();
	std::string fullPath = "./www" + path;
	std::string output = runCGI(fullPath);

	std::string headers;
	std::string body;

	size_t pos = output.find("\n\n");

	if (pos != std::string::npos)
	{
	    headers = output.substr(0, pos);
	    body = output.substr(pos + 2);
	}
	else
	{
	    body = output;
	    headers = "Content-Type: text/plain";
	}

	std::ostringstream oss;
	oss << "HTTP/1.1 200 OK\r\n";
	oss << headers << "\r\n";
	oss << "Content-Length: " << body.size() << "\r\n";
	oss << "\r\n";
	oss << body;

	return oss.str();
}