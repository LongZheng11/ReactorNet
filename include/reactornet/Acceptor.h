#pragma once

#include <cstdint>
#include <functional>

#include "reactornet/Channel.h"

class EventLoop;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int connfd)>;

    Acceptor(EventLoop* loop, uint16_t port);   // port 可为 0 表示让系统分配
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb) { newConnCb_ = std::move(cb); }
    uint16_t port() const { return boundPort_; }
    void listen();   // 开始监听：向 EventLoop 注册可读

private:
    void handleRead();

    EventLoop* loop_;
    int listenFd_;
    uint16_t boundPort_ = 0;
    Channel acceptChannel_;          // 值成员 → 必须 include Channel.h
    NewConnectionCallback newConnCb_;
};
