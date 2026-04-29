#include "ice/base/linux/event_loop.h"

namespace ice {

EventLoop::EventLoop() 
    : loop_(nullptr) 
{
    loop_ = ev_loop_new(EVFLAG_AUTO);
    ev_set_userdata(loop_, (void*)this);
}

EventLoop::~EventLoop() {
    if (loop_) {
        Stop();
        if (!ev_is_default_loop(loop_)) {
            ev_loop_destroy(loop_);
        }
    }
}

void EventLoop::Run() {
    ev_run(loop_);
}

void EventLoop::Stop() {
    ev_break(loop_, EVBREAK_ALL);
}

void EventLoop::Sleep(unsigned long usec) {
    ev_sleep(static_cast<double>(usec) / 1000000);
}

void EventLoop::Suspend() {
    ev_suspend(loop_);
}

void EventLoop:: Resume() {
    ev_resume(loop_);
}

//Timer
void GenericTimerCb(struct ev_loop*, struct ev_timer* w, int) {
    TimerWatcher* watcher = (TimerWatcher*)w;
    watcher->cb(watcher->el, watcher, watcher->priv_data);
}

TimerWatcher::TimerWatcher(EventLoop* el, TimerCb cb, void* priv_data, bool repeat) 
    : cb(cb), el(el), priv_data(priv_data), repeat(repeat) {
    timer.data = this;
}

TimerWatcher* EventLoop::CreateTimer(TimerCb cb, void* priv_data, bool repeat) {
    TimerWatcher* w = new TimerWatcher(this, cb, priv_data, repeat);
    ev_init(&(w->timer), GenericTimerCb);
    return w;
}

void EventLoop::DeleteTimer(TimerWatcher* w) {
    StopTimer(w);
    delete w;
}

void EventLoop::StartTimer(TimerWatcher* w, unsigned long usec) {
    struct ev_timer* timer = &(w->timer);
    float sec = static_cast<float>(usec) / 1000000;
    if (!w->repeat) {
        ev_timer_stop(loop_, timer);
        ev_timer_set(timer, sec, 0);
        ev_timer_start(loop_, timer);
    } else {
        timer->repeat = sec;
        ev_timer_again(loop_, timer);
    }
}

void EventLoop::StopTimer(TimerWatcher* w) {
    struct ev_timer* timer = &(w->timer);
    ev_timer_stop(loop_, timer);
}

//io
void GenericIOCb(struct ev_loop* el, struct ev_io* w, int event_mask) {
    (void)el;
    IOWatcher* watcher = (IOWatcher*)(w->data);
    watcher->cb(watcher->el, watcher, w->fd,
            event_mask, watcher->priv_data);
}

IOWatcher::IOWatcher(EventLoop* el, IOCb cb, void* priv_data)
        : cb(cb), el(el), priv_data(priv_data){
    io.data = this;
}

IOWatcher* EventLoop::CreateIOEvent(IOCb cb, void* priv_data) {
    IOWatcher* w = new IOWatcher(this, cb, priv_data);
    ev_init(&(w->io), GenericIOCb);
    return w;
}

void EventLoop::DeleteIOEvent(IOWatcher* w) {
    struct ev_io* io = &(w->io);
    ev_io_stop(loop_, io);
    delete w;
}

void EventLoop::StartIOEvent(IOWatcher* w, int fd, int event_mask) {
    struct ev_io* io = &(w->io);
    /* If there's no change on the events, just return */
    if (ev_is_active(io)) {
        //int active_events = TRANS_FROM_EV_MASK(io->events);
        int events = io->events | event_mask;
        if (io->events == events) {
            return;
        }
        //events = TRANS_TO_EV_MASK(events);
        ev_io_stop(loop_, io);
        ev_io_set(io, fd, events);
        ev_io_start(loop_, io);
    } else {
        ev_io_set(io, fd, event_mask);
        ev_io_start(loop_, io);
    }
}

void EventLoop::StopIOEvent(IOWatcher* w, int fd, int event_mask) {
    struct ev_io* io = &(w->io);
    //int active_events = TRANS_FROM_EV_MASK(io->events);
    int events = io->events & (~event_mask);
    if (io->events == events) {
        return;
    }
    //events = TRANS_TO_EV_MASK(events);
    ev_io_stop(loop_, io);
    if (events != EV_NONE) {
        ev_io_set(io, fd, events);
        ev_io_start(loop_, io);
    }
}

} // namespace ice


