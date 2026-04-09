#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include "Roomba.h"
#include "Zone.h"
#include "EventService.h"

class CleaningService {
public:
    CleaningService()
        : zones_(nullptr), roombas_(nullptr), running_(false) {
    }

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

        running_ = true;
        worker_ = std::thread(&CleaningService::loop, this);
    }

    void stopCleaning() {
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool isRunning() const {
        return running_.load();
    }

private:
    std::vector<std::shared_ptr<Zone>>* zones_;
    std::vector<std::shared_ptr<Roomba>>* roombas_;
    std::atomic<bool> running_;
    std::thread worker_;

    static double clampValue(double v, double minV, double maxV) {
        return std::max(minV, std::min(v, maxV));
    }

    static double distSq(double x1, double y1, double x2, double y2) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        return dx * dx + dy * dy;
    }

    bool collidesWithObstacle(std::shared_ptr<Zone> zone, double x, double y, double radius) {
        if (!zone) return false;
        for (const auto& obs : zone->getObstacles()) {
            if (obs && obs->collidesWith(x, y, radius)) {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<Zone> findNextUnfinishedZone() {
        if (!zones_) return nullptr;

        for (auto& zone : *zones_) {
            if (!zone) continue;
            if (zone->isFullyCleaned()) continue;

            bool assigned = false;
            if (roombas_) {
                for (auto& roomba : *roombas_) {
                    if (!roomba) continue;
                    if (roomba->getCurrentZone() == zone && roomba->isCleaning()) {
                        assigned = true;
                        break;
                    }
                }
            }

            if (!assigned) return zone;
        }

        return nullptr;
    }

    bool allZonesFinished() {
        if (!zones_) return true;

        for (auto& zone : *zones_) {
            if (zone && !zone->isFullyCleaned()) {
                return false;
            }
        }
        return true;
    }

    void assignZoneToRoomba(std::shared_ptr<Roomba>& roomba, std::shared_ptr<Zone>& zone) {
        if (!roomba || !zone) return;

        double start = std::max(20.0, roomba->getCleaningRadius() * 1.5);

        roomba->setCurrentZone(zone);
        roomba->setState(Roomba::CLEANING);
        roomba->setPosition(start, start);
        roomba->setAngle(0.0);
        roomba->resetStuckCounter();
        roomba->resetFailedMoves();
        roomba->clearPath();
        roomba->updateLastSuccessful(start, start);

        zone->markClean(start, start, roomba->getCleaningRadius());
        zone->addTrail(start, start, roomba->getId());

        wchar_t msg[256];
        swprintf_s(msg, L"Roomba #%d asignada a %s", roomba->getId(), zone->getName());
        EventService::getInstance().publishLog(msg);
    }

    bool tryMoveTo(std::shared_ptr<Roomba>& roomba,
        std::shared_ptr<Zone>& zone,
        double x, double y,
        double robotRadius,
        double cleanRadius) {
        if (!zone || !roomba) return false;

        double minX = robotRadius;
        double minY = robotRadius;
        double maxX = zone->getLength() - robotRadius;
        double maxY = zone->getWidth() - robotRadius;

        x = clampValue(x, minX, maxX);
        y = clampValue(y, minY, maxY);

        if (collidesWithObstacle(zone, x, y, robotRadius)) {
            return false;
        }

        double oldX = roomba->getX();
        double oldY = roomba->getY();

        if (distSq(oldX, oldY, x, y) < 0.25) {
            return false;
        }

        roomba->setPosition(x, y);
        roomba->setAngle(std::atan2(y - oldY, x - oldX));
        roomba->updateLastSuccessful(x, y);
        roomba->resetFailedMoves();

        zone->markClean(x, y, cleanRadius);
        zone->addTrail(x, y, roomba->getId());
        return true;
    }

    bool tryFollowPath(std::shared_ptr<Roomba>& roomba,
        std::shared_ptr<Zone>& zone,
        double speed,
        double robotRadius,
        double cleanRadius) {
        auto path = roomba->getPath();
        if (path.empty()) return false;

        auto nextCell = path.front();

        double tx, ty;
        zone->cellToWorldCenter(nextCell.row, nextCell.col, tx, ty);

        double x = roomba->getX();
        double y = roomba->getY();

        double dx = tx - x;
        double dy = ty - y;
        double dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= cleanRadius * 0.6) {
            roomba->popPathFront();
            return true;
        }

        if (dist < 0.001) {
            roomba->popPathFront();
            return true;
        }

        dx /= dist;
        dy /= dist;

        if (tryMoveTo(roomba, zone, x + dx * speed, y + dy * speed, robotRadius, cleanRadius)) {
            roomba->resetStuckCounter();
            return true;
        }

        return false;
    }

    void recomputePath(std::shared_ptr<Roomba>& roomba, std::shared_ptr<Zone>& zone, double robotRadius) {
        std::vector<Zone::GridCell> zonePath;

        if (zone->findPathToNearestDirtyCell(roomba->getX(), roomba->getY(), robotRadius, zonePath)) {
            std::vector<Roomba::PathCell> roombaPath;
            roombaPath.reserve(zonePath.size());

            for (const auto& cell : zonePath) {
                roombaPath.push_back({ cell.row, cell.col });
            }

            roomba->setPath(roombaPath);
            roomba->resetStuckCounter();
        }
        else {
            roomba->clearPath();
        }
    }

    void loop() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));

            if (!zones_ || !roombas_) continue;

            bool anyCleaning = false;

            for (auto& roomba : *roombas_) {
                if (!roomba) continue;

                auto zone = roomba->getCurrentZone();

                if (zone && roomba->isCleaning() && zone->isFullyCleaned()) {
                    wchar_t msg[256];
                    swprintf_s(msg, L"Roomba #%d completo %s", roomba->getId(), zone->getName());
                    EventService::getInstance().publishLog(msg);

                    roomba->setState(Roomba::IDLE);
                    roomba->setCurrentZone(nullptr);
                    roomba->resetStuckCounter();
                    roomba->resetFailedMoves();
                    roomba->clearPath();
                    zone = nullptr;
                }

                if (!roomba->isCleaning()) {
                    auto nextZone = findNextUnfinishedZone();
                    if (nextZone) {
                        assignZoneToRoomba(roomba, nextZone);
                        zone = nextZone;
                    }
                }

                if (zone && roomba->isCleaning()) {
                    anyCleaning = true;
                    moveRoomba(roomba, zone);
                }
            }

            if (!anyCleaning && allZonesFinished()) {
                EventService::getInstance().publishLog(L"Todas las zonas completadas");
                running_ = false;
            }
        }
    }

    void moveRoomba(std::shared_ptr<Roomba>& roomba, std::shared_ptr<Zone>& zone) {
        if (!roomba || !zone) return;

        const double speed = roomba->getMovementSpeed();
        const double cleanRadius = roomba->getCleaningRadius();
        const double robotRadius = std::max(8.0, cleanRadius * 0.6);

        double x = roomba->getX();
        double y = roomba->getY();

        zone->markClean(x, y, cleanRadius);
        zone->addTrail(x, y, roomba->getId());

        if (zone->isFullyCleaned()) {
            roomba->clearPath();
            return;
        }

        bool needsRecompute = false;

        if (!roomba->hasPath()) {
            needsRecompute = true;
        }
        else if (roomba->getStuckCounter() > 8) {
            needsRecompute = true;
        }
        else if (roomba->getFailedMoves() > 4) {
            needsRecompute = true;
        }

        if (needsRecompute) {
            recomputePath(roomba, zone, robotRadius);
            roomba->resetStuckCounter();
            roomba->resetFailedMoves();
        }

        bool moved = false;

        if (roomba->hasPath()) {
            moved = tryFollowPath(roomba, zone, speed, robotRadius, cleanRadius);
        }

        if (!moved) {
            roomba->incrementStuckCounter();
            roomba->incrementFailedMoves();

            const double escapeMoves[][2] = {
                { speed, 0.0 }, { -speed, 0.0 }, { 0.0, speed }, { 0.0, -speed },
                { speed, speed }, { speed, -speed }, { -speed, speed }, { -speed, -speed }
            };

            for (const auto& m : escapeMoves) {
                if (tryMoveTo(roomba, zone, x + m[0], y + m[1], robotRadius, cleanRadius)) {
                    moved = true;
                    roomba->resetStuckCounter();
                    roomba->resetFailedMoves();
                    break;
                }
            }
        }

        if (!moved) {
            zone->markClean(x, y, cleanRadius * 1.05);
        }
        else {
            roomba->resetStuckCounter();
        }
    }
};