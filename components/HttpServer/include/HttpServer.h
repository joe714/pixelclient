#pragma once

#include <esp_http_server.h>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace httpd {

std::string urldecode(std::string_view in);

class Server;

class Request {
public:
    Request(httpd_req_t* req) : request_(req) {};
    httpd_req_t* request() const { return request_; }

    httpd_method_t method() const { return (httpd_method_t)request_->method; }
    std::string_view uri() const { return std::string_view(request_->uri); }
    bool isGet() const { return HTTP_GET == method(); }
    bool isPost() const { return HTTP_POST == method(); }

    size_t contentLength() const { return request_->content_len; }

    std::string requestHeader(const std::string& name) const;

    void setContentType(const std::string& type);

    void sendResponse(std::string_view data);
    void sendResponse(const char* data, ssize_t len);

    void sendError(httpd_err_code_t error);
    void sendError(httpd_err_code_t err, const std::string& msg);

private:
    httpd_req_t* request_;
};

class RequestHandler {
public:
    virtual ~RequestHandler() = default;
    virtual bool canHandle(httpd_method_t method) const = 0;
    virtual esp_err_t handle(httpd_req_t* req) = 0;
};

template <httpd_method_t M>
class MethodBase { };

#define DECLARE_METHOD(NAME, METHOD)                                           \
    template <>                                                                \
    class MethodBase<METHOD> {                                                 \
    public:                                                                    \
        static const httpd_method_t method = METHOD;                           \
        virtual esp_err_t on##NAME(Request&) = 0;                              \
                                                                               \
    protected:                                                                 \
        esp_err_t handle_(httpd_req_t* req)                                    \
        {                                                                      \
            Request r(req);                                                    \
            return on##NAME(r);                                                \
        }                                                                      \
    };                                                                         \
//    using NAME = MethodBase<METHOD>;

DECLARE_METHOD(Delete, HTTP_DELETE);
DECLARE_METHOD(Get, HTTP_GET);
DECLARE_METHOD(Post, HTTP_POST);
DECLARE_METHOD(Patch, HTTP_PATCH);

#undef DECLARE_METHOD

template <class...> class MethodNode
{
protected:
    bool canHandle_(httpd_method_t) const { return false; }
    esp_err_t handle_(httpd_req_t*) { return ESP_FAIL; }
};

template <class This, class... Rest>
class MethodNode<This, Rest...> : public This, public MethodNode<Rest...> {
private:
    using Next = MethodNode<Rest...>;
    
protected:
    bool canHandle_(httpd_method_t m) const {
        return This::method == m || Next::canHandle_(m);
    }

    esp_err_t handle_(httpd_req_t* req) {
        if (This::method == req->method) {
            return This::handle_(req);
        }
        return Next::handle_(req);
    }
};

template <httpd_method_t... Methods>
class MethodHandler : public RequestHandler, public MethodNode<MethodBase<Methods>...>
{
private:
    using Nodes = MethodNode<MethodBase<Methods>...>;

public:
    virtual bool canHandle(httpd_method_t method) const override{
        return Nodes::canHandle_(method);
    }

    virtual esp_err_t handle(httpd_req_t* req) override
    {
        return Nodes::handle_(req);
    }
};

class Server {
public:
    Server();
    ~Server();

    void registerHandler(const std::string& uri, RequestHandler& handler);

protected:
    httpd_handle_t server() const { return handle_; }

private:
    httpd_handle_t handle_;
    static esp_err_t dispatch(httpd_req_t* req);
};

} // namespace httpd
