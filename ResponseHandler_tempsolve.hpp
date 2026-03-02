#ifndef RESPONSEHANDLER_TEMPSOLVE_HPP
#define RESPONSEHANDLER_TEMPSOLVE_HPP

#include <string>
#include <vector>
#include <map>

class ResponseHandler_tempsolve {
public:
    ResponseHandler_tempsolve();
    ~ResponseHandler_tempsolve();

    // Validate file existence and permissions
    static int validateFile(const std::string& path);

    // Read file content into buffer (binary safe)
    static bool readFile(const std::string& path, std::vector<unsigned char>& buffer);

    // Get hardcoded error page for 404, 403, 500
    static std::string getErrorPage(int code);

    // Get MIME type from file extension
    static std::string getMimeType(const std::string& path);

    // Construct a full HTTP response for a file
    static std::vector<unsigned char> constructHttpResponse(const std::string& filePath);

    // Sanity check for file path and headers
    static bool sanityCheck(const std::string& filePath);
};

#endif // RESPONSEHANDLER_TEMPSOLVE_HPP
