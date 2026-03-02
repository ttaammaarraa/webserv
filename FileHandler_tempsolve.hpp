#ifndef FILEHANDLER_TEMPSOLVE_HPP
#define FILEHANDLER_TEMPSOLVE_HPP

#include <string>
#include <vector>
#include <map>

class FileHandler_tempsolve {
public:
    // Check if file exists and is accessible
    static bool fileExists(const std::string& path);

    // Read file content into buffer
    static bool readFile(const std::string& path, std::vector<unsigned char>& buffer);

    // Get hardcoded error page for 404
    static std::string get404Page();

    // Get MIME type from file extension
    static std::string getMimeType(const std::string& path);

    // Construct a full HTTP response for a file
    static std::string constructHttpResponse(const std::string& filePath);
};

#endif // FILEHANDLER_TEMPSOLVE_HPP
