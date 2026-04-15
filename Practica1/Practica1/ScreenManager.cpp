#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ScreenManager.h"
#include "CleaningService.h"
#include "Zone.h"
#include "Roomba.h"
#include "Database.h"
#include "EventService.h"

#include <windowsx.h>
#include <commctrl.h>
#include <random>
#include <cmath>
#include <algorithm>
#include <cwchar>
#include <queue>
#include <map>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")

#define ID_TIMER_PAINT 1001
#define ID_BTN_START 2001
#define ID_BTN_CONFIG 2002
#define ID_BTN_EXIT 2003
#define ID_BTN_BASIC 2004
#define ID_BTN_ADVANCED 2005
#define ID_BTN_PREMIUM 2006
#define ID_BTN_MINUS 2007
#define ID_BTN_PLUS 2008
#define ID_BTN_BEGIN 2009
#define ID_BTN_BACK 2010
#define ID_BTN_STOP 2011
#define ID_BTN_MENU 2012
#define ID_BTN_RESTART 2013
#define ID_BTN_EXIT_FINISH 2014

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
    struct RectD {
        double x;
        double y;
        double w;
        double h;
    };

    static RectD inflateRect(const RectD& r, double margin) {
        return {
            r.x - margin,
            r.y - margin,
            r.w + margin * 2.0,
            r.h + margin * 2.0
        };
    }

    static bool rectsIntersect(const RectD& a, const RectD& b) {
        return a.x < b.x + b.w &&
            a.x + a.w > b.x &&
            a.y < b.y + b.h &&
            a.y + a.h > b.y;
    }

    static bool collidesExpanded(double px, double py, const RectD& r, double radius) {
        return px + radius >= r.x &&
            px - radius <= r.x + r.w &&
            py + radius >= r.y &&
            py - radius <= r.y + r.h;
    }

    static bool isValidObstaclePlacement(
        double zoneLength,
        double zoneWidth,
        const std::vector<std::shared_ptr<Obstacle>>& existing,
        const RectD& candidate,
        double robotRadius,
        double cellSize
    ) {
        const double wallClearance = robotRadius * 2.2;
        const double obstacleClearance = robotRadius * 2.2;

        if (candidate.x < wallClearance || candidate.y < wallClearance) {
            return false;
        }
        if (candidate.x + candidate.w > zoneLength - wallClearance ||
            candidate.y + candidate.h > zoneWidth - wallClearance) {
            return false;
        }

        RectD expandedCandidate = inflateRect(candidate, obstacleClearance);

        for (const auto& obs : existing) {
            if (!obs) continue;
            RectD other{
                obs->getX(),
                obs->getY(),
                obs->getWidth(),
                obs->getHeight()
            };
            RectD expandedOther = inflateRect(other, obstacleClearance);

            if (rectsIntersect(expandedCandidate, expandedOther)) {
                return false;
            }
        }

        const int cols = std::max(1, static_cast<int>(std::ceil(zoneLength / cellSize)));
        const int rows = std::max(1, static_cast<int>(std::ceil(zoneWidth / cellSize)));

        std::vector<std::vector<bool>> freeGrid(rows, std::vector<bool>(cols, true));

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                double px = (c + 0.5) * cellSize;
                double py = (r + 0.5) * cellSize;

                bool walkable = true;

                if (px < robotRadius || py < robotRadius ||
                    px > zoneLength - robotRadius || py > zoneWidth - robotRadius) {
                    walkable = false;
                }

                if (walkable) {
                    for (const auto& obs : existing) {
                        if (!obs) continue;
                        RectD other{
                            obs->getX(),
                            obs->getY(),
                            obs->getWidth(),
                            obs->getHeight()
                        };
                        if (collidesExpanded(px, py, other, robotRadius)) {
                            walkable = false;
                            break;
                        }
                    }
                }

                if (walkable && collidesExpanded(px, py, candidate, robotRadius)) {
                    walkable = false;
                }

                freeGrid[r][c] = walkable;
            }
        }

        int startRow = -1;
        int startCol = -1;
        int freeCount = 0;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (freeGrid[r][c]) {
                    freeCount++;
                    if (startRow == -1) {
                        startRow = r;
                        startCol = c;
                    }
                }
            }
        }

        if (freeCount == 0) {
            return false;
        }

        std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
        std::queue<std::pair<int, int>> q;
        q.push({ startRow, startCol });
        visited[startRow][startCol] = true;
        int visitedCount = 0;

        const int dr[4] = { -1, 1, 0, 0 };
        const int dc[4] = { 0, 0, -1, 1 };

        while (!q.empty()) {
            auto cur = q.front();
            q.pop();
            visitedCount++;

            for (int i = 0; i < 4; ++i) {
                int nr = cur.first + dr[i];
                int nc = cur.second + dc[i];
                if (nr < 0 || nc < 0 || nr >= rows || nc >= cols) continue;
                if (visited[nr][nc]) continue;
                if (!freeGrid[nr][nc]) continue;
                visited[nr][nc] = true;
                q.push({ nr, nc });
            }
        }

        return visitedCount == freeCount;
    }
}

ScreenManager::ScreenManager()
    : hwnd_(nullptr),
    hInstance_(nullptr),
    currentScreen_(SCREEN_START),
    cleaningService_(std::make_unique<CleaningService>()),
    roombaCount_(3),
    roombaType_(Roomba::ADVANCED),
    gdiplusToken_(0),
    roombaImageBasic_(nullptr),
    roombaImageAdvanced_(nullptr),
    roombaImagePremium_(nullptr),
    dirtImage1_(nullptr),
    dirtImage2_(nullptr),
    dirtImage3_(nullptr),
    dirtImage4_(nullptr),
    nenufar1_(nullptr),
    nenufar2_(nullptr),
    tronco1_(nullptr),
    tronco2_(nullptr),
    mouseTracked_(false) {
    lastMousePos_ = { 0, 0 };
}

ScreenManager::~ScreenManager() {
    cleaningService_->stopCleaning();
    EventService::getInstance().clearSubscriptions();

    if (roombaImageBasic_) delete roombaImageBasic_;
    if (roombaImageAdvanced_) delete roombaImageAdvanced_;
    if (roombaImagePremium_) delete roombaImagePremium_;
    if (dirtImage1_) delete dirtImage1_;
    if (dirtImage2_) delete dirtImage2_;
    if (dirtImage3_) delete dirtImage3_;
    if (dirtImage4_) delete dirtImage4_;
    if (nenufar1_) delete nenufar1_;
    if (nenufar2_) delete nenufar2_;
    if (tronco1_) delete tronco1_;
    if (tronco2_) delete tronco2_;

    if (gdiplusToken_) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
    }
}

bool ScreenManager::initialize() {
    hInstance_ = GetModuleHandle(nullptr);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr) != Gdiplus::Ok) {
        MessageBox(nullptr, L"Error al inicializar GDI+", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!loadImages()) {
        MessageBox(nullptr, L"Error: No se pudieron cargar todas las imagenes\nAsegurate de que esten en la carpeta del ejecutable",
            L"Advertencia", MB_OK | MB_ICONWARNING);
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"FroombaScreenClass";

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance_;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(COLOR_BG_DARK);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassEx(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 1400;
    int winH = 900;

    hwnd_ = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Sistema Distribuido Froomba - Limpieza de Estanques",
        WS_OVERLAPPEDWINDOW,
        (screenW - winW) / 2,
        (screenH - winH) / 2,
        winW,
        winH,
        nullptr,
        nullptr,
        hInstance_,
        this
    );

    if (!hwnd_) {
        return false;
    }

    Database::getInstance().initialize();

    EventService::getInstance().clearSubscriptions();
    EventService::getInstance().subscribe([this](const wchar_t* msg) {
        this->addLog(msg);
        });

    initializeZones();

    cleaningService_->setZones(&zones_);
    cleaningService_->setRoombas(&roombas_);

    SetTimer(hwnd_, ID_TIMER_PAINT, 33, nullptr);

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    addLog(L"Sistema de limpieza acuatica iniciado");

    return true;
}

bool ScreenManager::loadImages() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileName(nullptr, exePath, MAX_PATH);

    std::wstring exeDir = exePath;
    size_t pos = exeDir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        exeDir = exeDir.substr(0, pos + 1);
    }

    bool allLoaded = true;

    roombaImageBasic_ = Gdiplus::Image::FromFile((exeDir + L"rana3.png").c_str());
    if (!roombaImageBasic_ || roombaImageBasic_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    roombaImageAdvanced_ = Gdiplus::Image::FromFile((exeDir + L"rana.png").c_str());
    if (!roombaImageAdvanced_ || roombaImageAdvanced_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    roombaImagePremium_ = Gdiplus::Image::FromFile((exeDir + L"rana2.png").c_str());
    if (!roombaImagePremium_ || roombaImagePremium_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    dirtImage1_ = Gdiplus::Image::FromFile((exeDir + L"suciedad.png").c_str());
    if (!dirtImage1_ || dirtImage1_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    dirtImage2_ = Gdiplus::Image::FromFile((exeDir + L"suciedad2.png").c_str());
    if (!dirtImage2_ || dirtImage2_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    dirtImage3_ = Gdiplus::Image::FromFile((exeDir + L"suciedad3.png").c_str());
    if (!dirtImage3_ || dirtImage3_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    dirtImage4_ = Gdiplus::Image::FromFile((exeDir + L"suciedad4.png").c_str());
    if (!dirtImage4_ || dirtImage4_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    nenufar1_ = Gdiplus::Image::FromFile((exeDir + L"nenufar.png").c_str());
    if (!nenufar1_ || nenufar1_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    nenufar2_ = Gdiplus::Image::FromFile((exeDir + L"nenufar2.png").c_str());
    if (!nenufar2_ || nenufar2_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    tronco1_ = Gdiplus::Image::FromFile((exeDir + L"tronco.png").c_str());
    if (!tronco1_ || tronco1_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    tronco2_ = Gdiplus::Image::FromFile((exeDir + L"tronco2.png").c_str());
    if (!tronco2_ || tronco2_->GetLastStatus() != Gdiplus::Ok) allLoaded = false;

    return allLoaded;
}

void ScreenManager::drawRoombaImage(Gdiplus::Graphics& graphics, double x, double y,
    double angle, COLORREF tintColor, int size, Roomba::Type type) {
    Gdiplus::Image* img = nullptr;

    switch (type) {
    case Roomba::BASIC:
        img = roombaImageBasic_;
        break;
    case Roomba::ADVANCED:
        img = roombaImageAdvanced_;
        break;
    case Roomba::PREMIUM:
        img = roombaImagePremium_;
        break;
    }

    if (!img) return;

    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    Gdiplus::GraphicsState state = graphics.Save();

    graphics.TranslateTransform(static_cast<Gdiplus::REAL>(x), static_cast<Gdiplus::REAL>(y));

    float angleDegrees = static_cast<float>(angle * 180.0 / M_PI);
    graphics.RotateTransform(angleDegrees);

    int drawX = -size / 2;
    int drawY = -size / 2;

    Gdiplus::ColorMatrix colorMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };

    Gdiplus::ImageAttributes imageAttr;
    imageAttr.SetColorMatrix(&colorMatrix);

    Gdiplus::Rect destRect(drawX, drawY, size, size);
    graphics.DrawImage(
        img,
        destRect,
        0, 0, img->GetWidth(), img->GetHeight(),
        Gdiplus::UnitPixel,
        &imageAttr
    );

    graphics.Restore(state);
}

void ScreenManager::run() {
    MSG msg = { 0 };
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void ScreenManager::initializeZones() {
    zones_.clear();
    zones_.push_back(std::make_shared<Zone>(1, L"Laguna", 500.0, 150.0));
    zones_.push_back(std::make_shared<Zone>(2, L"Arroyo", 480.0, 101.0));
    zones_.push_back(std::make_shared<Zone>(3, L"Lago", 309.0, 480.0));
    zones_.push_back(std::make_shared<Zone>(4, L"Charca", 90.0, 220.0));

    std::random_device rd;
    std::mt19937 gen(rd());

    const double maxRobotRadius = 20.0;
    const double minClearance = maxRobotRadius * 2.2;
    const double navCellSize = 10.0;

    for (size_t i = 0; i < zones_.size(); ++i) {
        auto& zone = zones_[i];
        if (!zone) continue;

        const double zoneLength = zone->getLength();
        const double zoneWidth = zone->getWidth();
        const double minDimension = std::min(zoneLength, zoneWidth);
        const double zoneArea = zone->getArea();

        int numObs = 1;

        if (zoneArea > 100000.0) {
            numObs = 3;
        }
        else if (zoneArea > 50000.0) {
            numObs = 2;
        }
        else {
            numObs = 1;
        }

        double minObsW, maxObsW, minObsH, maxObsH;

        if (minDimension > 300.0) {
            minObsW = 35.0;
            maxObsW = 55.0;
            minObsH = 35.0;
            maxObsH = 55.0;
        }
        else if (minDimension > 200.0) {
            minObsW = 25.0;
            maxObsW = 45.0;
            minObsH = 25.0;
            maxObsH = 45.0;
        }
        else if (minDimension > 150.0) {
            minObsW = 20.0;
            maxObsW = 35.0;
            minObsH = 20.0;
            maxObsH = 35.0;
        }
        else if (minDimension > 100.0) {
            minObsW = 18.0;
            maxObsW = 30.0;
            minObsH = 18.0;
            maxObsH = 30.0;
        }
        else {
            minObsW = 15.0;
            maxObsW = 22.0;
            minObsH = 15.0;
            maxObsH = 22.0;
        }

        std::uniform_real_distribution<double> disW(minObsW, maxObsW);
        std::uniform_real_distribution<double> disH(minObsH, maxObsH);
        std::uniform_int_distribution<int> obsTypeDist(0, 1);

        int created = 0;
        int attempts = 0;
        const int maxAttempts = 500;

        while (created < numObs && attempts < maxAttempts) {
            attempts++;

            bool isSquare = (obsTypeDist(gen) == 0);
            double ow, oh;
            int imageType;

            if (isSquare) {
                double size = (disW(gen) + disH(gen)) / 2.0;
                ow = size;
                oh = size;
                imageType = (gen() % 2 == 0) ? 0 : 1;
            }
            else {
                ow = disW(gen) * 0.5;
                oh = disH(gen) * 1.6;
                imageType = (gen() % 2 == 0) ? 2 : 3;
            }

            const double wallClearance = minClearance;
            const double obstacleClearance = minClearance;

            if (zoneLength - ow - wallClearance * 2.0 <= 0.0 ||
                zoneWidth - oh - wallClearance * 2.0 <= 0.0) {
                ow = std::min(ow, zoneLength - wallClearance * 2.5);
                oh = std::min(oh, zoneWidth - wallClearance * 2.5);

                if (ow < 10.0 || oh < 10.0) {
                    continue;
                }
            }

            std::uniform_real_distribution<double> disX(
                wallClearance,
                std::max(wallClearance + 1.0, zoneLength - ow - wallClearance)
            );
            std::uniform_real_distribution<double> disY(
                wallClearance,
                std::max(wallClearance + 1.0, zoneWidth - oh - wallClearance)
            );

            RectD candidate{
                disX(gen),
                disY(gen),
                ow,
                oh
            };

            if (!isValidObstaclePlacement(
                zoneLength,
                zoneWidth,
                zone->getObstacles(),
                candidate,
                maxRobotRadius,
                navCellSize)) {
                continue;
            }

            zone->addObstacle(std::make_shared<Obstacle>(
                candidate.x,
                candidate.y,
                candidate.w,
                candidate.h,
                Obstacle::FURNITURE,
                imageType
            ));
            created++;
        }

        if (created == 0 && zoneArea > 20000.0) {
            double safeW = std::min(25.0, zoneLength * 0.15);
            double safeH = std::min(25.0, zoneWidth * 0.15);
            double safeX = (zoneLength - safeW) / 2.0;
            double safeY = (zoneWidth - safeH) / 2.0;

            zone->addObstacle(std::make_shared<Obstacle>(
                safeX,
                safeY,
                safeW,
                safeH,
                Obstacle::FURNITURE,
                0
            ));
        }
    }
}

void ScreenManager::resetZones() {
    if (!zones_.empty()) {
        for (auto& zone : zones_) {
            if (zone) {
                zone->resetCleaning();
            }
        }
    }
}

void ScreenManager::initializeRoombas() {
    cleaningService_->stopCleaning();
    roombas_.clear();
    resetZones();
    lastRoombaAngles_.clear();

    for (int i = 1; i <= roombaCount_; i++) {
        auto roomba = std::make_shared<Roomba>(i, static_cast<Roomba::Type>(roombaType_));
        roomba->setCurrentZone(nullptr);
        roomba->setState(Roomba::IDLE);
        roomba->setPosition(10.0, 10.0);
        roomba->setAngle(0.0);
        roombas_.push_back(roomba);
        lastRoombaAngles_[i] = 0.0;
    }

    wchar_t msg[128];
    const wchar_t* typeName = L"";
    switch (roombaType_) {
    case Roomba::BASIC: typeName = L"Renacuajo"; break;
    case Roomba::ADVANCED: typeName = L"Rana"; break;
    case Roomba::PREMIUM: typeName = L"Sapo"; break;
    }

    swprintf_s(msg, L"Equipo listo: %d Froombas tipo %s", roombaCount_, typeName);
    addLog(msg);
}

void ScreenManager::changeScreen(int screenId) {
    currentScreen_ = screenId;
    clearButtons();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void ScreenManager::addLog(const wchar_t* msg) {
    if (!msg) return;

    std::lock_guard<std::mutex> lock(logMutex_);
    logs_.push_back(std::wstring(msg));
    if (logs_.size() > 40) {
        logs_.erase(logs_.begin());
    }
    Database::getInstance().logEvent(msg);
}

LRESULT CALLBACK ScreenManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ScreenManager* mgr = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        mgr = reinterpret_cast<ScreenManager*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(mgr));
        if (mgr) {
            mgr->hwnd_ = hwnd;
        }
    }
    else {
        mgr = reinterpret_cast<ScreenManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (mgr) {
        return mgr->handleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT ScreenManager::handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);

        RECT rect;
        GetClientRect(hwnd_, &rect);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        paintScreen(memDC, rect);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_PAINT) {
            if (currentScreen_ == SCREEN_CLEANING &&
                !cleaningService_->isRunning()) {

                bool allComplete = true;
                for (const auto& zone : zones_) {
                    if (zone && !zone->isFullyCleaned()) {
                        allComplete = false;
                        break;
                    }
                }

                if (allComplete) {
                    changeScreen(SCREEN_FINISHED);
                    addLog(L"¡Todos los estanques cristalinos!");
                }
            }

            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (!mouseTracked_) {
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT) };
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd_;
            TrackMouseEvent(&tme);
            mouseTracked_ = true;
        }

        updateButtonHover(x, y);
        return 0;
    }

    case WM_MOUSELEAVE:
        mouseTracked_ = false;
        for (auto& btn : buttons_) {
            btn.isHovered = false;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int btnId = hitTestButton(x, y);
        if (btnId > 0) {
            handleButtonClick(btnId);
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd_, ID_TIMER_PAINT);
        cleaningService_->stopCleaning();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd_, uMsg, wParam, lParam);
}

void ScreenManager::paintScreen(HDC hdc, RECT& rect) {
    HBRUSH bgBrush = CreateSolidBrush(COLOR_BG_DARK);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    clearButtons();

    switch (currentScreen_) {
    case SCREEN_START:
        paintStartScreen(hdc, rect);
        break;
    case SCREEN_CONFIG:
        paintConfigScreen(hdc, rect);
        break;
    case SCREEN_CLEANING:
        paintCleaningScreen(hdc, rect);
        break;
    case SCREEN_FINISHED:
        paintFinishedScreen(hdc, rect);
        break;
    }
}

void ScreenManager::paintStartScreen(HDC hdc, RECT& rect) {
    int centerX = rect.right / 2;
    int centerY = rect.bottom / 2;

    SIZE textSize;

    // "SISTEMA DISTRIBUIDO"
    HFONT font1 = CreateFont(34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, font1);
    GetTextExtentPoint32(hdc, L"SISTEMA DISTRIBUIDO", 19, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font1);
    drawText(hdc, L"SISTEMA DISTRIBUIDO", centerX - textSize.cx / 2, centerY - 180, COLOR_TEXT, 34, true);

    // "FROOMBA"
    HFONT font2 = CreateFont(60, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, font2);
    GetTextExtentPoint32(hdc, L"FROOMBA", 7, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font2);
    drawText(hdc, L"FROOMBA", centerX - textSize.cx / 2, centerY - 100, COLOR_PRIMARY, 60, true);

    // "Convertimos tu charca en un estanque"
    HFONT font3 = CreateFont(18, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, font3);
    GetTextExtentPoint32(hdc, L"Convertimos tu charca en un estanque", 36, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font3);
    drawText(hdc, L"Convertimos tu charca en un estanque", centerX - textSize.cx / 2, centerY - 30, COLOR_SUCCESS, 18, false);

    // Subtítulo
    HFONT font4 = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, font4);
    GetTextExtentPoint32(hdc, L"Programacion en Entornos Distribuidos", 38, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font4);
    drawText(hdc, L"Programacion en Entornos Distribuidos", centerX - textSize.cx / 2, centerY + 5, COLOR_TEXT_DIM, 14, false);

    int btnY = centerY + 60;
    addButton(ID_BTN_START, centerX - 150, btnY, 300, 58, L"INICIAR LIMPIEZA", COLOR_SUCCESS);
    addButton(ID_BTN_CONFIG, centerX - 150, btnY + 72, 300, 58, L"CONFIGURAR EQUIPO", COLOR_PRIMARY);
    addButton(ID_BTN_EXIT, centerX - 150, btnY + 144, 300, 58, L"SALIR", COLOR_DANGER);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }

    HFONT font5 = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, font5);
    GetTextExtentPoint32(hdc, L"Nuestras ranas trabajadoras mantienen tu ecosistema acuatico impecable", 70, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font5);
    drawText(hdc, L"Nuestras ranas trabajadoras mantienen tu ecosistema acuatico impecable",
        centerX - textSize.cx / 2, rect.bottom - 55, COLOR_TEXT_DIM, 13, false);

    drawText(hdc, L"v3.0", rect.right - 60, rect.bottom - 30, COLOR_TEXT_DIM, 12, false);
}

void ScreenManager::paintConfigScreen(HDC hdc, RECT& rect) {
    int centerX = rect.right / 2;

    SIZE textSize;
    HFONT fontTitle = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, fontTitle);
    GetTextExtentPoint32(hdc, L"CONFIGURACION DEL EQUIPO", 24, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(fontTitle);
    drawText(hdc, L"CONFIGURACION DEL EQUIPO", centerX - textSize.cx / 2, 30, COLOR_TEXT, 28, true);

    drawRoundRect(hdc, centerX - 340, 80, 680, 530, 16, COLOR_BG_LIGHT, COLOR_TEXT);

    drawText(hdc, L"Numero de Froombas:", centerX - 295, 120, COLOR_TEXT, 18, true);
    addButton(ID_BTN_MINUS, centerX - 90, 155, 52, 52, L"-", COLOR_DANGER);

    wchar_t countStr[8];
    swprintf_s(countStr, L"%d", roombaCount_);
    SIZE numSize;
    HFONT fontNum = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, fontNum);
    GetTextExtentPoint32(hdc, countStr, static_cast<int>(wcslen(countStr)), &numSize);
    SelectObject(hdc, oldFont);
    DeleteObject(fontNum);
    drawText(hdc, countStr, centerX - numSize.cx / 2, 168, COLOR_PRIMARY, 28, true);

    addButton(ID_BTN_PLUS, centerX + 38, 155, 52, 52, L"+", COLOR_SUCCESS);

    drawText(hdc, L"Tipo de Froomba:", centerX - 295, 230, COLOR_TEXT, 18, true);

    COLORREF c1 = (roombaType_ == Roomba::BASIC) ? COLOR_PRIMARY : RGB(0xD0, 0xD0, 0xC0);
    COLORREF c2 = (roombaType_ == Roomba::ADVANCED) ? COLOR_SUCCESS : RGB(0xD0, 0xD0, 0xC0);
    COLORREF c3 = (roombaType_ == Roomba::PREMIUM) ? COLOR_DANGER : RGB(0xD0, 0xD0, 0xC0);

    Gdiplus::Graphics graphics(hdc);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    int imgSize = 50;
    int imgY = 270;

    if (roombaImageBasic_) {
        int imgX1 = centerX - 295 + (190 - imgSize) / 2;
        graphics.DrawImage(roombaImageBasic_, imgX1, imgY, imgSize, imgSize);
    }

    if (roombaImageAdvanced_) {
        int imgX2 = centerX - 95 + (190 - imgSize) / 2;
        graphics.DrawImage(roombaImageAdvanced_, imgX2, imgY, imgSize, imgSize);
    }

    if (roombaImagePremium_) {
        int imgX3 = centerX + 105 + (190 - imgSize) / 2;
        graphics.DrawImage(roombaImagePremium_, imgX3, imgY, imgSize, imgSize);
    }

    addButton(ID_BTN_BASIC, centerX - 295, 335, 190, 65, L"RENACUAJO", c1);
    addButton(ID_BTN_ADVANCED, centerX - 95, 335, 190, 65, L"RANA", c2);
    addButton(ID_BTN_PREMIUM, centerX + 105, 335, 190, 65, L"SAPO", c3);

    drawText(hdc, L"Estanques disponibles:", centerX - 295, 430, COLOR_TEXT, 16, true);

    COLORREF zoneColors[] = { COLOR_ZONE1, COLOR_ZONE2, COLOR_ZONE3, COLOR_ZONE4 };
    for (size_t i = 0; i < zones_.size(); i++) {
        auto& zone = zones_[i];
        if (!zone) continue;

        int zy = 460 + static_cast<int>(i) * 28;

        HBRUSH zb = CreateSolidBrush(zoneColors[i]);
        RECT zr = { centerX - 295, zy + 2, centerX - 278, zy + 19 };
        FillRect(hdc, &zr, zb);
        DeleteObject(zb);

        wchar_t info[160];
        swprintf_s(info, L"%s - %.0f cm x %.0f cm (%.0f cm2)",
            zone->getName(), zone->getLength(), zone->getWidth(), zone->getArea());
        drawText(hdc, info, centerX - 270, zy, COLOR_TEXT, 12, false);
    }

    addButton(ID_BTN_BACK, centerX - 295, 580, 190, 46, L"VOLVER", COLOR_TEXT_DIM);
    addButton(ID_BTN_BEGIN, centerX + 105, 580, 190, 46, L"¡AL AGUA!", COLOR_SUCCESS);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }
}

void ScreenManager::paintCleaningScreen(HDC hdc, RECT& rect) {
    SIZE textSize;
    HFONT fontTitle = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, fontTitle);
    GetTextExtentPoint32(hdc, L"LIMPIEZA EN PROGRESO", 20, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(fontTitle);
    drawText(hdc, L"LIMPIEZA EN PROGRESO", (rect.right - textSize.cx) / 2, 12, COLOR_TEXT, 22, true);

    int zoneW = 400;
    int zoneH = 280;
    int gap = 15;
    int startX = 20;
    int startY = 50;

    COLORREF zoneColors[] = { COLOR_ZONE1, COLOR_ZONE2, COLOR_ZONE3, COLOR_ZONE4 };

    for (size_t i = 0; i < zones_.size(); i++) {
        auto& zone = zones_[i];
        if (!zone) continue;

        int col = static_cast<int>(i % 2);
        int row = static_cast<int>(i / 2);
        int x = startX + col * (zoneW + gap);
        int y = startY + row * (zoneH + gap);

        drawZoneMiniMap(hdc, zone, x, y, zoneW, zoneH, zoneColors[i], i);

        for (size_t ri = 0; ri < roombas_.size(); ri++) {
            auto& roomba = roombas_[ri];
            if (!roomba) continue;
            if (roomba->getCurrentZone() != zone) continue;

            drawRoombaOnZone(hdc, roomba, zone, x, y, zoneW, zoneH);
        }
    }

    int panelX = 855;
    int panelY = 50;
    int panelW = 520;
    int panelH = 625;

    drawRoundRect(hdc, panelX, panelY, panelW, panelH, 12, COLOR_BG_LIGHT, COLOR_TEXT);

    bool isRunning = cleaningService_->isRunning();
    drawText(hdc, L"Estado:", panelX + 15, panelY + 15, COLOR_TEXT, 16, true);
    drawText(
        hdc,
        isRunning ? L"NADANDO" : L"EN REPOSO",
        panelX + 92,
        panelY + 15,
        isRunning ? COLOR_SUCCESS : COLOR_WARNING,
        16,
        true
    );

    double totalProg = 0.0;
    int validZones = 0;
    for (size_t i = 0; i < zones_.size(); i++) {
        if (zones_[i]) {
            totalProg += zones_[i]->getCleanedPercentage();
            validZones++;
        }
    }
    if (validZones > 0) {
        totalProg /= validZones;
    }

    drawText(hdc, L"Progreso Total:", panelX + 15, panelY + 50, COLOR_TEXT, 14, false);
    drawProgressBar(hdc, panelX + 15, panelY + 72, panelW - 30, 22, totalProg, COLOR_DANGER);

    wchar_t progStr[24];
    swprintf_s(progStr, L"%.1f%%", totalProg);
    SIZE progSize;
    HFONT fontProg = CreateFont(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, fontProg);
    GetTextExtentPoint32(hdc, progStr, static_cast<int>(wcslen(progStr)), &progSize);
    SelectObject(hdc, oldFont);
    DeleteObject(fontProg);
    drawText(hdc, progStr, panelX + (panelW - progSize.cx) / 2, panelY + 76, COLOR_BG_DARK, 12, true);

    drawText(hdc, L"Por Estanque:", panelX + 15, panelY + 112, COLOR_TEXT, 14, true);
    for (size_t i = 0; i < zones_.size(); i++) {
        auto& z = zones_[i];
        if (!z) continue;

        int zy = panelY + 138 + static_cast<int>(i) * 42;
        drawText(hdc, z->getName(), panelX + 15, zy, COLOR_TEXT, 12, false);
        drawProgressBar(hdc, panelX + 15, zy + 17, panelW - 30, 15, z->getCleanedPercentage(), zoneColors[i]);

        wchar_t pctStr[16];
        swprintf_s(pctStr, L"%.1f%%", z->getCleanedPercentage());
        drawText(hdc, pctStr, panelX + panelW - 58, zy - 1, zoneColors[i], 12, true);
    }

    drawText(hdc, L"Equipo Froomba:", panelX + 15, panelY + 320, COLOR_TEXT, 14, true);
    for (size_t i = 0; i < roombas_.size(); i++) {
        auto& r = roombas_[i];
        if (!r) continue;

        int ry = panelY + 346 + static_cast<int>(i) * 28;

        COLORREF dotColor = r->getColor();
        if (r->getStuckCounter() > 10) {
            dotColor = RGB(255, 140, 0);
        }

        HBRUSH rb = CreateSolidBrush(dotColor);
        SelectObject(hdc, rb);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, panelX + 15, ry, panelX + 30, ry + 15);
        DeleteObject(rb);

        wchar_t info[128];
        auto zone = r->getCurrentZone();

        const wchar_t* stateName = L"Inactiva";
        switch (r->getState()) {
        case Roomba::IDLE: stateName = L"Descansando"; break;
        case Roomba::CLEANING: stateName = L"Limpiando"; break;
        case Roomba::MOVING: stateName = L"Nadando"; break;
        case Roomba::FINISHED: stateName = L"Finalizada"; break;
        }

        if (zone) {
            swprintf_s(info, L"#%d - %s - %s", r->getId(), stateName, zone->getName());
        }
        else {
            swprintf_s(info, L"#%d - %s", r->getId(), stateName);
        }
        drawText(hdc, info, panelX + 38, ry, COLOR_TEXT, 11, false);
    }

    drawText(hdc, L"Registro de actividad:", panelX + 15, panelY + 470, COLOR_TEXT, 14, true);
    {
        std::lock_guard<std::mutex> lock(logMutex_);
        int logY = panelY + 498;
        int cnt = 0;
        for (int idx = static_cast<int>(logs_.size()) - 1; idx >= 0 && cnt < 7; idx--, cnt++) {
            drawText(hdc, logs_[idx].c_str(), panelX + 15, logY + cnt * 16, COLOR_TEXT_DIM, 10, false);
        }
    }

    addButton(
        ID_BTN_STOP,
        panelX + 15,
        panelY + 580,
        160,
        40,
        isRunning ? L"DETENER" : L"REANUDAR",
        isRunning ? COLOR_DANGER : COLOR_SUCCESS
    );
    addButton(ID_BTN_MENU, panelX + panelW - 175, panelY + 580, 160, 40, L"MENU", COLOR_TEXT_DIM);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }
}

void ScreenManager::paintFinishedScreen(HDC hdc, RECT& rect) {
    int centerX = rect.right / 2;
    int centerY = rect.bottom / 2;

    drawRoundRect(hdc, centerX - 350, centerY - 300, 700, 600, 20, COLOR_BG_LIGHT, COLOR_SUCCESS);

    SIZE textSize;

    // "¡ESTANQUES"
    HFONT font1 = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, font1);
    GetTextExtentPoint32(hdc, L"¡ESTANQUES", 10, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font1);
    drawText(hdc, L"¡ESTANQUES", centerX - textSize.cx / 2, centerY - 240, COLOR_SUCCESS, 48, true);

    // "CRISTALINOS!"
    font1 = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, font1);
    GetTextExtentPoint32(hdc, L"CRISTALINOS!", 12, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font1);
    drawText(hdc, L"CRISTALINOS!", centerX - textSize.cx / 2, centerY - 180, COLOR_SUCCESS, 48, true);

    HBRUSH checkBrush = CreateSolidBrush(COLOR_SUCCESS);
    HPEN checkPen = CreatePen(PS_SOLID, 4, RGB(255, 255, 255));
    SelectObject(hdc, checkBrush);
    SelectObject(hdc, checkPen);
    Ellipse(hdc, centerX - 60, centerY - 100, centerX + 60, centerY + 20);
    DeleteObject(checkBrush);

    HPEN whitePen = CreatePen(PS_SOLID, 8, RGB(255, 255, 255));
    SelectObject(hdc, whitePen);
    MoveToEx(hdc, centerX - 30, centerY - 40, nullptr);
    LineTo(hdc, centerX - 10, centerY - 15);
    LineTo(hdc, centerX + 35, centerY - 70);
    DeleteObject(whitePen);
    DeleteObject(checkPen);

    // "Resumen del servicio:"
    HFONT font2 = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, font2);
    GetTextExtentPoint32(hdc, L"Resumen del servicio:", 21, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font2);
    drawText(hdc, L"Resumen del servicio:", centerX - textSize.cx / 2, centerY + 50, COLOR_TEXT, 18, true);

    COLORREF zoneColors[] = { COLOR_ZONE1, COLOR_ZONE2, COLOR_ZONE3, COLOR_ZONE4 };
    int yOffset = centerY + 90;

    for (size_t i = 0; i < zones_.size(); i++) {
        auto& zone = zones_[i];
        if (!zone) continue;

        HBRUSH zb = CreateSolidBrush(zoneColors[i]);
        RECT zr = { centerX - 280, yOffset + 2, centerX - 260, yOffset + 20 };
        FillRect(hdc, &zr, zb);
        DeleteObject(zb);

        wchar_t info[128];
        swprintf_s(info, L"%s - %.1f%% purificada",
            zone->getName(),
            zone->getCleanedPercentage());
        drawText(hdc, info, centerX - 250, yOffset, COLOR_TEXT, 14, false);

        yOffset += 30;
    }

    yOffset += 20;
    wchar_t roombaInfo[64];
    swprintf_s(roombaInfo, L"Froombas trabajadoras: %d", static_cast<int>(roombas_.size()));
    drawText(hdc, roombaInfo, centerX - 280, yOffset, COLOR_TEXT_DIM, 14, false);

    yOffset += 25;
    wchar_t typeInfo[64];
    if (!roombas_.empty() && roombas_[0]) {
        const wchar_t* typeName = L"";
        switch (roombas_[0]->getType()) {
        case Roomba::BASIC: typeName = L"Renacuajo"; break;
        case Roomba::ADVANCED: typeName = L"Rana"; break;
        case Roomba::PREMIUM: typeName = L"Sapo"; break;
        }
        swprintf_s(typeInfo, L"Tipo: %s", typeName);
        drawText(hdc, typeInfo, centerX - 280, yOffset, COLOR_TEXT_DIM, 14, false);
    }

    int btnY = centerY + 230;
    addButton(ID_BTN_RESTART, centerX - 280, btnY, 260, 55, L"NUEVO SERVICIO", COLOR_PRIMARY);
    addButton(ID_BTN_EXIT_FINISH, centerX + 20, btnY, 260, 55, L"SALIR", COLOR_DANGER);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }

    // Mensaje final creativo
    HFONT font3 = CreateFont(14, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, font3);
    GetTextExtentPoint32(hdc, L"Tu ecosistema acuatico brilla como el agua de manantial", 56, &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font3);
    drawText(hdc, L"Tu ecosistema acuatico brilla como el agua de manantial",
        centerX - textSize.cx / 2, rect.bottom - 60, COLOR_TEXT_DIM, 14, false);
}

void ScreenManager::drawZoneMiniMap(
    HDC hdc,
    std::shared_ptr<Zone> zone,
    int x,
    int y,
    int w,
    int h,
    COLORREF accentColor,
    size_t zoneIndex
) {
    if (!zone) return;

    int innerX = x + 10;
    int innerY = y + 30;
    int innerW = w - 20;
    int innerH = h - 40;

    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

    // PASO 1: FONDO DE SUCIEDAD (CAPA INFERIOR)
    Gdiplus::Image* dirtImg = nullptr;
    switch (zoneIndex) {
    case 0: dirtImg = dirtImage1_; break;
    case 1: dirtImg = dirtImage2_; break;
    case 2: dirtImg = dirtImage3_; break;
    case 3: dirtImg = dirtImage4_; break;
    }

    if (dirtImg) {
        graphics.DrawImage(dirtImg, innerX, innerY, innerW, innerH);
    }
    else {
        HBRUSH floorBrush = CreateSolidBrush(RGB(0xE8, 0xE3, 0xCC));
        RECT floorRect = { innerX, innerY, innerX + innerW, innerY + innerH };
        FillRect(hdc, &floorRect, floorBrush);
        DeleteObject(floorBrush);
    }

    double scaleX = innerW / zone->getLength();
    double scaleY = innerH / zone->getWidth();

    // PASO 2: TRAZO CONTINUO CON CÍRCULOS INTERPOLADOS (OPACO)
    auto trail = zone->getTrail();

    if (!trail.empty()) {
        Gdiplus::SolidBrush cleanBrush(Gdiplus::Color(255, 0xCE, 0xE7, 0xCA));
        const float circleRadius = 8.0f;
        size_t start = (trail.size() > 2000) ? trail.size() - 2000 : 0;

        for (size_t j = start; j < trail.size(); j++) {
            auto& pt = trail[j];

            Gdiplus::REAL cx = static_cast<Gdiplus::REAL>(innerX + pt.x * scaleX);
            Gdiplus::REAL cy = static_cast<Gdiplus::REAL>(innerY + pt.y * scaleY);

            graphics.FillEllipse(&cleanBrush,
                cx - circleRadius,
                cy - circleRadius,
                circleRadius * 2.0f,
                circleRadius * 2.0f);

            if (j > start) {
                auto& prevPt = trail[j - 1];

                Gdiplus::REAL prevX = static_cast<Gdiplus::REAL>(innerX + prevPt.x * scaleX);
                Gdiplus::REAL prevY = static_cast<Gdiplus::REAL>(innerY + prevPt.y * scaleY);

                float dx = cx - prevX;
                float dy = cy - prevY;
                float dist = std::sqrt(dx * dx + dy * dy);

                const float stepSize = 3.0f;
                int numSteps = static_cast<int>(dist / stepSize);

                for (int step = 1; step < numSteps; ++step) {
                    float t = static_cast<float>(step) / numSteps;
                    float interpX = prevX + dx * t;
                    float interpY = prevY + dy * t;

                    graphics.FillEllipse(&cleanBrush,
                        interpX - circleRadius,
                        interpY - circleRadius,
                        circleRadius * 2.0f,
                        circleRadius * 2.0f);
                }
            }
        }
    }

    // PASO 3: ÁREA LIMPIA DE CELDAS (OPACO)
    auto cleanedGrid = zone->getGrid();
    double cellSize = zone->getCellSize();
    Gdiplus::SolidBrush cellBrush(Gdiplus::Color(255, 0xCE, 0xE7, 0xCA));

    for (size_t row = 0; row < cleanedGrid.size(); ++row) {
        for (size_t col = 0; col < cleanedGrid[row].size(); ++col) {
            if (!cleanedGrid[row][col]) continue;

            Gdiplus::REAL cx = static_cast<Gdiplus::REAL>(innerX + (col + 0.5) * cellSize * scaleX);
            Gdiplus::REAL cy = static_cast<Gdiplus::REAL>(innerY + (row + 0.5) * cellSize * scaleY);

            Gdiplus::REAL radiusX = static_cast<Gdiplus::REAL>(cellSize * scaleX * 1.1);
            Gdiplus::REAL radiusY = static_cast<Gdiplus::REAL>(cellSize * scaleY * 1.1);

            graphics.FillEllipse(&cellBrush,
                cx - radiusX,
                cy - radiusY,
                radiusX * 2.0f,
                radiusY * 2.0f);
        }
    }

    // PASO 4: OBSTÁCULOS (ENCIMA DE TODO)
    for (size_t oi = 0; oi < zone->getObstacles().size(); oi++) {
        auto& obs = zone->getObstacles()[oi];
        if (!obs) continue;

        int ox = innerX + static_cast<int>(obs->getX() * scaleX);
        int oy = innerY + static_cast<int>(obs->getY() * scaleY);
        int ow = std::max(10, static_cast<int>(obs->getWidth() * scaleX));
        int oh = std::max(10, static_cast<int>(obs->getHeight() * scaleY));

        Gdiplus::Image* obsImg = nullptr;
        int imgType = obs->getImageType();

        switch (imgType) {
        case 0: obsImg = nenufar1_; break;
        case 1: obsImg = nenufar2_; break;
        case 2: obsImg = tronco1_; break;
        case 3: obsImg = tronco2_; break;
        }

        if (obsImg) {
            graphics.DrawImage(obsImg, ox, oy, ow, oh);
        }
        else {
            HBRUSH obsBr = CreateSolidBrush(RGB(0x90, 0x70, 0x50));
            HPEN obsPen = CreatePen(PS_SOLID, 1, RGB(0x70, 0x50, 0x30));
            SelectObject(hdc, obsBr);
            SelectObject(hdc, obsPen);
            RoundRect(hdc, ox, oy, ox + ow, oy + oh, 6, 6);
            DeleteObject(obsBr);
            DeleteObject(obsPen);
        }
    }

    // PASO 5: BORDES Y DECORACIÓN
    HPEN borderPen = CreatePen(PS_SOLID, 3, COLOR_TEXT);
    SelectObject(hdc, borderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, x, y, x + w, y + h, 10, 10);
    DeleteObject(borderPen);

    drawText(hdc, zone->getName(), x + 12, y + 8, COLOR_TEXT, 13, true);

    wchar_t pct[20];
    swprintf_s(pct, L"%.1f%%", zone->getCleanedPercentage());
    SIZE pctSize;
    HFONT fontPct = CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, fontPct);
    GetTextExtentPoint32(hdc, pct, static_cast<int>(wcslen(pct)), &pctSize);
    SelectObject(hdc, oldFont);
    DeleteObject(fontPct);
    drawText(hdc, pct, x + w - pctSize.cx - 12, y + 8, COLOR_TEXT, 13, true);

    HPEN framePen = CreatePen(PS_SOLID, 1, COLOR_TEXT);
    SelectObject(hdc, framePen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, innerX, innerY, innerX + innerW, innerY + innerH);
    DeleteObject(framePen);

    wchar_t info[96];
    swprintf_s(info, L"Area: %.0f cm2", zone->getArea());
    drawText(hdc, info, x + 12, y + h - 22, COLOR_TEXT_DIM, 11, false);
}

void ScreenManager::drawRoombaOnZone(
    HDC hdc,
    std::shared_ptr<Roomba> roomba,
    std::shared_ptr<Zone> zone,
    int zoneX,
    int zoneY,
    int zoneW,
    int zoneH
) {
    if (!roomba || !zone) return;

    int innerX = zoneX + 10;
    int innerY = zoneY + 30;
    int innerW = zoneW - 20;
    int innerH = zoneH - 40;

    double scaleX = innerW / zone->getLength();
    double scaleY = innerH / zone->getWidth();

    int rx = innerX + static_cast<int>(roomba->getX() * scaleX);
    int ry = innerY + static_cast<int>(roomba->getY() * scaleY);

    Gdiplus::Graphics graphics(hdc);

    int imageSize = 30;

    double targetAngle = roomba->getAngle();
    double currentAngle = targetAngle;

    int roombaId = roomba->getId();
    if (lastRoombaAngles_.find(roombaId) != lastRoombaAngles_.end()) {
        double lastAngle = lastRoombaAngles_[roombaId];
        double angleDiff = targetAngle - lastAngle;

        while (angleDiff > M_PI) angleDiff -= 2 * M_PI;
        while (angleDiff < -M_PI) angleDiff += 2 * M_PI;

        currentAngle = lastAngle + angleDiff * 0.3;
    }
    lastRoombaAngles_[roombaId] = currentAngle;

    COLORREF bodyColor = roomba->getColor();
    if (roomba->getStuckCounter() > 10) {
        bodyColor = RGB(255, 140, 0);
    }

    bool hasImage = (roombaImageBasic_ && roombaImageAdvanced_ && roombaImagePremium_);

    if (hasImage) {
        drawRoombaImage(graphics, rx, ry, currentAngle, bodyColor, imageSize, roomba->getType());
    }
    else {
        HBRUSH shadow = CreateSolidBrush(RGB(15, 16, 20));
        SelectObject(hdc, shadow);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, rx - 12, ry - 12, rx + 16, ry + 16);
        DeleteObject(shadow);

        COLORREF auraColor = lightenColor(roomba->getColor(), 35);
        if (roomba->getStuckCounter() > 10) {
            auraColor = RGB(255, 190, 120);
        }

        HBRUSH aura = CreateSolidBrush(auraColor);
        SelectObject(hdc, aura);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, rx - 18, ry - 18, rx + 18, ry + 18);
        DeleteObject(aura);

        HBRUSH body = CreateSolidBrush(bodyColor);
        HPEN bodyPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        SelectObject(hdc, body);
        SelectObject(hdc, bodyPen);
        Ellipse(hdc, rx - 12, ry - 12, rx + 12, ry + 12);
        DeleteObject(body);
        DeleteObject(bodyPen);

        HPEN dirPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        SelectObject(hdc, dirPen);
        MoveToEx(hdc, rx, ry, nullptr);
        LineTo(hdc, rx + static_cast<int>(std::cos(currentAngle) * 11),
            ry + static_cast<int>(std::sin(currentAngle) * 11));
        DeleteObject(dirPen);
    }
}

void ScreenManager::drawRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 2, border);
    SelectObject(hdc, brush);
    SelectObject(hdc, pen);
    RoundRect(hdc, x, y, x + w, y + h, r, r);
    DeleteObject(brush);
    DeleteObject(pen);
}

void ScreenManager::drawText(HDC hdc, const wchar_t* text, int x, int y, COLORREF color, int size, bool bold) {
    if (!text) return;

    HFONT font = CreateFont(
        size,
        0,
        0,
        0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, x, y, text, static_cast<int>(wcslen(text)));
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void ScreenManager::drawButton(HDC hdc, const Button& btn) {
    RECT drawRect = btn.rect;

    if (btn.isHovered) {
        int expandX = (drawRect.right - drawRect.left) / 20;
        int expandY = (drawRect.bottom - drawRect.top) / 20;
        drawRect.left -= expandX;
        drawRect.right += expandX;
        drawRect.top -= expandY;
        drawRect.bottom += expandY;
    }

    int w = drawRect.right - drawRect.left;
    int h = drawRect.bottom - drawRect.top;

    drawRoundRect(hdc, drawRect.left, drawRect.top, w, h, 8, btn.color, btn.color);

    COLORREF textColor = COLOR_BG_DARK;
    int brightness = GetRValue(btn.color) + GetGValue(btn.color) + GetBValue(btn.color);
    if (brightness > 500) {
        textColor = COLOR_TEXT;
    }

    SIZE textSize;
    HFONT font = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    GetTextExtentPoint32(hdc, btn.text.c_str(), static_cast<int>(btn.text.length()), &textSize);
    SelectObject(hdc, oldFont);
    DeleteObject(font);

    int textX = drawRect.left + (w - textSize.cx) / 2;
    int textY = drawRect.top + (h - textSize.cy) / 2;
    drawText(hdc, btn.text.c_str(), textX, textY, textColor, 16, true);
}

void ScreenManager::drawProgressBar(HDC hdc, int x, int y, int w, int h, double progress, COLORREF color) {
    HBRUSH bgBrush = CreateSolidBrush(RGB(0xD0, 0xCA, 0xB0));
    RECT bgRect = { x, y, x + w, y + h };
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    if (progress > 0.0) {
        int fillW = static_cast<int>(w * progress / 100.0);
        if (fillW > w) fillW = w;
        if (fillW < 0) fillW = 0;

        if (fillW > 0) {
            HBRUSH fillBrush = CreateSolidBrush(color);
            RECT fillRect = { x, y, x + fillW, y + h };
            FillRect(hdc, &fillRect, fillBrush);
            DeleteObject(fillBrush);
        }
    }

    HPEN borderPen = CreatePen(PS_SOLID, 1, COLOR_TEXT);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    SelectObject(hdc, borderPen);
    Rectangle(hdc, x, y, x + w, y + h);
    DeleteObject(borderPen);
}

void ScreenManager::addButton(int id, int x, int y, int w, int h, const wchar_t* text, COLORREF color) {
    Button btn;
    btn.id = id;
    btn.rect.left = x;
    btn.rect.top = y;
    btn.rect.right = x + w;
    btn.rect.bottom = y + h;
    btn.originalRect = btn.rect;
    btn.text = text ? text : L"";
    btn.color = color;
    btn.isHovered = false;
    buttons_.push_back(btn);
}

void ScreenManager::clearButtons() {
    buttons_.clear();
}

int ScreenManager::hitTestButton(int x, int y) {
    for (size_t i = 0; i < buttons_.size(); i++) {
        if (x >= buttons_[i].originalRect.left && x <= buttons_[i].originalRect.right &&
            y >= buttons_[i].originalRect.top && y <= buttons_[i].originalRect.bottom) {
            return buttons_[i].id;
        }
    }
    return 0;
}

void ScreenManager::updateButtonHover(int x, int y) {
    bool changed = false;
    for (auto& btn : buttons_) {
        bool wasHovered = btn.isHovered;
        btn.isHovered = (x >= btn.originalRect.left && x <= btn.originalRect.right &&
            y >= btn.originalRect.top && y <= btn.originalRect.bottom);
        if (wasHovered != btn.isHovered) {
            changed = true;
        }
    }
    if (changed) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ScreenManager::handleButtonClick(int id) {
    switch (id) {
    case ID_BTN_START:
        initializeRoombas();
        addLog(L"Las Froombas saltan al agua...");
        cleaningService_->startCleaning();
        changeScreen(SCREEN_CLEANING);
        break;

    case ID_BTN_CONFIG:
        changeScreen(SCREEN_CONFIG);
        break;

    case ID_BTN_EXIT:
        cleaningService_->stopCleaning();
        DestroyWindow(hwnd_);
        break;

    case ID_BTN_MINUS:
        if (roombaCount_ > 1) {
            roombaCount_--;
        }
        break;

    case ID_BTN_PLUS:
        if (roombaCount_ < 4) {
            roombaCount_++;
        }
        break;

    case ID_BTN_BASIC:
        roombaType_ = Roomba::BASIC;
        break;

    case ID_BTN_ADVANCED:
        roombaType_ = Roomba::ADVANCED;
        break;

    case ID_BTN_PREMIUM:
        roombaType_ = Roomba::PREMIUM;
        break;

    case ID_BTN_BACK:
        changeScreen(SCREEN_START);
        break;

    case ID_BTN_BEGIN:
        initializeRoombas();
        addLog(L"¡Las Froombas comienzan a nadar!");
        cleaningService_->startCleaning();
        changeScreen(SCREEN_CLEANING);
        break;

    case ID_BTN_STOP:
        if (cleaningService_->isRunning()) {
            cleaningService_->stopCleaning();
            addLog(L"Froombas en descanso");
        }
        else {
            initializeRoombas();
            cleaningService_->startCleaning();
            addLog(L"Froombas de vuelta al trabajo");
        }
        break;

    case ID_BTN_MENU:
        cleaningService_->stopCleaning();
        resetZones();
        roombas_.clear();
        changeScreen(SCREEN_START);
        break;

    case ID_BTN_RESTART:
        cleaningService_->stopCleaning();
        resetZones();
        roombas_.clear();
        changeScreen(SCREEN_START);
        break;

    case ID_BTN_EXIT_FINISH:
        cleaningService_->stopCleaning();
        DestroyWindow(hwnd_);
        break;
    }
}

COLORREF ScreenManager::lightenColor(COLORREF color, int amount) {
    int r = std::min(255, static_cast<int>(GetRValue(color)) + amount);
    int g = std::min(255, static_cast<int>(GetGValue(color)) + amount);
    int b = std::min(255, static_cast<int>(GetBValue(color)) + amount);
    return RGB(r, g, b);
}


