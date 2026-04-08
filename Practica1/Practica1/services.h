#ifndef SERVICES_H
#define SERVICES_H

#include "models.h"
#include "repository.h"
#include "event_bus.h"

#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <mutex>
#include <atomic>

class ClientServerService
{
private:
    Repository& repository;
    EventBus& eventBus;
    std::mutex assignMutex;

public:
    ClientServerService(Repository& repo, EventBus& bus)
        : repository(repo), eventBus(bus)
    {
    }

    Zone* requestZoneForRoomba(int roombaId)
    {
        std::lock_guard<std::mutex> lock(assignMutex);

        for (auto& zone : repository.getZones())
        {
            if (zone.status == ZoneStatus::PENDING)
            {
                zone.status = ZoneStatus::ASSIGNED;
                zone.assignedRoombaId = roombaId;

                eventBus.publish({
                    EventType::ZONE_ASSIGNED,
                    zone.id,
                    roombaId,
                    "Zona asignada a la Roomba " + std::to_string(roombaId)
                    });

                return &zone;
            }
        }

        return nullptr;
    }
};

class PeerToPeerService
{
public:
    void notifyPeers(int roombaId, int zoneId)
    {
        std::cout << "[P2P] Roomba " << roombaId
            << " informa al resto que está trabajando en la zona "
            << zoneId << "\n";
    }
};

class CleaningCoordinator
{
private:
    Repository& repository;
    EventBus& eventBus;
    ClientServerService& clientServer;
    PeerToPeerService& p2pService;
    std::vector<std::thread> workers;
    std::atomic<int> finishedZones = 0;

public:
    CleaningCoordinator(Repository& repo, EventBus& bus,
        ClientServerService& cs, PeerToPeerService& p2p)
        : repository(repo), eventBus(bus), clientServer(cs), p2pService(p2p)
    {
    }

    void roombaWorker(Roomba& roomba)
    {
        while (true)
        {
            Zone* zone = clientServer.requestZoneForRoomba(roomba.id);

            if (zone == nullptr)
                break;

            p2pService.notifyPeers(roomba.id, zone->id);

            zone->status = ZoneStatus::CLEANING;
            roomba.busy = true;

            eventBus.publish({
                EventType::ZONE_STARTED,
                zone->id,
                roomba.id,
                "La Roomba " + std::to_string(roomba.id) +
                " ha empezado a limpiar " + zone->name
                });

            float estimatedSeconds = zone->area() / roomba.cleaningRateCm2PerSec;

            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(estimatedSeconds * 300))
            );
            // Multiplico por 300 ms para que se vea la simulación más rápida en pantalla.

            zone->status = ZoneStatus::FINISHED;
            roomba.busy = false;
            finishedZones++;

            eventBus.publish({
                EventType::ZONE_FINISHED,
                zone->id,
                roomba.id,
                "La Roomba " + std::to_string(roomba.id) +
                " ha terminado " + zone->name
                });
        }
    }

    void startCleaning()
    {
        for (auto& roomba : repository.getRoombas())
        {
            workers.emplace_back(&CleaningCoordinator::roombaWorker, this, std::ref(roomba));
        }
    }

    void waitForCompletion()
    {
        for (auto& t : workers)
        {
            if (t.joinable())
                t.join();
        }

        eventBus.publish({
            EventType::SESSION_FINISHED,
            -1,
            -1,
            "Todas las zonas han sido limpiadas."
            });
    }
};

#endif
