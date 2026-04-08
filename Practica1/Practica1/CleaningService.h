#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <random>
#include <cmath>
#include "Roomba.h"
#include "Zone.h"
#include "EventService.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline double safeMin(double a, double b) { return (a < b) ? a : b; }
inline double safeMax(double a, double b) { return (a > b) ? a : b; }

class CleaningService {
public:
    CleaningService() : running_(false), stopRequested_(false), zones_(nullptr), roombas_(nullptr) {}

    ~CleaningService() {
        stopCleaning();
    }

    void setZones(std::vector<std::shared_ptr<Zone>>* zones) {
        zones_ = zones;
    }

    void setRoombas(std::vector<std::shared_ptr<Roomba>>* roombas) {
        roombas_ = roombas;
    }

    void startCleaning() {
        if (running_.load()) return;
        if (!zones_ || !roombas_) return;
        if (zones_->empty() || roombas_->empty()) return;

        stopRequested_ = false;
        running_ = true;

        // Asignar zonas iniciales
        size_t zoneIdx = 0;
        for (auto& roomba : *roombas_) {
            if (!roomba) continue;
            if (zoneIdx >= zones_->size()) break;

            auto& zone = (*zones_)[zoneIdx];
            if (zone && !zone->isFullyCleaned()) {
                roomba->setCurrentZone(zone);
                roomba->setState(Roomba::CLEANING);
                roomba->setPosition(zone->getLength() / 2.0, zone->getWidth() / 2.0);
                roomba->setAngle(0.0);

                EventService::getInstance().publishRoombaStarted(roomba->getId(), zone->getName());
                zoneIdx++;
            }
        }

        cleaningThread_ = std::thread([this]() { this->cleaningLoop(); });
    }

    void stopCleaning() {
        stopRequested_ = true;
        running_ = false;

        if (cleaningThread_.joinable()) {
            cleaningThread_.join();
        }

        if (roombas_) {
            for (auto& roomba : *roombas_) {
                if (roomba) {
                    roomba->setState(Roomba::IDLE);
                }
            }
        }
    }

    bool isRunning() const { return running_.load(); }

private:
    std::vector<std::shared_ptr<Zone>>* zones_;
    std::vector<std::shared_ptr<Roomba>>* roombas_;
    std::atomic<bool> running_;
    std::atomic<bool> stopRequested_;
    std::thread cleaningThread_;

    void cleaningLoop() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> angleDis(-0.3, 0.3);
        std::uniform_real_distribution<double> turnDis(0.0, 1.0);

        while (running_.load() && !stopRequested_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));

            if (!roombas_ || !zones_) break;
            if (stopRequested_.load()) break;

            bool anyWorking = false;

            for (auto& roomba : *roombas_) {
                if (!roomba) continue;
                if (stopRequested_.load()) break;

                auto zone = roomba->getCurrentZone();
                if (!zone) continue;

                if (roomba->getState() != Roomba::CLEANING) continue;

                // Zona completa?
                if (zone->isFullyCleaned()) {
                    roomba->setState(Roomba::IDLE);
                    roomba->setCurrentZone(nullptr);

                    EventService::getInstance().publishRoombaFinished(roomba->getId(), zone->getName());

                    // Buscar siguiente zona
                    auto nextZone = findNextZone();
                    if (nextZone) {
                        roomba->setCurrentZone(nextZone);
                        roomba->setState(Roomba::CLEANING);
                        roomba->setPosition(nextZone->getLength() / 2.0, nextZone->getWidth() / 2.0);
                        EventService::getInstance().publishRoombaStarted(roomba->getId(), nextZone->getName());
                        anyWorking = true;
                    }
                    continue;
                }

                anyWorking = true;

                // Simular limpieza
                double areaPerTick = roomba->getCleaningRate() * 0.025;
                double totalArea = zone->getArea();
                if (totalArea <= 0) totalArea = 1;

                double currentProgress = zone->getCleanedPercentage();
                double newProgress = currentProgress + (areaPerTick / totalArea) * 100.0;
                newProgress = safeMin(newProgress, 100.0);
                zone->setCleanedPercentage(newProgress);
                roomba->addCleanedArea(areaPerTick);

                // Mover roomba
                moveRoomba(roomba, zone, gen, angleDis, turnDis);
            }

            // Verificar si todo está completo
            if (!anyWorking || checkAllZonesComplete()) {
                if (checkAllZonesComplete()) {
                    EventService::getInstance().publishAllCompleted();
                }
                running_ = false;
                break;
            }
        }

        running_ = false;
    }

    void moveRoomba(std::shared_ptr<Roomba>& roomba, std::shared_ptr<Zone>& zone,
        std::mt19937& gen,
        std::uniform_real_distribution<double>& angleDis,
        std::uniform_real_distribution<double>& turnDis) {

        if (!roomba || !zone) return;

        double currentX = roomba->getX();
        double currentY = roomba->getY();
        double angle = roomba->getAngle();
        double speed = 5.0;

        if (turnDis(gen) < 0.1) {
            angle += angleDis(gen) * M_PI;
        }

        double newX = currentX + cos(angle) * speed;
        double newY = currentY + sin(angle) * speed;

        double maxX = zone->getLength() - 8.0;
        double maxY = zone->getWidth() - 8.0;

        if (newX < 8.0) {
            newX = 8.0;
            angle = M_PI - angle;
        }
        else if (newX > maxX) {
            newX = maxX;
            angle = M_PI - angle;
        }

        if (newY < 8.0) {
            newY = 8.0;
            angle = -angle;
        }
        else if (newY > maxY) {
            newY = maxY;
            angle = -angle;
        }

        bool collision = false;
        for (const auto& obs : zone->getObstacles()) {
            if (obs && obs->collidesWith(newX, newY, 8.0)) {
                collision = true;
                angle += M_PI / 2.0 + angleDis(gen);
                break;
            }
        }

        if (!collision) {
            roomba->setPosition(newX, newY);
            roomba->setAngle(angle);
            zone->addTrailPoint(newX, newY, roomba->getId());
        }
    }

    std::shared_ptr<Zone> findNextZone() {
        if (!zones_) return nullptr;

        for (auto& zone : *zones_) {
            if (!zone) continue;
            if (zone->isFullyCleaned()) continue;

            bool inUse = false;
            if (roombas_) {
                for (auto& roomba : *roombas_) {
                    if (roomba && roomba->getCurrentZone() == zone &&
                        roomba->getState() == Roomba::CLEANING) {
                        inUse = true;
                        break;
                    }
                }
            }

            if (!inUse) return zone;
        }

        return nullptr;
    }

    bool checkAllZonesComplete() {
        if (!zones_) return true;

        for (auto& zone : *zones_) {
            if (zone && !zone->isFullyCleaned()) {
                return false;
            }
        }
        return true;
    }
};