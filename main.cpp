#include <vector>
#include <iostream>
#include <cassert>
#include "ServerConfig.hpp"
#include "ConfigParser.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuild.hpp"

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

        ServerConfig config = ConfigParser::parse(configFile);
        std::cout << "Port: " << config.port << std::endl;
        std::cout << "Host: " << config.host << std::endl;
        std::cout << "Root: " << config.root << std::endl;
        for (std::map<int, std::string>::iterator it = config.error_pages.begin(); it != config.error_pages.end(); ++it) 
            std::cout << "Error page " << it->first << ": " << it->second << std::endl;
        ResponseBuild builder;

        std::vector<std::string> testPaths;
            testPaths.push_back("/index.html"); //  200 OK
             testPaths.push_back("/doesnotexist.html") ;//  404
             testPaths.push_back("/../etc/passwd"); //  403
             testPaths.push_back("/forbidden.html"); //read permission → 403 ((need to do this to the file:chmod 000 www/forbidden.html))
             testPaths.push_back("/folder/"); // directory → index.html

        for (size_t i = 0; i < testPaths.size(); ++i)
        {
            std::string path = testPaths[i];
            HttpRequest req;
            req.method = "GET";
            req.path = path;
            req.version = "HTTP/1.1";

            std::cout << "===== Testing path: " << path << " =====\n";
            std::string response = builder.handle(req, config);
            std::cout << response << "\n\n";
        }

    }
    /*
       //Start of testing processes -> HttpRequest tests
        {
            std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nUser-Agent: test-agent\r\n\r\n";
            HttpRequest req = HttpRequest::parse(raw);
            assert(req.method == "GET");
            assert(req.path == "/index.html");
            assert(req.version == "HTTP/1.1");
            assert(req.headers["Host"] == "localhost");
            assert(req.headers["User-Agent"] == "test-agent");
            std::cout << "test_basic_get passed\n";
        }
        {
            std::string raw = "POST /submit HTTP/1.0\r\nContent-Type:   text/plain  \r\nX-Test:foo\r\n\r\n";
            HttpRequest req = HttpRequest::parse(raw);
            assert(req.method == "POST");
            assert(req.path == "/submit");
            assert(req.version == "HTTP/1.0");
            assert(req.headers["Content-Type"] == "text/plain");
            assert(req.headers["X-Test"] == "foo");
            std::cout << "test_header_whitespace passed\n";
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