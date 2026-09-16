#pragma once

#include <functional>
#include <map>
#include <vector>

class Channel;

class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();   // 阻塞运行直到 quit()
    void quit() { quitting_ = true; }

    // Channel 通过它注册/修改自己的关注事件（唯一能碰 epoll 的入口）
    void updateChannel(Channel* ch);
    void removeChannel(Channel* ch);

    // 把任务排到循环里，等本次事件全部分发完再执行
    void queueInLoop(Functor cb);

private:
    void runPendingFunctors();

    int epfd_;
    bool quitting_;
    std::map<int, Channel*> channels_;   // fd -> Channel 账本
    std::vector<Functor> pendingFunctors_;
};
