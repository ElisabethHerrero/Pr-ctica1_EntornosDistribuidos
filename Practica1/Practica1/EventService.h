#pragma once
#include <windows.h>
#include <vector>
#include <functional>
#include <mutex>

class EventService {
public:
    using EventCallback = std::function<void(const wchar_t*)>;

    static EventService& getInstance() {
        static EventService instance;
        return instance;
    }

    void subscribe(EventCallback callback) {
        if (!callback) return;
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.push_back(callback);
    }

    void clearSubscriptions() {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.clear();
    }

    void publishRoombaStarted(int id, const wchar_t* zone) {
        if (!zone) return;
        wchar_t msg[256];
        wsprintf(msg, L"Roomba #%d -> %s", id, zone);
        publish(msg);
    }

    void publishRoombaFinished(int id, const wchar_t* zone) {
        if (!zone) return;
        wchar_t msg[256];
        wsprintf(msg, L"Roomba #%d completo %s", id, zone);
        publish(msg);
    }

    void publishZoneCompleted(const wchar_t* zone) {
        if (!zone) return;
        wchar_t msg[256];
        wsprintf(msg, L"Zona completada: %s", zone);
        publish(msg);
    }

    void publishAllCompleted() {
        publish(L"TODAS LAS ZONAS COMPLETADAS!");
    }

    void publishLog(const wchar_t* msg) {
        if (msg) publish(msg);
    }

private:
    EventService() = default;
    std::vector<EventCallback> callbacks_;
    std::mutex mutex_;

    void publish(const wchar_t* message) {
        if (!message) return;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& callback : callbacks_) {
            if (callback) {
                try {
                    callback(message);
                }
                catch (...) {}
            }
        }
    }
};