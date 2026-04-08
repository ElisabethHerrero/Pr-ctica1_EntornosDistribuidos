#ifndef MODELS_H
#define MODELS_H

#include <string>

enum class ZoneStatus
{
    PENDING,
    ASSIGNED,
    CLEANING,
    FINISHED
};

class Zone
{
private:
    int id;
    std::string name;

    float x, y;
    float width, height;

    float largoCm;
    float anchoCm;

    ZoneStatus status;
    int assignedRoombaId;

public:
    Zone(int id, std::string name,
        float x, float y, float w, float h,
        float largo, float ancho)
        : id(id), name(name),
        x(x), y(y), width(w), height(h),
        largoCm(largo), anchoCm(ancho),
        status(ZoneStatus::PENDING),
        assignedRoombaId(-1)
    {
    }

    float getArea() const
    {
        return largoCm * anchoCm;
    }

    // getters
    int getId() const { return id; }
    std::string getName() const { return name; }

    float getX() const { return x; }
    float getY() const { return y; }
    float getWidth() const { return width; }
    float getHeight() const { return height; }

    ZoneStatus getStatus() const { return status; }
    void setStatus(ZoneStatus s) { status = s; }

    void assignRoomba(int id) { assignedRoombaId = id; }
    int getAssignedRoomba() const { return assignedRoombaId; }
};

class Roomba
{
private:
    int id;
    std::string model;
    float cleaningRate;
    bool busy;

public:
    Roomba(int id, std::string model, float rate)
        : id(id), model(model), cleaningRate(rate), busy(false)
    {
    }

    int getId() const { return id; }
    float getRate() const { return cleaningRate; }

    bool isBusy() const { return busy; }
    void setBusy(bool b) { busy = b; }
};

class CleaningSession
{
private:
    int sessionId;
    float totalArea;
    float estimatedTime;

public:
    CleaningSession(int id)
        : sessionId(id), totalArea(0), estimatedTime(0)
    {
    }

    void setTotalArea(float area) { totalArea = area; }
    void setEstimatedTime(float time) { estimatedTime = time; }

    float getTotalArea() const { return totalArea; }
    float getEstimatedTime() const { return estimatedTime; }
};

#endif
