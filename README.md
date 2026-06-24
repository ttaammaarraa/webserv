*This project has been created as part of the 42 curriculum by taabu-fe , aal-joul , rhasan.*

# Webserv

## Description
Webserv is a custom HTTP/1.1 web server written entirely in C++98. The goal of this project is to deeply understand the Hypertext Transfer Protocol (HTTP), Unix socket programming, and non-blocking I/O multiplexing.

The server is built using an event-driven architecture powered by `epoll`. It is designed to be highly resilient, handling multiple concurrent client connections without crashing, hanging, or using thread pools. It supports GET, POST, and DELETE methods, serves static websites, handles file uploads, generates autoindex directory listings, and executes CGI scripts (like PHP and Python) completely asynchronously.

## Instructions
**Prerequisites:**
This project requires a Unix-based operating system that supports the `epoll` system call. You will also need a standard C++ compiler (`c++`, `g++`, or `clang++`).

**Compilation:**
To compile the server, run the following command at the root of the repository. This will use the strict `-Wall -Wextra -Werror -std=c++98` flags:

    make

Other available make rules: `clean`, `fclean`, `re`.

**Execution:**
Run the compiled executable and provide a configuration file as an argument:

    ./webserv default.conf

If no configuration file is provided, the program will attempt to run using a default setup. You can test the server using any standard web browser or command-line tools like `curl` and `telnet`.

## Resources
*## Resources
**Networking & Sockets:**
* [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
* [IBM - Nonblocking I/O and Sockets](https://developer.ibm.com/articles/l-async/)
* [TCP Socket Programming & HTTP](https://www.youtube.com/watch?v=bdIiTxtMaKB)
* [Linux man-pages (epoll)](https://man7.org/linux/man-pages/man7/epoll.7.html)

**HTTP Protocol & CGI:**
* [MDN - HTTP Overview](https://developer.mozilla.org/en-US/docs/Web/HTTP)
* [MDN - HTTP Status Codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status)
* [CGI (Common Gateway Interface) Explained](https://en.wikipedia.org/wiki/Common_Gateway_Interface)

**RFCs (Official Specifications):**
* [RFC 9110 - HTTP Semantics](https://datatracker.ietf.org/doc/html/rfc9110)
* [RFC 9112 - HTTP/1.1](https://datatracker.ietf.org/doc/html/rfc9112)
* [RFC 3875 - CGI](https://datatracker.ietf.org/doc/html/rfc3875)
* [RFC 3986 - (URI) Generic Syntax](https://datatracker.ietf.org/doc/html/rfc3986)

**Testing & Tools:**
* [Postman](https://www.postman.com/): Send custom HTTP requests to the server.
* [Wireshark](https://www.wireshark.org/): Capture and analyze request/response traffic.
* [Siege](https://github.com/JoeDog/siege): Load testing to ensure server stability under pressure.
* [Nginx](https://nginx.org/en/docs/): Used as a reference for configuration file logic and behavior.

## AI Usage
During the development of this project, I used Gemini (AI Assistant) as an architectural sounding board and debugging assistant. Specifically, the AI helped with:

* **Debugging Complex Logic:** Discussing the safest way to implement a full-duplex non-blocking `epoll` loop (handling `EPOLLIN` and `EPOLLOUT` simultaneously) and placing map-guards to prevent segmentation faults during sudden client hangups.
* **Strict C++98 Compliance:** Ensuring the codebase strictly adheres to the C++98 standard. For instance, Gemini helped replace modern C++20 functions (like `std::string::starts_with()`) with compliant alternatives (`std::string::compare()`).
* **Enforcing Subject Rules:** Identifying and replacing forbidden system calls (such as swapping `pread()` with standard sequential `read()`) to guarantee the project met all 42 School requirements. All AI-assisted logic was heavily reviewed, modified, and integrated manually by the team.<!--  -->

# Testing
To ensure the server functions correctly and adheres to the project requirements, we have established a comprehensive set of manual test cases using `curl`.

### 1. Basic Functionality
Ensure the server is running: `./webserv configs/evaluation_basic.conf`
* **Homepage Access (Index):** `curl -v http://127.0.0.1:8080/`
* **Static File Access:** `curl -v http://127.0.0.1:8080/get.html`
* **404 Error Handling:** `curl -v http://127.0.0.1:8080/nonexistent`

### 2. Method Validation
Ensure the server is running: `./webserv configs/evaluation_methods.conf`
* **Testing Method Restriction (405):** `curl -X POST -v http://127.0.0.1:8093/only_get`
* **Testing Allowed Methods (201):** `curl -X POST -v http://127.0.0.1:8093/get_post`

### 3. CGI Execution
Ensure the server is running: `./webserv configs/evaluation_cgi.conf`
* **GET Request to CGI Script:** `curl -v http://127.0.0.1:8081/cgi-bin/test.py`
* **POST Request to CGI Script:** `curl -d "name=Tamara&status=Success" -v http://127.0.0.1:8081/cgi-bin/test.py`

### 4. Directory Listing & Index Handling
Ensure the server is running: `./webserv configs/evaluation_full.conf`
* **Autoindex Enabled:** `curl -v http://127.0.0.1:8080/auto_on/`
* **Autoindex Disabled (Serving Index File):** `curl -v http://127.0.0.1:8080/has_index/`
* **Autoindex Disabled (Security/No Index):** `curl -v http://127.0.0.1:8080/no_index/`

### 4. Advanced Features & Uploads
* **File Upload (`evaluation_upload.conf`):**
  ```bash
  echo "Hello World" > test_upload.txt
  curl -v -X POST -F "file=@test_upload.txt" http://127.0.0.1:8082/uploads
* **Max Body Size (413) (evaluation_413.conf):**
`curl -v -X POST -d "This string is definitely larger than the limit we set in our config file" http://127.0.0.1:8083/`

### 5. Configuration & Error Handling
* **Invalid Syntax (invalid.conf):**
`./webserv configs/invalid.conf`
Expectation: Detect syntax error, print a clear message, and exit safely.

* **Duplicate Server Name (duplicate_server_name.conf):**
`./webserv configs/duplicate_server_name.conf`
Expectation: Detect conflict (server_name/port) and report a warning or error.

### 6. Multi-Server Routing
* **Ensure the server is running: ./webserv configs/evaluation_multi.conf :**
`curl -H "Host: server1.com" http://127.0.0.1:8084/`
`curl -H "Host: server2.com" http://127.0.0.1:8085/`


# Testing with Siege (Load Testing)
Siege is used to simulate multiple concurrent users and verify that the server remains stable under high load without crashing or blocking.


## Running a Load Test
To test how your server handles 10 users simultaneously for 30 seconds, run the following command in your terminal:

```bash
siege -c 10 -t 30S http://127.0.0.1:8080/
