// 全链路 echo：TcpServer(accept→read→onMessage→send) + 真实客户端
// 外加一条 fd 泄漏断言——它是唯一能抓住「shared_ptr 自引用环」的测试
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "reactornet/EventLoop.h"
#include "reactornet/TcpConnection.h"
#include "reactornet/TcpServer.h"

namespace {

int g_failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++g_failures;
}

int dial(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

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

// 数一数本进程当前开着几个 fd —— 泄漏一个连接就是泄漏一个 fd
int countFds() {
    DIR* d = ::opendir("/proc/self/fd");
    if (d == nullptr) return -1;
    int n = 0;
    while (dirent* e = ::readdir(d)) {
        if (e->d_name[0] != '.') ++n;   // 跳过 "." 和 ".."
    }
    ::closedir(d);
    return n;
}

}  // namespace

int main() {
    int client = -1;
    int fds_before = -1;

    {
        EventLoop loop;
        TcpServer server(&loop, 0);   // 端口 0 → 系统分配
        std::string echoed;
        server.setMessageCallback([&](TcpConnection* conn, const std::string& msg) {
            echoed = msg;
            conn->send(msg);          // 原样回
            loop.quit();
        });
        server.start();
        check(server.port() != 0, "TcpServer 拿到实际端口");

        client = dial(server.port());
        check(client >= 0, "客户端连上服务器");
        if (client < 0) return 1;

        fds_before = countFds();   // 服务端还没 accept：此刻 = epfd + listen fd + client fd

        const char* msg = "hello reactor";
        ::write(client, msg, 13);
        loop.loop();   // accept → 可读 → onMessage → echo → quit

        check(echoed == msg, "服务端收到并回显原始消息");
        std::string back;
        check(readN(client, &back, 13), "客户端读回 13 字节");
        check(back == msg, "客户端收到的等于发出去的（echo 闭环）");
    }   // ← ~TcpServer 在这里发生：它必须先拆掉 shared_ptr 自引用环，
        //    否则连接对象永不析构 → 它的 fd 永不关闭

    // fds_before 取样时 epfd + listenfd 都还开着，块退出时这两个随各自析构正常关闭；
    // 而 clientfd 仍然开着（下面才 close）。所以正确期望是 fds_before - 2：
    //   -2 = epfd、listenfd 正常关；connfd 也关 = 无泄漏
    // 若自引用环没拆，connfd 不会关 → 结果变成 fds_before - 1 ≠ fds_before - 2 → 这条 FAIL
    const int fds_after = countFds();
    check(fds_after == fds_before - 2,
          "TcpServer 析构后连接已释放（connfd 已关 → 证明环已拆）");

    if (client >= 0) ::close(client);

    if (g_failures == 0) {
        std::printf("PASS: TcpServer echo 全链路 + 连接无泄漏\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
