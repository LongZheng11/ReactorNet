#include "reactornet/EventLoop.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/epoll.h>
#include <unistd.h>

#include "reactornet/Channel.h"

namespace {

constexpr int kMaxEvents = 64;

}  // namespace

EventLoop::EventLoop() : epfd_(::epoll_create1(0)), quitting_(false) {
    if (epfd_ < 0) {
        std::perror("epoll_create1");
        std::exit(1);
    }
}

EventLoop::~EventLoop() {
    ::close(epfd_);
}

void EventLoop::updateChannel(Channel* ch) {
    epoll_event ev{};
    ev.events = ch->events();
    ev.data.ptr = ch;
    auto it = channels_.find(ch->fd());
    int op = (it == channels_.end()) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    if (::epoll_ctl(epfd_, op, ch->fd(), &ev) < 0) {
        std::perror("epoll_ctl");
        std::exit(1);
    }
    channels_[ch->fd()] = ch;
}

void EventLoop::removeChannel(Channel* ch) {
    ::epoll_ctl(epfd_, EPOLL_CTL_DEL, ch->fd(), nullptr);
    channels_.erase(ch->fd());
}

void EventLoop::queueInLoop(Functor cb) {
    pendingFunctors_.push_back(std::move(cb));
}

void EventLoop::runPendingFunctors() {
    std::vector<Functor> functors;
    functors.swap(pendingFunctors_);
    for (auto& f : functors) f();
}

void EventLoop::loop() {
    epoll_event events[kMaxEvents];
    while (!quitting_) {
        int n = ::epoll_wait(epfd_, events, kMaxEvents, -1);
        if (n < 0) {
            if (errno == EINTR) continue;   // 被信号打断，不是错误
            std::perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; ++i) {
            auto* ch = static_cast<Channel*>(events[i].data.ptr);
            ch->handleEvent(events[i].events);
        }
        runPendingFunctors();
    }
}
