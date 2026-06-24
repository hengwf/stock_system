#pragma once

#include "server/api_handler.h"
#include <memory>
#include <string>

namespace quant {

class HttpServer {
public:
    HttpServer(std::shared_ptr<ApiHandler> handler, int port = 8888);
    ~HttpServer();

    bool start();
    void stop();
    bool isRunning() const { return running_; }

private:
    void run();
    std::string handleRequest(const std::string& method,
                              const std::string& path,
                              const std::string& query,
                              const std::string& body);

    std::string parseQueryParam(const std::string& query, const std::string& key);
    std::string urlDecode(const std::string& str);

    std::shared_ptr<ApiHandler> handler_;
    int port_;
    bool running_ = false;
    void* server_handle_ = nullptr;
};

}
