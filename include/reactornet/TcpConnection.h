#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "reactornet/Channel.h"

class EventLoop;

// 一条已建立连接的读写。拥有 Channel 与 socket fd。
class TcpConnection {
public:
    using MessageCallback = std::function<void(const std::string& msg)>;
    using CloseCallback = std::function<void()>;

    TcpConnection(EventLoop* loop, int connfd);
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    void setMessageCallback(MessageCallback cb) { messageCb_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb)     { closeCb_ = std::move(cb); }

    void enableReading();
    void send(const std::string& data);   // 应用层调它发数据（可能只发一部分，其余进写缓冲）

    int fd() const { return connfd_; }

private:
    void handleRead();
    void handleWrite();
    void handleClose();

    EventLoop* loop_;
    const int connfd_;
    Channel channel_;             // 值成员 → 必须 include Channel.h
    std::string inputBuffer_;     // 从 socket 读到的原始字节
    std::string outputBuffer_;    // 一次没发完、要等可写事件再发的字节
    MessageCallback messageCb_;
    CloseCallback closeCb_;
};
