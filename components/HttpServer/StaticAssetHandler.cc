#include "StaticAssetHandler.h"

namespace httpd {

StaticAssetHandler::StaticAssetHandler(const StaticAsset& asset,
                                       const std::string& contentType)
  : asset_(asset), contentType_(contentType)
{
}

StaticAssetHandler::StaticAssetHandler(const char* data,
                                       size_t len,
                                       const std::string& contentType)
  : StaticAssetHandler(StaticAsset{ data, len }, contentType)
{
}

esp_err_t
StaticAssetHandler::onGet(Request& req)
{
    req.setContentType(contentType_);
    req.sendResponse(asset_.data, asset_.len);
    return ESP_OK;
}
} // namespace httpd
