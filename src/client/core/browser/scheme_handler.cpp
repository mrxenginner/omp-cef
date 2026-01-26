#include "scheme_handler.hpp"
#include "system/resource_manager.hpp"
#include "include/wrapper/cef_helpers.h"
#include "include/cef_parser.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <cstring>

LocalSchemeHandlerFactory::LocalSchemeHandlerFactory(ResourceManager& resource_manager)
    : resource_manager_(resource_manager)
{
}

CefRefPtr<CefResourceHandler> LocalSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& scheme_name,
    CefRefPtr<CefRequest> request)
{
    CEF_REQUIRE_IO_THREAD();
    return new LocalResourceHandler(resource_manager_);
}

static const std::unordered_map<std::string, std::string>& GetMimeTypeMap()
{
    static const std::unordered_map<std::string, std::string> mime_map = {
        // Web content types
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".mjs", "application/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},

        // Image types
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".webp", "image/webp"},
        {".bmp", "image/bmp"},

        // Audio types
        {".mp3", "audio/mpeg"},
        {".ogg", "audio/ogg"},
        {".wav", "audio/wav"},
        {".m4a", "audio/mp4"},
        {".aac", "audio/aac"},
        {".flac", "audio/flac"},

        // Video types
        {".mp4", "video/mp4"},
        {".webm", "video/webm"},
        {".ogv", "video/ogg"},
        {".avi", "video/x-msvideo"},

        // Font types
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        {".eot", "application/vnd.ms-fontobject"},

        // Document/data types
        {".pdf", "application/pdf"},
        {".txt", "text/plain"},
        {".md", "text/markdown"},
    };

    return mime_map;
}

// Optimized MIME type resolver using cache
std::string GetMimeType(const std::string& path)
{
    size_t pos = path.rfind('.');
    if (pos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return std::tolower(c); });

    const auto& mime_map = GetMimeTypeMap();
    auto it = mime_map.find(ext);
    if (it != mime_map.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

LocalResourceHandler::LocalResourceHandler(ResourceManager& resource_manager) 
    : resource_manager_(resource_manager), read_offset_(0) 
{
}

bool LocalResourceHandler::ProcessRequest(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback)
{
    CEF_REQUIRE_IO_THREAD();

    std::string url = request->GetURL();
    std::string path_part;
    
    if (url.rfind("cef://", 0) == 0) {
        path_part = url.substr(6);
    }
    else if (url.rfind("http://cef/", 0) == 0) {
        path_part = url.substr(11);
    }
    else {
        LOG_DEBUG("[CEF] Unsupported URL scheme: {}", url);
        return false;
    }

    // Strip query parameters and fragments for file resolution
    size_t query_pos = path_part.find('?');
    if (query_pos != std::string::npos) {
        path_part = path_part.substr(0, query_pos);
    }

    size_t fragment_pos = path_part.find('#');
    if (fragment_pos != std::string::npos) {
        path_part = path_part.substr(0, fragment_pos);
    }

    size_t first_slash_pos = path_part.find('/');
    if (first_slash_pos == std::string::npos) {
        LOG_DEBUG("[CEF] Invalid URL format: {}", url);
        return false;
    }

    std::string resource_name = path_part.substr(0, first_slash_pos);
    std::string internal_path = path_part.substr(first_slash_pos + 1);

    // Block files without an extension
    size_t pos = internal_path.rfind('.');
    if (pos == std::string::npos) {
        LOG_WARN("[CEF] Blocked request for file with no extension: {}", internal_path);
        return false;
    }

    std::string ext = internal_path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { 
        return std::tolower(c); 
    });

    // Block the request if the extension is not in the allowed types.
    const auto& mime_map = GetMimeTypeMap();
    if (mime_map.find(ext) == mime_map.end()) {
        LOG_WARN("[CEF] Blocked request for unallowed file type ({}): {}", ext, internal_path);
        return false; 
    }

    // URL decode the path (handle spaces and special characters)
    CefString decoded = CefURIDecode(
        internal_path, 
        true, 
        static_cast<cef_uri_unescape_rule_t>(UU_SPACES | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS)
    );

    internal_path = decoded.ToString();

    // Normalize path and prevent directory traversal
    std::replace(internal_path.begin(), internal_path.end(), '\\', '/');
    if (internal_path.find("..") != std::string::npos ||
        internal_path.find("~") != std::string::npos) {
        LOG_DEBUG("[CEF] Path traversal attempt blocked: {}", internal_path);
        return false;
    }

    // Attempt to load file content from ResourceManager
    if (resource_manager_.GetFileContent(resource_name, internal_path, data_)) {
        mime_type_ = GetMimeType(internal_path);
        read_offset_ = 0;

        LOG_DEBUG("[CEF] Successfully loaded: {} from resource '{}'", internal_path, resource_name);
        callback->Continue();
        return true;
    }

    LOG_DEBUG("[CEF] Resource not found: {}/{}", resource_name, internal_path);
    return false;
}

void LocalResourceHandler::GetResponseHeaders(
    CefRefPtr<CefResponse> response,
    int64_t& response_length,
    CefString& redirectUrl)
{
    CEF_REQUIRE_IO_THREAD();

    response->SetMimeType(mime_type_);
    response->SetStatus(200);
    response_length = static_cast<int64_t>(data_.size());
}

bool LocalResourceHandler::ReadResponse(
    void* data_out,
    int bytes_to_read,
    int& bytes_read,
    CefRefPtr<CefCallback> callback)
{
    CEF_REQUIRE_IO_THREAD();

    size_t remaining_bytes = data_.size() - read_offset_;
    if (remaining_bytes == 0) {
        bytes_read = 0;
        return false;
    }

    size_t bytes_to_copy = std::min(static_cast<size_t>(bytes_to_read), remaining_bytes);
    std::memcpy(data_out, data_.data() + read_offset_, bytes_to_copy);

    read_offset_ += bytes_to_copy;
    bytes_read = static_cast<int>(bytes_to_copy);

    return true;
}

void LocalResourceHandler::Cancel()
{
    CEF_REQUIRE_IO_THREAD();

    data_.clear();
    data_.shrink_to_fit();
    read_offset_ = 0;
}