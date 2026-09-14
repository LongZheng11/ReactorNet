// Acceptor：listen fd 可读 → 循环 accept → 回调收到新连接 fd
#include <arpa/inet.h>
#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "reactornet/Acceptor.h"
#include "reactornet/EventLoop.h"

int main() {
    EventLoop loop;
    Acceptor acceptor(&loop, 0);   // port 0 → 系统分配
    acceptor.setNewConnectionCallback([&](int connfd) {
        std::printf("accepted connfd=%d\n", connfd);
        ::close(connfd);           // 测试里直接关掉即可
        loop.quit();
    });
    acceptor.listen();
    std::printf("listening on port %u\n", acceptor.port());
    if (acceptor.port() == 0) {
        std::printf("FAIL: port should be assigned\n");
        return 1;
    }

    // 客户端连一下（连接完成进入 backlog，无需等 accept）
    int client = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(acceptor.port());
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    int rc = ::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc != 0) {
        std::perror("connect");
        return 1;
    }

    loop.loop();   // accept 事件一到就回调并 quit
    ::close(client);
    std::printf("PASS: Acceptor 收到新连接\n");
    return 0;
}
