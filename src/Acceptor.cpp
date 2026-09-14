#include "reactornet/Acceptor.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "reactornet/Channel.h"
#include "reactornet/EventLoop.h"

namespace {

// 建一个已 bind+listen 的非阻塞监听 socket
int createListeningSocket(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        std::perror("socket");
        std::exit(1);
    }
    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        std::exit(1);
    }
    if (::listen(fd, SOMAXCONN) < 0) {
        std::perror("listen");
        std::exit(1);
    }
    return fd;
}

}  // namespace

Acceptor::Acceptor(EventLoop* loop, uint16_t port)
    : loop_(loop),
      listenFd_(createListeningSocket(port)),
      acceptChannel_(loop_, listenFd_) {
    acceptChannel_.setReadCallback([this] { handleRead(); });

    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getsockname(listenFd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        boundPort_ = ntohs(addr.sin_port);
    }
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();   // 从 epoll 摘掉
    ::close(listenFd_);
}

void Acceptor::listen() {
    acceptChannel_.enableReading();   // 开始关心"有新连接"
}

void Acceptor::handleRead() {
    while (true) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        int connfd = ::accept4(listenFd_, reinterpret_cast<sockaddr*>(&peer), &len,
                               SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // 收完了
            if (errno == EINTR) continue;
            std::perror("accept4");
            break;
        }
        if (newConnCb_) newConnCb_(connfd);
    }
}
