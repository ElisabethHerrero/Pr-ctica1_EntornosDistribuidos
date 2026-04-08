#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <string>
#include <queue>
#include <mutex>
#include <optional>

enum class EventType
{
    ZONE_ASSIGNED,
    ZONE_STARTED,
    ZONE_FINISHED,
    SESSION_FINISHED
};

struct Event
{
    EventType type;
    int zoneId;
    int roombaId;
    std::string message;
};

class EventBus
{
private:
    std::queue<Event> events;
    std::mutex mtx;

public:
    void publish(const Event& e)
    {
        std::lock_guard<std::mutex> lock(mtx);
        events.push(e);
    }

    std::optional<Event> poll()
    {
        std::lock_guard<std::mutex> lock(mtx);

        if (events.empty())
            return std::nullopt;

        Event e = events.front();
        events.pop();
        return e;
    }
};

#endif
