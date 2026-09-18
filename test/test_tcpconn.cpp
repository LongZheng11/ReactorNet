// TcpConnection：读事件 → 读空缓冲 → onMessage → send echo 回对端
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "reactornet/EventLoop.h"
#include "reactornet/TcpConnection.h"

bool readN(int fd, std::string* out, size_t n) {
    out->clear();
    char buf[4096];
    while (out->size() < n) {
        ssize_t r = ::read(fd, buf, sizeof(buf));
        if (r <= 0) return false;
        out->append(buf, static_cast<size_t>(r));
    }
    return out->size() == n;
}

int main() {
    EventLoop loop;
    int sv[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        std::perror("socketpair");
        return 1;
    }

    std::string received;
    // sv[0] 交给 TcpConnection 当作"已连接 socket"
    TcpConnection conn(&loop, sv[0]);
    conn.setMessageCallback([&](const std::string& msg) {
        received = msg;
        conn.send(msg);        // echo 回写
        loop.quit();
    });
    conn.enableReading();

    const char* msg = "hello conn";
    ::write(sv[1], msg, 10);   // 对端(测试)发数据
    loop.loop();               // 读到 → 回调 echo → quit

    if (received != msg) {
        std::printf("FAIL: received=%s expected=%s\n", received.c_str(), msg);
        return 1;
    }
    std::string back;
    if (!readN(sv[1], &back, 10) || back != msg) {
        std::printf("FAIL: echo back mismatch: %s\n", back.c_str());
        return 1;
    }
    std::printf("PASS: TcpConnection 读到数据并 echo 回对端\n");
    ::close(sv[1]);
    return 0;
}
