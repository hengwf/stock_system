#include "datasource/sina_provider.h"
#include "cache/data_cache.h"
#include "server/api_handler.h"
#include "server/http_server.h"
#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>

using namespace quant;

static HttpServer* g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\nShutting down..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = 8888;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find("--port=") == 0) {
            port = std::stoi(arg.substr(7));
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto provider = std::make_shared<SinaProvider>();
    auto cache = std::make_shared<DataCache>();
    auto handler = std::make_shared<ApiHandler>(provider, cache);
    auto server = std::make_shared<HttpServer>(handler, port);
    g_server = server.get();

    if (!server->start()) {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return 1;
    }

    std::cout << "Quant stock selector server running on port " << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;

    while (server->isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Server stopped" << std::endl;
    return 0;
}
