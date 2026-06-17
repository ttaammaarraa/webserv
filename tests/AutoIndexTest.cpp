#include "AutoIndexGenerator.hpp"
#include "HttpRequest.hpp"
#include "ResponseBuilder.hpp"
#include "Server.hpp"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static std::string makeUniqueDir(const std::string& prefix)
{
    std::ostringstream oss;
    oss << "/tmp/" << prefix << "_" << getpid();
    return oss.str();
}

static void makeDir(const std::string& path)
{
    int rc = mkdir(path.c_str(), 0755);
    assert(rc == 0);
}

static void writeFile(const std::string& path, const std::string& content)
{
    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    assert(fd >= 0);
    ssize_t written = write(fd, content.c_str(), content.size());
    assert(written == static_cast<ssize_t>(content.size()));
    close(fd);
}

static void removeTree(const std::string& path)
{
    DIR* dir = opendir(path.c_str());
    if (!dir)
    {
        remove(path.c_str());
        return;
    }

    struct dirent* entry = NULL;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        std::string child = path + "/" + name;
        struct stat st;
        if (stat(child.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            removeTree(child);
        else
            remove(child.c_str());
    }
    closedir(dir);
    rmdir(path.c_str());
}

static void test_generator_renders_directory_listing()
{
    std::string root = makeUniqueDir("webserv_autoindex_gen");
    makeDir(root);
    makeDir(root + "/subdir");
    writeFile(root + "/alpha.txt", "alpha");
    writeFile(root + "/a<b>&c\"d'e.txt", "special");

    std::string html = AutoIndexGenerator::generate(root, "/demo");

    assert(html.find("Index of /demo/") != std::string::npos);
    assert(html.find("alpha.txt") != std::string::npos);
    assert(html.find("subdir/") != std::string::npos);
    assert(html.find("a&lt;b&gt;&amp;c&quot;d&#39;e.txt") != std::string::npos);
    assert(html.find("<li><a href=\"../\">..</a></li>") != std::string::npos);

    removeTree(root);
}

static void test_response_builder_autoindex_integration()
{
    std::string root = makeUniqueDir("webserv_autoindex_resp");
    std::string folder = root + "/docs";

    makeDir(root);
    makeDir(folder);
    writeFile(folder + "/readme.txt", "readme");

    ServerConfig conf;
    conf.root = root;

    Location loc("/");
    loc.autoindex = true;
    conf.locations.push_back(loc);

    Connection conn;
    conn.serverConfig = &conf;

    HttpRequest req = HttpRequest::parse("GET /docs/ HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string response = ResponseBuilder::handle(&conn, req);

    assert(response.find("HTTP/1.1 200 OK") != std::string::npos);
    assert(response.find("Index of /docs/") != std::string::npos);
    assert(response.find("readme.txt") != std::string::npos);
    assert(conn.file_fd == -1);

    removeTree(root);
}

static void test_post_file_upload()
{
    std::string root = makeUniqueDir("webserv_post_test");
    makeDir(root);

    ServerConfig conf;
    conf.root = root;

    Location loc("/uploads");
    loc.upload_path = root + "/uploads";
    conf.locations.push_back(loc);
    makeDir(loc.upload_path);

    Connection conn;
    conn.serverConfig = &conf;

    std::string body = "Hello, this is uploaded content!";
    std::string rawReq = "POST /uploads/myfile.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: " 
                        + std::string(body.size() > 0 ? "31" : "0") + "\r\n\r\n" + body;
    HttpRequest req = HttpRequest::parse(rawReq);
    std::string response = ResponseBuilder::handle(&conn, req);

    assert(response.find("HTTP/1.1 201 Created") != std::string::npos);

    struct stat st;
    std::string uploadedFile = root + "/uploads/myfile.txt";
    assert(stat(uploadedFile.c_str(), &st) == 0);

    removeTree(root);
}

static void test_delete_file()
{
    std::string root = makeUniqueDir("webserv_delete_test");
    makeDir(root);
    writeFile(root + "/deleteme.txt", "content to delete");

    ServerConfig conf;
    conf.root = root;

    Connection conn;
    conn.serverConfig = &conf;

    HttpRequest req = HttpRequest::parse("DELETE /deleteme.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string response = ResponseBuilder::handle(&conn, req);

    assert(response.find("HTTP/1.1 204 No Content") != std::string::npos);

    struct stat st;
    std::string deletedFile = root + "/deleteme.txt";
    assert(stat(deletedFile.c_str(), &st) != 0);

    removeTree(root);
}

static void test_delete_nonexistent_file()
{
    std::string root = makeUniqueDir("webserv_delete_404_test");
    makeDir(root);

    ServerConfig conf;
    conf.root = root;

    Connection conn;
    conn.serverConfig = &conf;

    HttpRequest req = HttpRequest::parse("DELETE /doesnotexist.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string response = ResponseBuilder::handle(&conn, req);

    assert(response.find("HTTP/1.1 404 Not Found") != std::string::npos);

    removeTree(root);
}

static void test_delete_directory_forbidden()
{
    std::string root = makeUniqueDir("webserv_delete_dir_test");
    makeDir(root);
    makeDir(root + "/folder");

    ServerConfig conf;
    conf.root = root;

    Connection conn;
    conn.serverConfig = &conf;

    HttpRequest req = HttpRequest::parse("DELETE /folder HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string response = ResponseBuilder::handle(&conn, req);

    assert(response.find("HTTP/1.1 403 Forbidden") != std::string::npos);

    removeTree(root);
}

int main()
{
    test_generator_renders_directory_listing();
    test_response_builder_autoindex_integration();
    test_post_file_upload();
    test_delete_file();
    test_delete_nonexistent_file();
    test_delete_directory_forbidden();

    std::cout << "All autoindex, POST, and DELETE tests passed." << std::endl;
    return 0;
}
