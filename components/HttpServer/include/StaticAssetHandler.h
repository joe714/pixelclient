#pragma once

#include "HttpServer.h"

namespace httpd {

struct StaticAsset {
    const char* data;
    size_t len;
};

// Initialize a StaticAsset from a file embedded in the build. The ASM address
// lookup must be a statement, so this abuses a lambda to scope it and return.
#define EmbeddedAsset(name__)                                                  \
    [] {                                                                       \
        using httpd::StaticAsset;                                              \
        extern const char start__[] asm("_binary_" #name__ "_start");          \
        extern size_t len__ asm(#name__ "_length");                            \
        return StaticAsset{ start__, len__ };                                  \
    }()

class StaticAssetHandler : public MethodHandler<HTTP_GET>
{
public:
    StaticAssetHandler(const StaticAsset& asset, const std::string& contentType = "text/html");
    StaticAssetHandler(const char* data, size_t len, const std::string& contentType = "text/html");

    esp_err_t onGet(httpd::Request& req);

private:
    StaticAsset asset_;
    std::string contentType_;
};

} // namespace httpd
