#include "reactornet/TcpServer.h"

#include "reactornet/EventLoop.h"
#include "reactornet/TcpConnection.h"

TcpServer::TcpServer(EventLoop* loop, uint16_t port)
    : loop_(loop), acceptor_(loop, port) {
    acceptor_.setNewConnectionCallback([this](int connfd) { onNewConnection(connfd); });
}

TcpServer::~TcpServer() {
    // 必须先拆环，conns_.clear() 才会真的析构连接（见下方教学点二）
    for (auto& kv : conns_) {
        kv.second->setMessageCallback(nullptr);
        kv.second->setCloseCallback(nullptr);
    }
    conns_.clear();
}

void TcpServer::start() {
    acceptor_.listen();
}

void TcpServer::onNewConnection(int connfd) {
    auto conn = std::make_shared<TcpConnection>(loop_, connfd);
    conns_[connfd] = conn;
    conn->setMessageCallback([this, conn](const std::string& msg) {
        if (messageCb_) messageCb_(conn.get(), msg);   // 把连接交给用户层
    });
    conn->setCloseCallback([this, conn] {
        removeConnection(conn);
    });
    conn->enableReading();
}

void TcpServer::removeConnection(const std::shared_ptr<TcpConnection>& conn) {
    loop_->queueInLoop([this, conn] {
        conn->setMessageCallback(nullptr);   // 先拆环！否则引用计数永远归不了零
        conn->setCloseCallback(nullptr);
        conns_.erase(conn->fd());            // 归零 → 析构 → ::close(fd)
    });
}
