#pragma once

#include <cstdint>
#include <functional>
#include <sys/epoll.h>

class EventLoop;

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    int fd() const { return fd_; }
    uint32_t events() const { return events_; }

    void handleEvent(uint32_t revents);

    // 想改关注事件，都只能经 EventLoop 这一道门
    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    void setReadCallback(EventCallback cb)  { readCb_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCb_ = std::move(cb); }

private:
    static constexpr uint32_t kReadEvent = EPOLLIN | EPOLLPRI;
    static constexpr uint32_t kWriteEvent = EPOLLOUT;

    EventLoop* loop_;
    int fd_;
    uint32_t events_;       // 当前关心的底层事件掩码
    EventCallback readCb_;
    EventCallback writeCb_;
};