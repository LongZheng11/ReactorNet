// epoll 冒烟测试：验证 WSL 里 epoll 全链路可用
// socketpair 创建管道，epoll 监听一端，写入数据后 epoll_wait 应捕获可读事件
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>

int main() {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair");
        return 1;
    }
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        return 1;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = sv[1];
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sv[1], &ev) != 0) {
        perror("epoll_ctl");
        return 1;
    }
    const char msg[] = "hello epoll";
    write(sv[0], msg, sizeof(msg));

    epoll_event ready[4];
    int n = epoll_wait(epfd, ready, 4, 1000);
    if (n < 0) {
        perror("epoll_wait");
        return 1;
    }
    if (n == 0) {
        printf("FAIL: epoll_wait 超时，没有捕获到事件\n");
        return 1;
    }
    char buf[64]{};
    ssize_t r = read(ready[0].data.fd, buf, sizeof(buf) - 1);
    printf("PASS: epoll_wait 捕获 %d 个事件, 读到 %zd 字节: %s\n", n, r, buf);
    return 0;
}
