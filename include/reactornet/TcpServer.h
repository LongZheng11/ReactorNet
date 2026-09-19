#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>

#include "reactornet/Acceptor.h"

class EventLoop;
class TcpConnection;

class TcpServer {
public:
    using MessageCallback = std::function<void(TcpConnection*, const std::string& msg)>;

    TcpServer(EventLoop* loop, uint16_t port);
    ~TcpServer();

    void setMessageCallback(MessageCallback cb) { messageCb_ = std::move(cb); }
    void start();
    uint16_t port() const { return acceptor_.port(); }

private:
    void onNewConnection(int connfd);
    void removeConnection(const std::shared_ptr<TcpConnection>& conn);

    EventLoop* loop_;
    Acceptor acceptor_;                       // 值成员 → 必须 include Acceptor.h
    std::map<int, std::shared_ptr<TcpConnection>> conns_;   // fd -> 连接
    MessageCallback messageCb_;
};
