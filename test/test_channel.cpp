// EventLoop + Channel 冒烟：socketpair 单线程自闭环
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "reactornet/Channel.h"
#include "reactornet/EventLoop.h"

int main() {
    EventLoop loop;
    int sv[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        std::perror("socketpair");
        return 1;
    }

    std::string got;
    Channel ch(&loop, sv[0]);
    ch.setReadCallback([&] {
        char buf[32];
        ssize_t n = ::read(sv[0], buf, sizeof(buf));
        if (n > 0) got.assign(buf, static_cast<size_t>(n));
        loop.quit();
    });
    ch.enableReading();

    const char* msg = "ping";
    ::write(sv[1], msg, 4);
    loop.loop();   // 事件一到，readCallback 触发并 quit

    if (got != msg) {
        std::printf("FAIL: got=%s expected=%s\n", got.c_str(), msg);
        return 1;
    }
    std::printf("PASS: EventLoop+Channel 读到一个事件\n");
    ::close(sv[0]);
    ::close(sv[1]);
    return 0;
}
