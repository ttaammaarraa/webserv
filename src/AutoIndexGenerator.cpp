#include "AutoIndexGenerator.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <sstream>
#include <vector>

std::string AutoIndexGenerator::joinPath(const std::string& base, const std::string& suffix)
{
    if (base.empty())
        return suffix;
    if (suffix.empty())
        return base;
    if (base[base.size() - 1] == '/' && suffix[0] == '/')
        return base + suffix.substr(1);
    if (base[base.size() - 1] != '/' && suffix[0] != '/')
        return base + "/" + suffix;
    return base + suffix;
}

std::string AutoIndexGenerator::escapeHtml(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i)
    {
        switch (value[i])
        {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += value[i]; break;
        }
    }

    return escaped;
}

std::string AutoIndexGenerator::generate(const std::string& dirPath, const std::string& requestPath)
{
    DIR* dir = opendir(dirPath.c_str());
    if (!dir)
        return "";

    std::vector<std::string> entries;
    for (struct dirent* entry = readdir(dir); entry != NULL; entry = readdir(dir))
        entries.push_back(entry->d_name);
    closedir(dir);

    std::sort(entries.begin(), entries.end());

    std::string normalizedPath = requestPath.empty() ? "/" : requestPath;
    if (normalizedPath[normalizedPath.size() - 1] != '/')
        normalizedPath += "/";

    std::ostringstream body;
    body << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Index of "
         << escapeHtml(normalizedPath)
         << "</title></head><body><h1>Index of "
         << escapeHtml(normalizedPath)
         << "</h1><ul>";

    if (normalizedPath != "/")
        body << "<li><a href=\"../\">..</a></li>";

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const std::string& name = entries[i];
        if (name == "." || name == "..")
            continue;

        std::string href = normalizedPath + name;

        struct stat entryStat;
        const std::string fullPath = joinPath(dirPath, name);
        if (stat(fullPath.c_str(), &entryStat) == 0 && S_ISDIR(entryStat.st_mode))
            href += "/";

        body << "<li><a href=\"" << escapeHtml(href) << "\">"
             << escapeHtml(name)
             << "</a></li>";
    }

    body << "</ul></body></html>";
    return body.str();
}
