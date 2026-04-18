#include <vector>
#include <iostream>
#include <csignal>
#include "ServerConfig.hpp"
#include "ConfigParser.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuilder.hpp"
#include "Server.hpp"

extern volatile sig_atomic_t g_keepRunning;

void signalHandler(int signum)
{
    (void)signum;
    g_keepRunning = 0;
}

int main(int argc, char **argv) 
{
    try 
    {
        //Production
        std::string configFile;
        if (argc == 2)
            configFile = argv[1];
        else if (argc == 1) 
            configFile = "default.conf";
        else 
        {
            std::cerr << "Error: Usage: " << argv[0] << " [config_file]" << std::endl;
            return 1;
        }

        std::vector<ServerConfig> configs = ConfigParser::parse(configFile);
        if (configs.empty())
            throw std::runtime_error("No server blocks were parsed from the configuration file");

        Server server(configs);
        server.init();

        std::signal(SIGINT, signalHandler);
        server.run();
        server.stop();

    }
    /*
       //Start of testing processes -> HttpRequest tests
        {
            std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nUser-Agent: test-agent\r\n\r\n";
            HttpRequest req = HttpRequest::parse(raw);
            assert(req.getMethod() == "GET");
            assert(req.getPath() == "/index.html");
            assert(req.getVersion() == "HTTP/1.1");
            assert(req.getHeaders().find("Host") != req.getHeaders().end());
            assert(req.getHeaders().find("User-Agent") != req.getHeaders().end());
            assert(req.getHeaders().find("Host")->second == "localhost");
            assert(req.getHeaders().find("User-Agent")->second == "test-agent");
            std::cout << "test_basic_get passed\n";
        }
        {
            std::string raw = "POST /submit HTTP/1.0\r\nContent-Type:   text/plain  \r\nX-Test:foo\r\n\r\n";
            HttpRequest req = HttpRequest::parse(raw);
            assert(req.getMethod() == "POST");
            assert(req.getPath() == "/submit");
            assert(req.getVersion() == "HTTP/1.0");
            assert(req.getHeaders().find("Content-Type") != req.getHeaders().end());
            assert(req.getHeaders().find("X-Test") != req.getHeaders().end());
            assert(req.getHeaders().find("Content-Type")->second == "text/plain");
            assert(req.getHeaders().find("X-Test")->second == "foo");
            std::cout << "test_header_whitespace passed\n";
        }
        // Chunked Transfer-Encoding tests
        {
            std::string raw = "POST /u HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
                             "4\r\nWiki\r\n"
                             "5\r\npedia\r\n"
                             "0\r\n\r\n";
            HttpRequest req = HttpRequest::parse(raw);
            assert(req.getMethod() == "POST");
            assert(req.getPath() == "/u");
            assert(req.getBody() == "Wikipedia");
            std::cout << "test_chunked_basic passed\n";
        }
        {
            std::string raw = "POST /u HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
                             "4\r\nWi";
            HttpRequest req = HttpRequest::parse(raw);
            assert(req.getBody().empty());
            std::cout << "test_chunked_incomplete passed\n";
        }
        {
            std::string raw = "POST /u HTTP/1.1\r\nTransfer-Encoding: gzip, chunked\r\n\r\n"
                             "5\r\nhello\r\n"
                             "0\r\n\r\n";
            HttpRequest req = HttpRequest::parse(raw);
            assert(req.getBody() == "hello");
            std::cout << "test_chunked_with_multiple_encodings passed\n";
        }
        std::cout << "All tests passed!\n";  // End of testing processes
  */      
    catch (const std::exception& e)
    {
        std::cerr << "Configuration Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}