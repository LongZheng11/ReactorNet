#include "reactornet/Channel.h"

#include "reactornet/EventLoop.h"

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), events_(0) {}

void Channel::handleEvent(uint32_t revents) {
    if (revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        if (readCb_) readCb_();
    }
    if (revents & EPOLLOUT) {
        if (writeCb_) writeCb_();
    }
}

void Channel::enableReading()  { events_ |= kReadEvent;  loop_->updateChannel(this); }
void Channel::disableReading() { events_ &= ~kReadEvent; loop_->updateChannel(this); }
void Channel::enableWriting()  { events_ |= kWriteEvent; loop_->updateChannel(this); }
void Channel::disableWriting() { events_ &= ~kWriteEvent; loop_->updateChannel(this); }

void Channel::disableAll() {
    events_ = 0;
    loop_->removeChannel(this);
}