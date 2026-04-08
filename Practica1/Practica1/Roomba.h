#pragma once
#include <windows.h>
#include <atomic>
#include <memory>

class Zone;

class Roomba {
public:
    enum Type { BASIC = 0, ADVANCED = 1, PREMIUM = 2 };
    enum State { IDLE = 0, CLEANING = 1, MOVING = 2, FINISHED = 3 };

    Roomba(int id, Type type)
        : id_(id), type_(type), state_(IDLE), x_(50.0), y_(50.0),
        totalAreaCleaned_(0.0), angle_(0.0), currentZone_(nullptr) {

        switch (type) {
        case BASIC: cleaningRate_ = 5000.0; break;
        case ADVANCED: cleaningRate_ = 8000.0; break;
        case PREMIUM: cleaningRate_ = 12000.0; break;
        default: cleaningRate_ = 6000.0;
        }

        switch (type) {
        case BASIC: color_ = RGB(52, 152, 219); break;
        case ADVANCED: color_ = RGB(46, 204, 113); break;
        case PREMIUM: color_ = RGB(231, 76, 60); break;
        default: color_ = RGB(155, 89, 182);
        }
    }

    int getId() const { return id_; }
    Type getType() const { return type_; }
    State getState() const { return state_.load(); }
    double getCleaningRate() const { return cleaningRate_; }
    unsigned int getColor() const { return color_; }
    double getAngle() const { return angle_.load(); }
    void setAngle(double angle) { angle_.store(angle); }

    const wchar_t* getTypeName() const {
        switch (type_) {
        case BASIC: return L"Basica";
        case ADVANCED: return L"Avanzada";
        case PREMIUM: return L"Premium";
        default: return L"Desconocida";
        }
    }

    const wchar_t* getStateName() const {
        State s = state_.load();
        switch (s) {
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
        x_.store(x);
        y_.store(y);
    }

    void setCurrentZone(std::shared_ptr<Zone> zone) { currentZone_ = zone; }
    std::shared_ptr<Zone> getCurrentZone() const { return currentZone_; }

    void setState(State state) { state_.store(state); }
    bool isAvailable() const { return state_.load() == IDLE; }
    bool isCleaning() const { return state_.load() == CLEANING; }

    double getTotalAreaCleaned() const { return totalAreaCleaned_.load(); }

    void addCleanedArea(double area) {
        double current = totalAreaCleaned_.load();
        totalAreaCleaned_.store(current + area);
    }

private:
    int id_;
    Type type_;
    double cleaningRate_;
    unsigned int color_;
    std::atomic<State> state_;
    std::atomic<double> x_;
    std::atomic<double> y_;
    std::atomic<double> angle_;
    std::atomic<double> totalAreaCleaned_;
    std::shared_ptr<Zone> currentZone_;
};