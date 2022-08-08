#pragma once

#include "HttpServer.h"

class RootPage : public httpd::MethodHandler<HTTP_GET, HTTP_POST>
{
public:
    virtual esp_err_t onGet(httpd::Request& req) override;
    virtual esp_err_t onPost(httpd::Request& req) override;
};

