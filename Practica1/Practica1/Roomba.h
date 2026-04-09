#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class Zone;

class Roomba {
public:
    struct PathCell {
        int row;
        int col;
    };

    enum Type { BASIC = 0, ADVANCED = 1, PREMIUM = 2 };
    enum State { IDLE = 0, CLEANING = 1, MOVING = 2, FINISHED = 3 };

    Roomba(int id, Type type)
        : id_(id), type_(type), state_(IDLE),
        x_(50.0), y_(50.0),
        lastX_(50.0), lastY_(50.0),
        angle_(0.0), totalAreaCleaned_(0.0),
        stuckCounter_(0),
        failedMoveAttempts_(0),
        lastSuccessfulX_(50.0),
        lastSuccessfulY_(50.0) {

        switch (type) {
        case BASIC:
            cleaningRate_ = 5000.0;
            movementSpeed_ = 4.0;
            cleaningRadius_ = 14.0;
            color_ = RGB(52, 152, 219);
            break;
        case ADVANCED:
            cleaningRate_ = 8000.0;
            movementSpeed_ = 5.5;
            cleaningRadius_ = 17.0;
            color_ = RGB(46, 204, 113);
            break;
        case PREMIUM:
            cleaningRate_ = 12000.0;
            movementSpeed_ = 7.0;
            cleaningRadius_ = 20.0;
            color_ = RGB(231, 76, 60);
            break;
        default:
            cleaningRate_ = 6000.0;
            movementSpeed_ = 5.0;
            cleaningRadius_ = 16.0;
            color_ = RGB(155, 89, 182);
            break;
        }
    }

    int getId() const { return id_; }
    Type getType() const { return type_; }
    State getState() const { return state_.load(); }
    void setState(State state) { state_.store(state); }

    double getCleaningRate() const { return cleaningRate_; }
    double getMovementSpeed() const { return movementSpeed_; }
    double getCleaningRadius() const { return cleaningRadius_; }
    COLORREF getColor() const { return color_; }

    const wchar_t* getTypeName() const {
        switch (type_) {
        case BASIC: return L"Basica";
        case ADVANCED: return L"Avanzada";
        case PREMIUM: return L"Premium";
        default: return L"Desconocida";
        }
    }

    const wchar_t* getStateName() const {
        switch (state_.load()) {
        case IDLE: return L"Inactiva";
        case CLEANING: return L"Limpiando";
        case MOVING: return L"Moviendose";
        case FINISHED: return L"Finalizada";
        default: return L"Desconocido";
        }
    }

    double getX() const { return x_.load(); }
    double getY() const { return y_.load(); }

    void setPosition(double x, double y) {
        lastX_.store(x_.load());
        lastY_.store(y_.load());
        x_.store(x);
        y_.store(y);
    }

    double getLastX() const { return lastX_.load(); }
    double getLastY() const { return lastY_.load(); }

    double getAngle() const { return angle_.load(); }
    void setAngle(double angle) { angle_.store(angle); }

    void setCurrentZone(std::shared_ptr<Zone> zone) {
        std::lock_guard<std::mutex> lock(zoneMutex_);
        currentZone_ = zone;
    }

    std::shared_ptr<Zone> getCurrentZone() const {
        std::lock_guard<std::mutex> lock(zoneMutex_);
        return currentZone_;
    }

    bool isAvailable() const { return state_.load() == IDLE; }
    bool isCleaning() const { return state_.load() == CLEANING; }

    double getTotalAreaCleaned() const { return totalAreaCleaned_.load(); }
    void addCleanedArea(double area) {
        totalAreaCleaned_.store(totalAreaCleaned_.load() + area);
    }

    void resetStuckCounter() { stuckCounter_.store(0); }
    void incrementStuckCounter() { stuckCounter_.store(stuckCounter_.load() + 1); }
    int getStuckCounter() const { return stuckCounter_.load(); }

    void incrementFailedMoves() { failedMoveAttempts_.store(failedMoveAttempts_.load() + 1); }
    void resetFailedMoves() { failedMoveAttempts_.store(0); }
    int getFailedMoves() const { return failedMoveAttempts_.load(); }

    void updateLastSuccessful(double x, double y) {
        lastSuccessfulX_.store(x);
        lastSuccessfulY_.store(y);
    }

    double getLastSuccessfulX() const { return lastSuccessfulX_.load(); }
    double getLastSuccessfulY() const { return lastSuccessfulY_.load(); }

    void setPath(const std::vector<PathCell>& path) {
        std::lock_guard<std::mutex> lock(pathMutex_);
        path_ = path;
    }

    std::vector<PathCell> getPath() const {
        std::lock_guard<std::mutex> lock(pathMutex_);
        return path_;
    }

    bool hasPath() const {
        std::lock_guard<std::mutex> lock(pathMutex_);
        return !path_.empty();
    }

    void popPathFront() {
        std::lock_guard<std::mutex> lock(pathMutex_);
        if (!path_.empty()) {
            path_.erase(path_.begin());
        }
    }

    void clearPath() {
        std::lock_guard<std::mutex> lock(pathMutex_);
        path_.clear();
    }

private:
    int id_;
    Type type_;
    double cleaningRate_;
    double movementSpeed_;
    double cleaningRadius_;
    COLORREF color_;

    std::atomic<State> state_;
    std::atomic<double> x_;
    std::atomic<double> y_;
    std::atomic<double> lastX_;
    std::atomic<double> lastY_;
    std::atomic<double> angle_;
    std::atomic<double> totalAreaCleaned_;
    std::atomic<int> stuckCounter_;
    std::atomic<int> failedMoveAttempts_;
    std::atomic<double> lastSuccessfulX_;
    std::atomic<double> lastSuccessfulY_;

    mutable std::mutex zoneMutex_;
    std::shared_ptr<Zone> currentZone_;

    mutable std::mutex pathMutex_;
    std::vector<PathCell> path_;
};