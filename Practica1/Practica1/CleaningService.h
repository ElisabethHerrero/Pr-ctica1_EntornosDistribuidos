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

    bool collidesWithObstacle(const std::shared_ptr<Zone>& zone, double x, double y, double radius) {
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

            if (!assigned) {
                return zone;
            }
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

    void releaseRoombaFromZone(std::shared_ptr<Roomba>& roomba,
        const std::shared_ptr<Zone>& zone,
        const wchar_t* reason) {
        if (!roomba) return;

        if (zone && reason) {
            wchar_t msg[256];
            swprintf_s(msg, L"Roomba #%d completa %s (%s)", roomba->getId(), zone->getName(), reason);
            EventService::getInstance().publishLog(msg);
        }

        roomba->setState(Roomba::IDLE);
        roomba->setCurrentZone(nullptr);
        roomba->resetStuckCounter();
        roomba->resetFailedMoves();
        roomba->clearPath();
    }

    bool zoneHasRemainingWork(const std::shared_ptr<Zone>& zone, double robotRadius) {
        if (!zone) return false;
        if (zone->isFullyCleaned()) return false;
        return zone->hasDirtyWalkableCell(robotRadius);
    }

    bool zoneHasReachableWork(const std::shared_ptr<Zone>& zone,
        const std::shared_ptr<Roomba>& roomba,
        double robotRadius) {
        if (!zone || !roomba) return false;
        if (zone->isFullyCleaned()) return false;
        return zone->hasReachableDirtyCell(roomba->getX(), roomba->getY(), robotRadius);
    }

    void assignZoneToRoomba(std::shared_ptr<Roomba>& roomba, std::shared_ptr<Zone>& zone) {
        if (!roomba || !zone) return;

        const double cleanRadius = roomba->getCleaningRadius();
        const double robotRadius = std::max(8.0, cleanRadius * 0.6);

        double startX = std::max(robotRadius, 20.0);
        double startY = std::max(robotRadius, 20.0);

        if (!zone->findAnyDirtyCell(robotRadius, startX, startY)) {
            startX = clampValue(startX, robotRadius, zone->getLength() - robotRadius);
            startY = clampValue(startY, robotRadius, zone->getWidth() - robotRadius);
        }

        roomba->setCurrentZone(zone);
        roomba->setState(Roomba::CLEANING);
        roomba->setPosition(startX, startY);
        roomba->setAngle(0.0);
        roomba->resetStuckCounter();
        roomba->resetFailedMoves();
        roomba->clearPath();
        roomba->updateLastSuccessful(startX, startY);

        zone->markClean(startX, startY, cleanRadius);
        zone->addTrail(startX, startY, roomba->getId());

        wchar_t msg[256];
        swprintf_s(msg, L"Roomba #%d asignada a %s", roomba->getId(), zone->getName());
        EventService::getInstance().publishLog(msg);
    }

    bool tryMoveTo(std::shared_ptr<Roomba>& roomba,
        std::shared_ptr<Zone>& zone,
        double x,
        double y,
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

        double tx = 0.0;
        double ty = 0.0;
        zone->cellToWorldCenter(nextCell.row, nextCell.col, tx, ty);

        double x = roomba->getX();
        double y = roomba->getY();

        double dx = tx - x;
        double dy = ty - y;
        double dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= cleanRadius * 0.6 || dist < 0.001) {
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

    bool recomputePath(std::shared_ptr<Roomba>& roomba,
        std::shared_ptr<Zone>& zone,
        double robotRadius) {
        std::vector<Zone::GridCell> zonePath;

        if (zone->findPathToNearestDirtyCell(roomba->getX(), roomba->getY(), robotRadius, zonePath)) {
            std::vector<Roomba::PathCell> roombaPath;
            roombaPath.reserve(zonePath.size());

            for (const auto& cell : zonePath) {
                roombaPath.push_back({ cell.row, cell.col });
            }

            roomba->setPath(roombaPath);
            roomba->resetStuckCounter();
            return true;
        }

        roomba->clearPath();
        return false;
    }

    bool tryRelocateToDirtyCell(std::shared_ptr<Roomba>& roomba,
        std::shared_ptr<Zone>& zone,
        double robotRadius,
        double cleanRadius) {
        if (!roomba || !zone) return false;

        double targetX = 0.0;
        double targetY = 0.0;

        if (!zone->findAnyDirtyCell(robotRadius, targetX, targetY)) {
            return false;
        }

        if (collidesWithObstacle(zone, targetX, targetY, robotRadius)) {
            return false;
        }

        roomba->setPosition(targetX, targetY);
        roomba->setAngle(0.0);
        roomba->updateLastSuccessful(targetX, targetY);
        roomba->resetStuckCounter();
        roomba->resetFailedMoves();
        roomba->clearPath();

        zone->markClean(targetX, targetY, cleanRadius);
        zone->addTrail(targetX, targetY, roomba->getId());

        wchar_t msg[256];
        swprintf_s(msg, L"Roomba #%d recolocada en %s por recuperacion de ruta",
            roomba->getId(), zone->getName());
        EventService::getInstance().publishLog(msg);

        return true;
    }

    void loop() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));

            if (!zones_ || !roombas_) continue;

            bool anyCleaning = false;

            for (auto& roomba : *roombas_) {
                if (!roomba) continue;

                auto zone = roomba->getCurrentZone();
                const double cleanRadius = roomba->getCleaningRate() > 0.0 ? roomba->getCleaningRadius() : roomba->getCleaningRadius();
                const double robotRadius = std::max(8.0, cleanRadius * 0.6);

                if (zone && roomba->isCleaning() && !zoneHasRemainingWork(zone, robotRadius)) {
                    releaseRoombaFromZone(roomba, zone, L"sin suciedad caminable");
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

                    zone = roomba->getCurrentZone();
                    if (zone && !zoneHasRemainingWork(zone, robotRadius)) {
                        releaseRoombaFromZone(roomba, zone, L"zona finalizada");
                    }
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

        if (!zoneHasRemainingWork(zone, robotRadius)) {
            roomba->clearPath();
            return;
        }

        if (!zoneHasReachableWork(zone, roomba, robotRadius)) {
            if (!tryRelocateToDirtyCell(roomba, zone, robotRadius, cleanRadius)) {
                releaseRoombaFromZone(roomba, zone, L"sin suciedad alcanzable");
            }
            return;
        }

        bool moved = false;
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
            if (!recomputePath(roomba, zone, robotRadius)) {
                if (!tryRelocateToDirtyCell(roomba, zone, robotRadius, cleanRadius)) {
                    releaseRoombaFromZone(roomba, zone, L"ruta imposible");
                }
                return;
            }

            roomba->resetStuckCounter();
            roomba->resetFailedMoves();
        }

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
                    roomba->clearPath();
                    break;
                }
            }
        }

        if (!moved && roomba->getFailedMoves() > 8) {
            if (zoneHasReachableWork(zone, roomba, robotRadius)) {
                if (!recomputePath(roomba, zone, robotRadius)) {
                    if (!tryRelocateToDirtyCell(roomba, zone, robotRadius, cleanRadius)) {
                        releaseRoombaFromZone(roomba, zone, L"atasco persistente");
                    }
                    return;
                }
            }
            else {
                if (!tryRelocateToDirtyCell(roomba, zone, robotRadius, cleanRadius)) {
                    releaseRoombaFromZone(roomba, zone, L"sin progreso");
                }
                return;
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