#ifndef REPOSITORY_H
#define REPOSITORY_H

#include "models.h"
#include <vector>
#include <mutex>

class Repository
{
private:
    std::vector<Zone> zones;
    std::vector<Roomba> roombas;
    std::vector<CleaningSession> sessions;
    mutable std::mutex mtx;

public:
    void addZone(const Zone& zone)
    {
        std::lock_guard<std::mutex> lock(mtx);
        zones.push_back(zone);
    }

    void addRoomba(const Roomba& roomba)
    {
        std::lock_guard<std::mutex> lock(mtx);
        roombas.push_back(roomba);
    }

    void addSession(const CleaningSession& session)
    {
        std::lock_guard<std::mutex> lock(mtx);
        sessions.push_back(session);
    }

    std::vector<Zone>& getZones()
    {
        return zones;
    }

    std::vector<Roomba>& getRoombas()
    {
        return roombas;
    }

    std::vector<CleaningSession>& getSessions()
    {
        return sessions;
    }

    float calculateTotalArea() const
    {
        std::lock_guard<std::mutex> lock(mtx);
        float total = 0.0f;
        for (const auto& z : zones)
            total += z.area();
        return total;
    }
};

#endif#pragma once
