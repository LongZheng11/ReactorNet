// Echo 服务器：收到的每条消息原样发回。验证：echo hi | nc -N 127.0.0.1 8888
#include <cstdio>
#include <string>

#include "reactornet/EventLoop.h"
#include "reactornet/TcpConnection.h"
#include "reactornet/TcpServer.h"

int main() {
    EventLoop loop;
    TcpServer server(&loop, 8888);
    server.setMessageCallback([](TcpConnection* conn, const std::string& msg) {
        std::printf("recv %zu bytes, echo back\n", msg.size());
        std::fflush(stdout);
        conn->send(msg);
    });
    server.start();
    std::printf("echo server on port %u\n", server.port());
    loop.loop();
    return 0;
}
