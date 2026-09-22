// 全链路 echo：TcpServer(accept→read→onMessage→send) + 真实客户端
// 外加 fd 泄漏断言——它是唯一能抓住「shared_ptr 自引用环」的测试
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
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

// 运行期回收：客户端断开后，连接应该在 loop 运行期间就被释放，
// 而不是一直拖着、等 ~TcpServer 的兜底拆环才释放。
void checkRuntimeReclaim() {
    int a = -1;
    int bConnFd = -1;
    bool sawA = false;

    {
        EventLoop loop;
        TcpServer server(&loop, 0);
        server.setMessageCallback([&](TcpConnection* conn, const std::string& msg) {
            conn->send(msg);                    // 原样回
            if (msg == "gone") {
                bConnFd = conn->fd();           // B 的服务端侧 fd，稍后验它已释放
                // quit 也排进队：它和 removeConnection 排入的删除任务会在同一批
                // runPendingFunctors 里执行（quit 只影响下一轮 while 判断）。
                loop.queueInLoop([&] { loop.quit(); });
            } else {
                sawA = true;
            }
        });
        server.start();

        a = dial(server.port());
        int b = dial(server.port());
        check(a >= 0 && b >= 0, "两条客户端都连上（A 保持，B 立刻断开）");
        if (a < 0 || b < 0) return;

        // 两条 connect 都发生在 loop 之前 → 第 1 轮 accept 掉两条，
        // 第 2 轮同时收到它们的数据，顺序无关。
        ::write(b, "gone", 4);
        ::close(b);            // 数据 + FIN 一起；loopback 上服务端一次读到两者
        ::write(a, "keep", 4);

        loop.loop();

        // ↓↓↓ server 此刻还活着：~TcpServer 的兜底拆环尚未发生。
        //     所以 B 的 fd 若已经关了，只可能是「运行期回收」干的。
        check(bConnFd != -1, "B 的消息被处理（拿到服务端侧 fd）");
        check(bConnFd != -1 && ::fcntl(bConnFd, F_GETFD) == -1,
              "B 的连接在 loop 运行期就被回收（不是等 ~TcpServer 兜底）");

        std::string back;
        check(readN(a, &back, 4) && back == "keep",
              "A 的连接仍活着、echo 闭环正常");
    }   // ← ~TcpServer 在这里才发生（回收 A）

    if (a >= 0) ::close(a);
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

    checkRuntimeReclaim();

    if (g_failures == 0) {
        std::printf("PASS: TcpServer echo 全链路 + 连接无泄漏\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
