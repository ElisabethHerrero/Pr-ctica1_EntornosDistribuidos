#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

class Obstacle {
public:
    enum Type { FURNITURE, WALL, DECORATION, OTHER };

    Obstacle(double x, double y, double w, double h, Type t)
        : x_(x), y_(y), width_(w), height_(h), type_(t) {
    }

    double getX() const { return x_; }
    double getY() const { return y_; }
    double getWidth() const { return width_; }
    double getHeight() const { return height_; }
    Type getType() const { return type_; }

    bool collidesWith(double x, double y, double radius = 5.0) const {
        return (x + radius >= x_ && x - radius <= x_ + width_ &&
            y + radius >= y_ && y - radius <= y_ + height_);
    }

private:
    double x_, y_, width_, height_;
    Type type_;
};

class Zone {
public:
    Zone(int id, const wchar_t* name, double length, double width)
        : id_(id), length_(length), width_(width), cleanedPercentage_(0.0) {
        if (name) {
            name_ = name;
        }
    }

    int getId() const { return id_; }
    const wchar_t* getName() const { return name_.c_str(); }
    double getLength() const { return length_; }
    double getWidth() const { return width_; }
    double getArea() const { return length_ * width_; }

    void addObstacle(std::shared_ptr<Obstacle> obs) {
        if (obs) obstacles_.push_back(obs);
    }
    const std::vector<std::shared_ptr<Obstacle>>& getObstacles() const { return obstacles_; }

    double getCleanedPercentage() const { return cleanedPercentage_; }
    void setCleanedPercentage(double p) { cleanedPercentage_ = p; }
    bool isFullyCleaned() const { return cleanedPercentage_ >= 100.0; }

    void addTrailPoint(double x, double y, int roombaId) {
        std::lock_guard<std::mutex> lock(trailMutex_);
        if (trailPoints_.size() > 1500) {
            trailPoints_.erase(trailPoints_.begin(), trailPoints_.begin() + 300);
        }
        trailPoints_.push_back({ x, y, roombaId });
    }

    struct TrailPoint {
        double x, y;
        int roombaId;
    };

    std::vector<TrailPoint> getTrailPointsCopy() const {
        std::lock_guard<std::mutex> lock(trailMutex_);
        return trailPoints_;
    }

    void clearTrail() {
        std::lock_guard<std::mutex> lock(trailMutex_);
        trailPoints_.clear();
    }

private:
    int id_;
    std::wstring name_;
    double length_, width_;
    double cleanedPercentage_;
    std::vector<std::shared_ptr<Obstacle>> obstacles_;
    mutable std::mutex trailMutex_;
    std::vector<TrailPoint> trailPoints_;
};