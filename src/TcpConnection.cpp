#include "reactornet/TcpConnection.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include "reactornet/EventLoop.h"

namespace {
void setNonBlocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
}  // namespace

TcpConnection::TcpConnection(EventLoop* loop, int connfd)
    : loop_(loop), connfd_(connfd), channel_(loop_, connfd_) {
    setNonBlocking(connfd_);
    channel_.setReadCallback([this] { handleRead(); });
    channel_.setWriteCallback([this] { handleWrite(); });
}

TcpConnection::~TcpConnection() {
    channel_.disableAll();
    ::close(connfd_);
}

void TcpConnection::enableReading() {
    channel_.enableReading();
}

void TcpConnection::handleRead() {
    char buf[65536];
    bool closed = false;
    while (true) {
        ssize_t n = ::read(connfd_, buf, sizeof(buf));
        if (n > 0) {
            inputBuffer_.append(buf, static_cast<size_t>(n));
            continue;               // 内核里可能还有，继续读到 EAGAIN
        }
        if (n == 0) { closed = true; break; }   // 对端优雅关闭
        if (errno == EINTR) continue;           // 被信号打断，重试
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // 读完了
        closed = true;              // 真错误，按关闭处理
        break;
    }

    if (!inputBuffer_.empty() && messageCb_) {
        std::string msg;
        msg.swap(inputBuffer_);
        messageCb_(msg);
    }
    if (closed) handleClose();
}

void TcpConnection::handleWrite() {
    while (!outputBuffer_.empty()) {
        ssize_t n = ::write(connfd_, outputBuffer_.data(), outputBuffer_.size());
        if (n > 0) {
            outputBuffer_.erase(0, static_cast<size_t>(n));
            continue;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;   // 内核缓冲满，等可写事件
        handleClose();   // 真错误
        return;
    }
    if (outputBuffer_.empty()) channel_.disableWriting();
}

void TcpConnection::send(const std::string& data) {
    outputBuffer_ += data;
    channel_.enableWriting();   // 至少尝试可写；handleWrite 发完会自己关掉
    handleWrite();
}

void TcpConnection::handleClose() {
    channel_.disableAll();   // 先摘掉，不再收任何事件
    if (closeCb_) closeCb_();
}
