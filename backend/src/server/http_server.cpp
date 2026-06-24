#include "server/http_server.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

namespace quant {

HttpServer::HttpServer(std::shared_ptr<ApiHandler> handler, int port)
    : handler_(std::move(handler)), port_(port) {}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port_);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed on port " << port_ << std::endl;
        closesocket(server_fd);
        return false;
    }

    if (listen(server_fd, 16) < 0) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(server_fd);
        return false;
    }

    running_ = true;
    server_handle_ = (void*)(intptr_t)server_fd;

    std::cout << "Quant server started on http://127.0.0.1:" << port_ << std::endl;

    std::thread([this, server_fd]() {
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            SOCKET client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd == INVALID_SOCKET) {
                if (!running_) break;
                continue;
            }

            char buffer[65536];
            int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                std::string request(buffer);

                std::string method, path, query, body;
                size_t method_end = request.find(' ');
                if (method_end != std::string::npos) {
                    method = request.substr(0, method_end);
                    size_t path_start = method_end + 1;
                    size_t path_end = request.find(' ', path_start);
                    if (path_end != std::string::npos) {
                        std::string full_path = request.substr(path_start, path_end - path_start);
                        size_t query_pos = full_path.find('?');
                        if (query_pos != std::string::npos) {
                            path = full_path.substr(0, query_pos);
                            query = full_path.substr(query_pos + 1);
                        } else {
                            path = full_path;
                        }
                    }

                    size_t body_pos = request.find("\r\n\r\n");
                    if (body_pos != std::string::npos) {
                        body = request.substr(body_pos + 4);
                    }
                }

                std::string response_body = handleRequest(method, path, query, body);

                std::ostringstream resp;
                resp << "HTTP/1.1 200 OK\r\n";
                resp << "Content-Type: application/json; charset=utf-8\r\n";
                resp << "Access-Control-Allow-Origin: *\r\n";
                resp << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
                resp << "Access-Control-Allow-Headers: Content-Type\r\n";
                resp << "Content-Length: " << response_body.size() << "\r\n";
                resp << "Connection: close\r\n";
                resp << "\r\n";
                resp << response_body;

                std::string resp_str = resp.str();
                send(client_fd, resp_str.c_str(), (int)resp_str.size(), 0);
            }

            closesocket(client_fd);
        }
    }).detach();

    return true;
}

void HttpServer::stop() {
    if (!running_) return;
    running_ = false;
    if (server_handle_) {
        SOCKET fd = (SOCKET)(intptr_t)server_handle_;
        closesocket(fd);
        server_handle_ = nullptr;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

std::string HttpServer::handleRequest(const std::string& method,
                                      const std::string& path,
                                      const std::string& query,
                                      const std::string& body) {
    if (method == "OPTIONS") {
        return "{}";
    }

    if (path == "/api/health") {
        return handler_->handleHealth();
    }

    if (path == "/api/market/list") {
        return handler_->handleMarketList(query);
    }

    if (path == "/api/stock/realtime") {
        std::string codes = parseQueryParam(query, "codes");
        return handler_->handleStockRealtime(codes);
    }

    if (path == "/api/stock/detail") {
        std::string code = parseQueryParam(query, "code");
        return handler_->handleStockDetail(code);
    }

    if (path == "/api/stock/kline") {
        std::string code = parseQueryParam(query, "code");
        std::string type = parseQueryParam(query, "type");
        std::string count_str = parseQueryParam(query, "count");
        int count = count_str.empty() ? 200 : std::stoi(count_str);
        return handler_->handleKline(code, type, count);
    }

    if (path == "/api/stock/fund_flow") {
        std::string code = parseQueryParam(query, "code");
        return handler_->handleFundFlow(code);
    }

    if (path == "/api/selector/run" && method == "POST") {
        return handler_->handleRunSelector(body);
    }

    if (path == "/api/selector/result") {
        std::string period = parseQueryParam(query, "period");
        return handler_->handleSelectorResult(period);
    }

    std::ostringstream err;
    err << "{\"code\":404,\"message\":\"Not found: " << path << "\",\"data\":null}";
    return err.str();
}

std::string HttpServer::parseQueryParam(const std::string& query, const std::string& key) {
    if (query.empty()) return "";
    size_t pos = 0;
    while (pos < query.size()) {
        size_t amp = query.find('&', pos);
        std::string pair = (amp == std::string::npos) ? query.substr(pos) : query.substr(pos, amp - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string k = pair.substr(0, eq);
            std::string v = pair.substr(eq + 1);
            if (k == key) {
                return urlDecode(v);
            }
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return "";
}

std::string HttpServer::urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int val = 0;
            char hex[3] = { str[i+1], str[i+2], '\0' };
            val = std::strtol(hex, nullptr, 16);
            result += (char)val;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

}
