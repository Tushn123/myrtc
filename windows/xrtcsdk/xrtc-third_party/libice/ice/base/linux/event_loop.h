#ifndef ICE_BASE_LINUX_EVENT_LOOP_H_
#define ICE_BASE_LINUX_EVENT_LOOP_H_

#include <ev.h>

namespace ice {

class EventLoop;
class TimerWatcher;
class IOWatcher;

typedef void (*TimerCb)(EventLoop* el, TimerWatcher* w, void* priv_data);
typedef void (*IOCb)(EventLoop* el, IOWatcher* w, int fd, 
                     int event_mask, void* priv_data);

class TimerWatcher {
public:
    ev_timer timer;
    TimerCb cb;
    EventLoop* el;
    void* priv_data;
    bool repeat;
    TimerWatcher(EventLoop* el, TimerCb cb, void* priv_data, bool repeat);
};

class IOWatcher {
public:
    ev_io io;
    IOCb cb;
    EventLoop* el;
    void* priv_data;
    IOWatcher(EventLoop* el, IOCb cb, void* priv_data);
};

class EventLoop {
public:
    enum class EventType {
        NONE = 0x00,
        READ = 0x01,
        WRITE = 0x02
    };
    EventLoop();
    ~EventLoop();

    void Run();
    void Stop();
    void Sleep(unsigned long usec);
    void Suspend();
    void Resume();

    unsigned long Now(); // get current time

    // Timer
    TimerWatcher *CreateTimer(TimerCb cb, void* priv_data, bool repeat);
    void DeleteTimer(TimerWatcher* w);
    void StartTimer(TimerWatcher* w, unsigned long usec);
    void StopTimer(TimerWatcher* w);

    //io
    IOWatcher* CreateIOEvent(IOCb cb, void* priv_data);
    void DeleteIOEvent(IOWatcher* w);
    void StartIOEvent(IOWatcher* w, int fd, int event_mask);
    void StopIOEvent(IOWatcher* w, int fd, int event_mask);

public:
    //void* owner;

private:
    struct ev_loop* loop_;
};

} // namespace ice

#endif // ICE_BASE_LINUX_EVENT_LOOP_H_


