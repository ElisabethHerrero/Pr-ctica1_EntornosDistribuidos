#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <queue>

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
    double x_;
    double y_;
    double width_;
    double height_;
    Type type_;
};

class Zone {
public:
    struct TrailPoint {
        double x;
        double y;
        int roombaId;
    };

    struct GridCell {
        int row;
        int col;
    };

    Zone(int id, const wchar_t* name, double length, double width)
        : id_(id),
        length_(length),
        width_(width),
        cellSize_(10.0),
        cleanedAccessibleCells_(0),
        totalAccessibleCells_(0),
        navigationClearance_(12.0) {
        name_ = name ? name : L"";
        initGrid();
    }

    int getId() const { return id_; }
    const wchar_t* getName() const { return name_.c_str(); }
    double getLength() const { return length_; }
    double getWidth() const { return width_; }
    double getArea() const { return length_ * width_; }
    double getCellSize() const { return cellSize_; }
    double getNavigationClearance() const { return navigationClearance_; }

    size_t getRows() const { return rows_; }
    size_t getCols() const { return cols_; }

    void addObstacle(std::shared_ptr<Obstacle> obs) {
        if (!obs) return;

        std::lock_guard<std::mutex> lock(mutex_);
        obstacles_.push_back(obs);
        rebuildAccessibilityLocked();
    }

    const std::vector<std::shared_ptr<Obstacle>>& getObstacles() const {
        return obstacles_;
    }

    void resetCleaning() {
        std::lock_guard<std::mutex> lock(mutex_);

        for (size_t r = 0; r < rows_; ++r) {
            for (size_t c = 0; c < cols_; ++c) {
                cleanedGrid_[r][c] = false;
            }
        }

        cleanedAccessibleCells_ = 0;
        trail_.clear();
    }

    void markClean(double x, double y, double radius) {
        std::lock_guard<std::mutex> lock(mutex_);

        int minCol = std::max(0, static_cast<int>((x - radius) / cellSize_));
        int maxCol = std::min(static_cast<int>(cols_) - 1, static_cast<int>((x + radius) / cellSize_));
        int minRow = std::max(0, static_cast<int>((y - radius) / cellSize_));
        int maxRow = std::min(static_cast<int>(rows_) - 1, static_cast<int>((y + radius) / cellSize_));

        for (int r = minRow; r <= maxRow; ++r) {
            for (int c = minCol; c <= maxCol; ++c) {
                if (!accessibleGrid_[r][c]) continue;

                double cx = (c + 0.5) * cellSize_;
                double cy = (r + 0.5) * cellSize_;
                double dx = cx - x;
                double dy = cy - y;

                if (dx * dx + dy * dy <= radius * radius) {
                    if (!cleanedGrid_[r][c]) {
                        cleanedGrid_[r][c] = true;
                        cleanedAccessibleCells_++;
                    }
                }
            }
        }
    }

    double getCleanedPercentage() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (totalAccessibleCells_ == 0) return 100.0;

        double pct = static_cast<double>(cleanedAccessibleCells_) * 100.0 /
            static_cast<double>(totalAccessibleCells_);
        if (pct < 0.0) pct = 0.0;
        if (pct > 100.0) pct = 100.0;
        return pct;
    }

    bool isFullyCleaned() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cleanedAccessibleCells_ >= totalAccessibleCells_;
    }

    std::vector<std::vector<bool>> getGrid() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cleanedGrid_;
    }

    void addTrail(double x, double y, int roombaId) {
        std::lock_guard<std::mutex> lock(mutex_);

        trail_.push_back({ x, y, roombaId });
        if (trail_.size() > 2500) {
            trail_.erase(trail_.begin(), trail_.begin() + 500);
        }
    }

    std::vector<TrailPoint> getTrail() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return trail_;
    }

    bool worldToCell(double x, double y, int& row, int& col) const {
        col = static_cast<int>(x / cellSize_);
        row = static_cast<int>(y / cellSize_);
        return row >= 0 && col >= 0 &&
            row < static_cast<int>(rows_) &&
            col < static_cast<int>(cols_);
    }

    void cellToWorldCenter(int row, int col, double& x, double& y) const {
        x = (col + 0.5) * cellSize_;
        y = (row + 0.5) * cellSize_;
    }

    bool isCellWalkable(int row, int col, double robotRadius) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return isCellWalkableLocked(row, col, robotRadius);
    }

    bool isCellClean(int row, int col) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (row < 0 || col < 0 || row >= static_cast<int>(rows_) || col >= static_cast<int>(cols_)) {
            return true;
        }
        return cleanedGrid_[row][col];
    }

    bool hasDirtyWalkableCell(double robotRadius) const {
        std::lock_guard<std::mutex> lock(mutex_);

        for (int r = 0; r < static_cast<int>(rows_); ++r) {
            for (int c = 0; c < static_cast<int>(cols_); ++c) {
                if (!accessibleGrid_[r][c]) continue;
                if (cleanedGrid_[r][c]) continue;
                if (!isCellWalkableLocked(r, c, robotRadius)) continue;
                return true;
            }
        }

        return false;
    }

    bool hasReachableDirtyCell(double fromX, double fromY, double robotRadius) const {
        std::lock_guard<std::mutex> lock(mutex_);

        int startRow = -1;
        int startCol = -1;
        if (!worldToCell(fromX, fromY, startRow, startCol)) {
            return false;
        }

        if (!isCellWalkableLocked(startRow, startCol, robotRadius)) {
            return false;
        }

        std::vector<std::vector<bool>> visited(rows_, std::vector<bool>(cols_, false));
        std::queue<GridCell> q;

        q.push({ startRow, startCol });
        visited[startRow][startCol] = true;

        static const int dr[4] = { -1, 1, 0, 0 };
        static const int dc[4] = { 0, 0, -1, 1 };

        while (!q.empty()) {
            GridCell cur = q.front();
            q.pop();

            if (accessibleGrid_[cur.row][cur.col] && !cleanedGrid_[cur.row][cur.col]) {
                return true;
            }

            for (int i = 0; i < 4; ++i) {
                int nr = cur.row + dr[i];
                int nc = cur.col + dc[i];

                if (nr < 0 || nc < 0 || nr >= static_cast<int>(rows_) || nc >= static_cast<int>(cols_)) continue;
                if (visited[nr][nc]) continue;
                if (!isCellWalkableLocked(nr, nc, robotRadius)) continue;

                visited[nr][nc] = true;
                q.push({ nr, nc });
            }
        }

        return false;
    }

    bool findNearestDirtyCell(double fromX, double fromY, double robotRadius,
        double& outX, double& outY) const {
        std::lock_guard<std::mutex> lock(mutex_);

        int startRow = -1;
        int startCol = -1;
        if (!worldToCell(fromX, fromY, startRow, startCol)) {
            return false;
        }

        if (!isCellWalkableLocked(startRow, startCol, robotRadius)) {
            return false;
        }

        std::vector<std::vector<bool>> visited(rows_, std::vector<bool>(cols_, false));
        std::queue<GridCell> q;

        q.push({ startRow, startCol });
        visited[startRow][startCol] = true;

        static const int dr[4] = { -1, 1, 0, 0 };
        static const int dc[4] = { 0, 0, -1, 1 };

        while (!q.empty()) {
            GridCell cur = q.front();
            q.pop();

            if (accessibleGrid_[cur.row][cur.col] && !cleanedGrid_[cur.row][cur.col]) {
                cellToWorldCenter(cur.row, cur.col, outX, outY);
                return true;
            }

            for (int i = 0; i < 4; ++i) {
                int nr = cur.row + dr[i];
                int nc = cur.col + dc[i];

                if (nr < 0 || nc < 0 || nr >= static_cast<int>(rows_) || nc >= static_cast<int>(cols_)) continue;
                if (visited[nr][nc]) continue;
                if (!isCellWalkableLocked(nr, nc, robotRadius)) continue;

                visited[nr][nc] = true;
                q.push({ nr, nc });
            }
        }

        return false;
    }

    bool findAnyDirtyCell(double robotRadius, double& outX, double& outY) const {
        std::lock_guard<std::mutex> lock(mutex_);

        for (int r = 0; r < static_cast<int>(rows_); ++r) {
            for (int c = 0; c < static_cast<int>(cols_); ++c) {
                if (!accessibleGrid_[r][c]) continue;
                if (cleanedGrid_[r][c]) continue;
                if (!isCellWalkableLocked(r, c, robotRadius)) continue;

                cellToWorldCenter(r, c, outX, outY);
                return true;
            }
        }

        return false;
    }

    bool findPathToNearestDirtyCell(double fromX, double fromY, double robotRadius,
        std::vector<GridCell>& outPath) const {
        outPath.clear();

        std::lock_guard<std::mutex> lock(mutex_);

        int startRow = -1;
        int startCol = -1;
        if (!worldToCell(fromX, fromY, startRow, startCol)) {
            return false;
        }

        if (!isCellWalkableLocked(startRow, startCol, robotRadius)) {
            return false;
        }

        std::vector<std::vector<bool>> visited(rows_, std::vector<bool>(cols_, false));
        std::vector<std::vector<GridCell>> parent(
            rows_,
            std::vector<GridCell>(cols_, { -1, -1 })
        );
        std::queue<GridCell> q;

        q.push({ startRow, startCol });
        visited[startRow][startCol] = true;

        static const int dr[4] = { -1, 1, 0, 0 };
        static const int dc[4] = { 0, 0, -1, 1 };

        GridCell target = { -1, -1 };

        while (!q.empty()) {
            GridCell cur = q.front();
            q.pop();

            if (accessibleGrid_[cur.row][cur.col] && !cleanedGrid_[cur.row][cur.col]) {
                target = cur;
                break;
            }

            for (int i = 0; i < 4; ++i) {
                int nr = cur.row + dr[i];
                int nc = cur.col + dc[i];

                if (nr < 0 || nc < 0 || nr >= static_cast<int>(rows_) || nc >= static_cast<int>(cols_)) continue;
                if (visited[nr][nc]) continue;
                if (!isCellWalkableLocked(nr, nc, robotRadius)) continue;

                visited[nr][nc] = true;
                parent[nr][nc] = cur;
                q.push({ nr, nc });
            }
        }

        if (target.row == -1) {
            return false;
        }

        std::vector<GridCell> reversePath;
        GridCell cur = target;

        while (!(cur.row == startRow && cur.col == startCol)) {
            reversePath.push_back(cur);
            GridCell p = parent[cur.row][cur.col];
            if (p.row == -1 && p.col == -1) break;
            cur = p;
        }

        std::reverse(reversePath.begin(), reversePath.end());
        outPath = reversePath;
        return !outPath.empty();
    }

private:
    int id_;
    std::wstring name_;
    double length_;
    double width_;

    mutable std::mutex mutex_;

    double cellSize_;
    size_t rows_;
    size_t cols_;

    size_t cleanedAccessibleCells_;
    size_t totalAccessibleCells_;
    double navigationClearance_;

    std::vector<std::vector<bool>> accessibleGrid_;
    std::vector<std::vector<bool>> cleanedGrid_;
    std::vector<TrailPoint> trail_;
    std::vector<std::shared_ptr<Obstacle>> obstacles_;

    void initGrid() {
        cols_ = std::max<size_t>(1, static_cast<size_t>(std::ceil(length_ / cellSize_)));
        rows_ = std::max<size_t>(1, static_cast<size_t>(std::ceil(width_ / cellSize_)));

        accessibleGrid_.assign(rows_, std::vector<bool>(cols_, true));
        cleanedGrid_.assign(rows_, std::vector<bool>(cols_, false));

        rebuildAccessibilityLocked();
    }

    void rebuildAccessibilityLocked() {
        totalAccessibleCells_ = 0;
        cleanedAccessibleCells_ = 0;

        for (int r = 0; r < static_cast<int>(rows_); ++r) {
            for (int c = 0; c < static_cast<int>(cols_); ++c) {
                double x = 0.0;
                double y = 0.0;
                cellToWorldCenter(r, c, x, y);

                bool accessible = true;

                if (x < navigationClearance_ || y < navigationClearance_ ||
                    x > length_ - navigationClearance_ || y > width_ - navigationClearance_) {
                    accessible = false;
                }

                for (const auto& obs : obstacles_) {
                    if (obs && obs->collidesWith(x, y, navigationClearance_)) {
                        accessible = false;
                        break;
                    }
                }

                accessibleGrid_[r][c] = accessible;

                if (!accessible) {
                    cleanedGrid_[r][c] = false;
                }
                else {
                    totalAccessibleCells_++;
                    if (cleanedGrid_[r][c]) {
                        cleanedAccessibleCells_++;
                    }
                }
            }
        }
    }

    bool isCellWalkableLocked(int row, int col, double robotRadius) const {
        if (row < 0 || col < 0 || row >= static_cast<int>(rows_) || col >= static_cast<int>(cols_)) {
            return false;
        }
        if (!accessibleGrid_[row][col]) {
            return false;
        }

        double x = 0.0;
        double y = 0.0;
        cellToWorldCenter(row, col, x, y);

        double minX = robotRadius;
        double minY = robotRadius;
        double maxX = length_ - robotRadius;
        double maxY = width_ - robotRadius;

        if (x < minX || x > maxX || y < minY || y > maxY) {
            return false;
        }

        for (const auto& obs : obstacles_) {
            if (obs && obs->collidesWith(x, y, robotRadius)) {
                return false;
            }
        }

        return true;
    }
};