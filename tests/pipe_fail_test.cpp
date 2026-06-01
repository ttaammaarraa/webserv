#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <errno.h>
#include <string.h>

int main()
{
    // Lower RLIMIT_NOFILE to force pipe failure on second pipe
    struct rlimit rl;
    rl.rlim_cur = 5;
    rl.rlim_max = 5;
    if (setrlimit(RLIMIT_NOFILE, &rl) != 0)
    {
        std::cerr << "setrlimit failed: " << strerror(errno) << std::endl;
        return 2;
    }

    int stdinPipe[2];
    int stdoutPipe[2];

    if (pipe(stdinPipe) != 0)
    {
        std::cerr << "stdin pipe failed: " << strerror(errno) << std::endl;
        return 1;
    }
    std::cout << "stdinPipe created fds: " << stdinPipe[0] << ", " << stdinPipe[1] << std::endl;

    if (pipe(stdoutPipe) != 0)
    {
        std::cerr << "stdout pipe failed as expected: " << strerror(errno) << std::endl;
        // close first pipe to avoid leak
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        // verify closed
        if (fcntl(stdinPipe[0], F_GETFD) == -1 && errno == EBADF && fcntl(stdinPipe[1], F_GETFD) == -1 && errno == EBADF)
        {
            std::cout << "stdinPipe fds successfully closed and invalid now" << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "stdinPipe fds still open!" << std::endl;
            return 3;
        }
    }

    std::cout << "Both pipes created (unexpected)" << std::endl;
    // Cleanup
    close(stdinPipe[0]); close(stdinPipe[1]);
    close(stdoutPipe[0]); close(stdoutPipe[1]);
    return 0;
}
