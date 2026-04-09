#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ScreenManager.h"
#include <windowsx.h>
#include <commctrl.h>
#include <random>
#include <cmath>
#include <algorithm>
#include <cwchar>

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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ScreenManager::ScreenManager()
    : hwnd_(nullptr),
    hInstance_(nullptr),
    currentScreen_(SCREEN_START),
    roombaCount_(3),
    roombaType_(Roomba::ADVANCED) {
}

ScreenManager::~ScreenManager() {
    cleaningService_.stopCleaning();
    EventService::getInstance().clearSubscriptions();
}

bool ScreenManager::initialize() {
    hInstance_ = GetModuleHandle(nullptr);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"RoombaScreenClass";

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
        0, CLASS_NAME,
        L"Sistema Distribuido Roomba",
        WS_OVERLAPPEDWINDOW,
        (screenW - winW) / 2, (screenH - winH) / 2,
        winW, winH,
        nullptr, nullptr, hInstance_, this
    );

    if (!hwnd_) return false;

    Database::getInstance().initialize();

    EventService::getInstance().clearSubscriptions();
    EventService::getInstance().subscribe([this](const wchar_t* msg) {
        this->addLog(msg);
        });

    initializeZones();

    cleaningService_.setZones(&zones_);
    cleaningService_.setRoombas(&roombas_);

    SetTimer(hwnd_, ID_TIMER_PAINT, 33, nullptr);

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    addLog(L"Sistema iniciado");

    return true;
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

    zones_.push_back(std::make_shared<Zone>(1, L"Sala Principal", 500.0, 150.0));
    zones_.push_back(std::make_shared<Zone>(2, L"Pasillo", 480.0, 101.0));
    zones_.push_back(std::make_shared<Zone>(3, L"Comedor", 309.0, 480.0));
    zones_.push_back(std::make_shared<Zone>(4, L"Entrada", 90.0, 220.0));

    std::random_device rd;
    std::mt19937 gen(rd());

    for (size_t i = 0; i < zones_.size(); i++) {
        auto& zone = zones_[i];
        if (!zone) continue;

        double zoneLength = zone->getLength();
        double zoneWidth = zone->getWidth();

        int numObs = (zone->getArea() > 50000.0) ? 2 : 1;

        std::uniform_real_distribution<double> disW(22.0, 42.0);
        std::uniform_real_distribution<double> disH(22.0, 42.0);

        int created = 0;
        int attempts = 0;

        while (created < numObs && attempts < 100) {
            attempts++;

            double ow = disW(gen);
            double oh = disH(gen);

            if (zoneLength - ow - 20.0 <= 20.0 || zoneWidth - oh - 20.0 <= 20.0) {
                continue;
            }

            std::uniform_real_distribution<double> disX(20.0, zoneLength - ow - 20.0);
            std::uniform_real_distribution<double> disY(20.0, zoneWidth - oh - 20.0);

            double ox = disX(gen);
            double oy = disY(gen);

            bool overlaps = false;

            for (const auto& existing : zone->getObstacles()) {
                if (!existing) continue;

                double margin = 12.0;

                bool intersect =
                    ox < existing->getX() + existing->getWidth() + margin &&
                    ox + ow + margin > existing->getX() &&
                    oy < existing->getY() + existing->getHeight() + margin &&
                    oy + oh + margin > existing->getY();

                if (intersect) {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps) {
                zone->addObstacle(std::make_shared<Obstacle>(
                    ox, oy, ow, oh, Obstacle::FURNITURE
                ));
                created++;
            }
        }
    }
}

void ScreenManager::resetZones() {
    for (size_t i = 0; i < zones_.size(); i++) {
        if (zones_[i]) {
            zones_[i]->resetCleaning();
        }
    }
}

void ScreenManager::initializeRoombas() {
    cleaningService_.stopCleaning();
    roombas_.clear();
    resetZones();

    for (int i = 1; i <= roombaCount_; i++) {
        auto roomba = std::make_shared<Roomba>(i, roombaType_);
        roomba->setCurrentZone(nullptr);
        roomba->setState(Roomba::IDLE);
        roomba->setPosition(10.0, 10.0);
        roomba->setAngle(0.0);
        roombas_.push_back(roomba);
    }

    wchar_t msg[128];
    swprintf_s(msg, L"Configuradas %d Roombas de tipo %s", roombaCount_,
        roombas_.empty() ? L"N/A" : roombas_[0]->getTypeName());
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
        if (mgr) mgr->hwnd_ = hwnd;
    }
    else {
        mgr = reinterpret_cast<ScreenManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (mgr) return mgr->handleMessage(uMsg, wParam, lParam);
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
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int btnId = hitTestButton(x, y);
        if (btnId > 0) handleButtonClick(btnId);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd_, ID_TIMER_PAINT);
        cleaningService_.stopCleaning();
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
    }
}

void ScreenManager::paintStartScreen(HDC hdc, RECT& rect) {
    int centerX = rect.right / 2;
    int centerY = rect.bottom / 2;

    drawText(hdc, L"SISTEMA DISTRIBUIDO", centerX - 200, centerY - 220, COLOR_TEXT, 34, true);
    drawText(hdc, L"ROOMBA", centerX - 95, centerY - 155, COLOR_PRIMARY, 54, true);
    drawText(hdc, L"Programacion en Entornos Distribuidos", centerX - 190, centerY - 90, COLOR_TEXT_DIM, 16, false);

    drawRoundRect(hdc, centerX - 140, centerY - 20, 280, 95, 25, COLOR_PANEL, COLOR_PRIMARY);

    HBRUSH robotBrush = CreateSolidBrush(COLOR_PRIMARY);
    HPEN robotPen = CreatePen(PS_SOLID, 3, COLOR_SECONDARY);
    SelectObject(hdc, robotBrush);
    SelectObject(hdc, robotPen);
    Ellipse(hdc, centerX - 45, centerY - 5, centerX + 45, centerY + 85);
    DeleteObject(robotBrush);
    DeleteObject(robotPen);

    HBRUSH eyeBrush = CreateSolidBrush(COLOR_BG_DARK);
    SelectObject(hdc, eyeBrush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, centerX - 22, centerY + 26, centerX - 9, centerY + 39);
    Ellipse(hdc, centerX + 9, centerY + 26, centerX + 22, centerY + 39);
    DeleteObject(eyeBrush);

    int btnY = centerY + 130;
    addButton(ID_BTN_START, centerX - 150, btnY, 300, 58, L"INICIAR", COLOR_SUCCESS);
    addButton(ID_BTN_CONFIG, centerX - 150, btnY + 72, 300, 58, L"CONFIGURAR", COLOR_PRIMARY);
    addButton(ID_BTN_EXIT, centerX - 150, btnY + 144, 300, 58, L"SALIR", COLOR_DANGER);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }

    drawText(hdc, L"Visualizacion sincronizada con limpieza real", centerX - 170, rect.bottom - 55, COLOR_TEXT_DIM, 14, false);
    drawText(hdc, L"v2.1", rect.right - 60, rect.bottom - 30, COLOR_TEXT_DIM, 12, false);
}

void ScreenManager::paintConfigScreen(HDC hdc, RECT& rect) {
    int centerX = rect.right / 2;

    drawText(hdc, L"CONFIGURACION", centerX - 115, 30, COLOR_TEXT, 28, true);
    drawRoundRect(hdc, centerX - 340, 80, 680, 530, 16, COLOR_BG_LIGHT, COLOR_TEXT_DIM);

    drawText(hdc, L"Numero de Roombas:", centerX - 295, 120, COLOR_TEXT, 18, true);
    addButton(ID_BTN_MINUS, centerX - 90, 155, 52, 52, L"-", COLOR_DANGER);

    wchar_t countStr[8];
    swprintf_s(countStr, L"%d", roombaCount_);
    drawText(hdc, countStr, centerX - 8, 168, COLOR_PRIMARY, 28, true);

    addButton(ID_BTN_PLUS, centerX + 38, 155, 52, 52, L"+", COLOR_SUCCESS);

    drawText(hdc, L"Tipo de Roomba:", centerX - 295, 245, COLOR_TEXT, 18, true);

    COLORREF c1 = (roombaType_ == Roomba::BASIC) ? COLOR_PRIMARY : RGB(70, 70, 90);
    COLORREF c2 = (roombaType_ == Roomba::ADVANCED) ? COLOR_SUCCESS : RGB(70, 70, 90);
    COLORREF c3 = (roombaType_ == Roomba::PREMIUM) ? COLOR_DANGER : RGB(70, 70, 90);

    addButton(ID_BTN_BASIC, centerX - 295, 285, 190, 65, L"BASICA", c1);
    addButton(ID_BTN_ADVANCED, centerX - 95, 285, 190, 65, L"AVANZADA", c2);
    addButton(ID_BTN_PREMIUM, centerX + 105, 285, 190, 65, L"PREMIUM", c3);

    drawText(hdc, L"Zonas disponibles:", centerX - 295, 390, COLOR_TEXT, 16, true);

    COLORREF zoneColors[] = { COLOR_ZONE1, COLOR_ZONE2, COLOR_ZONE3, COLOR_ZONE4 };
    for (size_t i = 0; i < zones_.size(); i++) {
        auto& zone = zones_[i];
        if (!zone) continue;

        int zy = 425 + (int)i * 28;

        HBRUSH zb = CreateSolidBrush(zoneColors[i]);
        RECT zr = { centerX - 295, zy + 2, centerX - 278, zy + 19 };
        FillRect(hdc, &zr, zb);
        DeleteObject(zb);

        wchar_t info[160];
        swprintf_s(info, L"%s - %.0f cm x %.0f cm (%.0f cm2)",
            zone->getName(), zone->getLength(), zone->getWidth(), zone->getArea());
        drawText(hdc, info, centerX - 270, zy, COLOR_TEXT, 12, false);
    }

    addButton(ID_BTN_BACK, centerX - 295, 548, 190, 46, L"VOLVER", COLOR_TEXT_DIM);
    addButton(ID_BTN_BEGIN, centerX + 105, 548, 190, 46, L"COMENZAR", COLOR_SUCCESS);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }
}

void ScreenManager::paintCleaningScreen(HDC hdc, RECT& rect) {
    drawText(hdc, L"LIMPIEZA EN PROGRESO", 20, 12, COLOR_TEXT, 22, true);

    int zoneW = 400;
    int zoneH = 280;
    int gap = 15;
    int startX = 20;
    int startY = 50;

    COLORREF zoneColors[] = { COLOR_ZONE1, COLOR_ZONE2, COLOR_ZONE3, COLOR_ZONE4 };

    for (size_t i = 0; i < zones_.size(); i++) {
        auto& zone = zones_[i];
        if (!zone) continue;

        int col = (int)(i % 2);
        int row = (int)(i / 2);
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

    drawRoundRect(hdc, panelX, panelY, panelW, panelH, 12, COLOR_BG_LIGHT, COLOR_TEXT_DIM);

    bool isRunning = cleaningService_.isRunning();
    drawText(hdc, L"Estado:", panelX + 15, panelY + 15, COLOR_TEXT, 16, true);
    drawText(hdc,
        isRunning ? L"LIMPIANDO" : L"DETENIDO",
        panelX + 92, panelY + 15,
        isRunning ? COLOR_SUCCESS : COLOR_WARNING,
        16, true);

    double totalProg = 0.0;
    int validZones = 0;
    for (size_t i = 0; i < zones_.size(); i++) {
        if (zones_[i]) {
            totalProg += zones_[i]->getCleanedPercentage();
            validZones++;
        }
    }
    if (validZones > 0) totalProg /= validZones;

    drawText(hdc, L"Progreso Total:", panelX + 15, panelY + 50, COLOR_TEXT, 14, false);
    drawProgressBar(hdc, panelX + 15, panelY + 72, panelW - 30, 22, totalProg, COLOR_SUCCESS);

    wchar_t progStr[24];
    swprintf_s(progStr, L"%.1f%%", totalProg);
    drawText(hdc, progStr, panelX + panelW / 2 - 24, panelY + 74, COLOR_BG_DARK, 12, true);

    drawText(hdc, L"Por Zona:", panelX + 15, panelY + 112, COLOR_TEXT, 14, true);
    for (size_t i = 0; i < zones_.size(); i++) {
        auto& z = zones_[i];
        if (!z) continue;

        int zy = panelY + 138 + (int)i * 42;
        drawText(hdc, z->getName(), panelX + 15, zy, COLOR_TEXT, 12, false);
        drawProgressBar(hdc, panelX + 15, zy + 17, panelW - 30, 15, z->getCleanedPercentage(), zoneColors[i]);

        wchar_t pctStr[16];
        swprintf_s(pctStr, L"%.0f%%", z->getCleanedPercentage());
        drawText(hdc, pctStr, panelX + panelW - 58, zy - 1, zoneColors[i], 12, true);
    }

    drawText(hdc, L"Roombas:", panelX + 15, panelY + 320, COLOR_TEXT, 14, true);
    for (size_t i = 0; i < roombas_.size(); i++) {
        auto& r = roombas_[i];
        if (!r) continue;

        int ry = panelY + 346 + (int)i * 28;

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
        if (zone) {
            swprintf_s(info, L"#%d - %s - %s", r->getId(), r->getStateName(), zone->getName());
        }
        else {
            swprintf_s(info, L"#%d - %s", r->getId(), r->getStateName());
        }
        drawText(hdc, info, panelX + 38, ry, COLOR_TEXT, 11, false);
    }

    drawText(hdc, L"Log:", panelX + 15, panelY + 470, COLOR_TEXT, 14, true);
    {
        std::lock_guard<std::mutex> lock(logMutex_);
        int logY = panelY + 498;
        int cnt = 0;
        for (int idx = (int)logs_.size() - 1; idx >= 0 && cnt < 7; idx--, cnt++) {
            drawText(hdc, logs_[idx].c_str(), panelX + 15, logY + cnt * 16, COLOR_TEXT_DIM, 10, false);
        }
    }

    addButton(ID_BTN_STOP, panelX + 15, panelY + 580, 160, 40,
        isRunning ? L"DETENER" : L"REINICIAR",
        isRunning ? COLOR_DANGER : COLOR_SUCCESS);
    addButton(ID_BTN_MENU, panelX + panelW - 175, panelY + 580, 160, 40, L"MENU", COLOR_TEXT_DIM);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }
}

void ScreenManager::drawZoneMiniMap(HDC hdc, std::shared_ptr<Zone> zone, int x, int y, int w, int h, COLORREF accentColor, size_t zoneIndex) {
    if (!zone) return;

    HBRUSH zoneBg = CreateSolidBrush(RGB(35, 37, 50));
    RECT zoneRect = { x, y, x + w, y + h };
    FillRect(hdc, &zoneRect, zoneBg);
    DeleteObject(zoneBg);

    HPEN borderPen = CreatePen(PS_SOLID, 2, accentColor);
    SelectObject(hdc, borderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, x, y, x + w, y + h, 10, 10);
    DeleteObject(borderPen);

    drawText(hdc, zone->getName(), x + 12, y + 8, accentColor, 13, true);

    wchar_t pct[20];
    swprintf_s(pct, L"%.0f%%", zone->getCleanedPercentage());
    drawText(hdc, pct, x + w - 58, y + 8, accentColor, 13, true);

    int innerX = x + 10;
    int innerY = y + 30;
    int innerW = w - 20;
    int innerH = h - 40;

    HBRUSH floorBrush = CreateSolidBrush(RGB(44, 46, 65));
    RECT floorRect = { innerX, innerY, innerX + innerW, innerY + innerH };
    FillRect(hdc, &floorRect, floorBrush);
    DeleteObject(floorBrush);

    double scaleX = innerW / zone->getLength();
    double scaleY = innerH / zone->getWidth();

    auto cleanedGrid = zone->getGrid();
    double cellSize = zone->getCellSize();
    COLORREF cleanColor = lightenColor(accentColor, 30);

    for (size_t row = 0; row < cleanedGrid.size(); ++row) {
        for (size_t col = 0; col < cleanedGrid[row].size(); ++col) {
            if (!cleanedGrid[row][col]) continue;

            int cx = innerX + (int)(col * cellSize * scaleX);
            int cy = innerY + (int)(row * cellSize * scaleY);
            int cw = std::max(2, (int)std::ceil(cellSize * scaleX));
            int ch = std::max(2, (int)std::ceil(cellSize * scaleY));

            HBRUSH cb = CreateSolidBrush(cleanColor);
            RECT cellRect = { cx, cy, cx + cw, cy + ch };
            FillRect(hdc, &cellRect, cb);
            DeleteObject(cb);
        }
    }

    HBRUSH gridBrush = CreateSolidBrush(RGB(58, 60, 82));
    for (int gx = innerX; gx < innerX + innerW; gx += 28) {
        RECT lineRect = { gx, innerY, gx + 1, innerY + innerH };
        FillRect(hdc, &lineRect, gridBrush);
    }
    for (int gy = innerY; gy < innerY + innerH; gy += 28) {
        RECT lineRect = { innerX, gy, innerX + innerW, gy + 1 };
        FillRect(hdc, &lineRect, gridBrush);
    }
    DeleteObject(gridBrush);

    for (size_t oi = 0; oi < zone->getObstacles().size(); oi++) {
        auto& obs = zone->getObstacles()[oi];
        if (!obs) continue;

        int ox = innerX + (int)(obs->getX() * scaleX);
        int oy = innerY + (int)(obs->getY() * scaleY);
        int ow = std::max(6, (int)(obs->getWidth() * scaleX));
        int oh = std::max(6, (int)(obs->getHeight() * scaleY));

        HBRUSH obsBr = CreateSolidBrush(RGB(92, 95, 120));
        HPEN obsPen = CreatePen(PS_SOLID, 1, RGB(135, 140, 170));
        SelectObject(hdc, obsBr);
        SelectObject(hdc, obsPen);
        RoundRect(hdc, ox, oy, ox + ow, oy + oh, 6, 6);
        DeleteObject(obsBr);
        DeleteObject(obsPen);
    }

    auto trail = zone->getTrail();
    size_t start = (trail.size() > 700) ? trail.size() - 700 : 0;

    for (size_t j = start; j < trail.size(); j++) {
        auto& pt = trail[j];

        int px = innerX + (int)(pt.x * scaleX);
        int py = innerY + (int)(pt.y * scaleY);

        COLORREF trailColor = lightenColor(accentColor, 8);
        HBRUSH tb = CreateSolidBrush(trailColor);
        SelectObject(hdc, tb);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, px - 4, py - 4, px + 4, py + 4);
        DeleteObject(tb);
    }

    HPEN framePen = CreatePen(PS_SOLID, 1, RGB(90, 94, 120));
    SelectObject(hdc, framePen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, innerX, innerY, innerX + innerW, innerY + innerH);
    DeleteObject(framePen);

    wchar_t info[96];
    swprintf_s(info, L"Area: %.0f cm2", zone->getArea());
    drawText(hdc, info, x + 12, y + h - 22, COLOR_TEXT_DIM, 11, false);
}

void ScreenManager::drawRoombaOnZone(HDC hdc, std::shared_ptr<Roomba> roomba, std::shared_ptr<Zone> zone,
    int zoneX, int zoneY, int zoneW, int zoneH) {
    if (!roomba || !zone) return;

    int innerX = zoneX + 10;
    int innerY = zoneY + 30;
    int innerW = zoneW - 20;
    int innerH = zoneH - 40;

    double scaleX = innerW / zone->getLength();
    double scaleY = innerH / zone->getWidth();

    int rx = innerX + (int)(roomba->getX() * scaleX);
    int ry = innerY + (int)(roomba->getY() * scaleY);

    HBRUSH shadow = CreateSolidBrush(RGB(15, 16, 20));
    SelectObject(hdc, shadow);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, rx - 10, ry - 10, rx + 14, ry + 14);
    DeleteObject(shadow);

    COLORREF auraColor = lightenColor(roomba->getColor(), 35);
    if (roomba->getStuckCounter() > 10) {
        auraColor = RGB(255, 190, 120);
    }

    HBRUSH aura = CreateSolidBrush(auraColor);
    SelectObject(hdc, aura);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, rx - 15, ry - 15, rx + 15, ry + 15);
    DeleteObject(aura);

    COLORREF bodyColor = roomba->getColor();
    if (roomba->getStuckCounter() > 10) {
        bodyColor = RGB(255, 140, 0);
    }

    HBRUSH body = CreateSolidBrush(bodyColor);
    HPEN bodyPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    SelectObject(hdc, body);
    SelectObject(hdc, bodyPen);
    Ellipse(hdc, rx - 12, ry - 12, rx + 12, ry + 12);
    DeleteObject(body);
    DeleteObject(bodyPen);

    double ang = roomba->getAngle();
    HPEN dirPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    SelectObject(hdc, dirPen);
    MoveToEx(hdc, rx, ry, nullptr);
    LineTo(hdc, rx + (int)(std::cos(ang) * 11), ry + (int)(std::sin(ang) * 11));
    DeleteObject(dirPen);

    wchar_t idStr[8];
    swprintf_s(idStr, L"%d", roomba->getId());
    drawText(hdc, idStr, rx - 4, ry - 7, RGB(255, 255, 255), 10, true);
}

void ScreenManager::drawRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    SelectObject(hdc, brush);
    SelectObject(hdc, pen);
    RoundRect(hdc, x, y, x + w, y + h, r, r);
    DeleteObject(brush);
    DeleteObject(pen);
}

void ScreenManager::drawText(HDC hdc, const wchar_t* text, int x, int y, COLORREF color, int size, bool bold) {
    if (!text) return;

    HFONT font = CreateFont(
        size, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
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
    TextOut(hdc, x, y, text, (int)wcslen(text));
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void ScreenManager::drawButton(HDC hdc, const Button& btn) {
    int w = btn.rect.right - btn.rect.left;
    int h = btn.rect.bottom - btn.rect.top;

    drawRoundRect(hdc, btn.rect.left, btn.rect.top, w, h, 8, btn.color, btn.color);

    COLORREF textColor = COLOR_BG_DARK;
    int brightness = GetRValue(btn.color) + GetGValue(btn.color) + GetBValue(btn.color);
    if (brightness < 400) textColor = COLOR_TEXT;

    int textLen = (int)btn.text.length();
    int textX = btn.rect.left + w / 2 - textLen * 4;
    int textY = btn.rect.top + h / 2 - 8;
    drawText(hdc, btn.text.c_str(), textX, textY, textColor, 16, true);
}

void ScreenManager::drawProgressBar(HDC hdc, int x, int y, int w, int h, double progress, COLORREF color) {
    HBRUSH bgBrush = CreateSolidBrush(RGB(50, 52, 70));
    RECT bgRect = { x, y, x + w, y + h };
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    if (progress > 0.0) {
        int fillW = (int)(w * progress / 100.0);
        if (fillW > w) fillW = w;
        if (fillW < 0) fillW = 0;

        if (fillW > 0) {
            HBRUSH fillBrush = CreateSolidBrush(color);
            RECT fillRect = { x, y, x + fillW, y + h };
            FillRect(hdc, &fillRect, fillBrush);
            DeleteObject(fillBrush);
        }
    }

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(85, 88, 110));
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
    btn.text = text ? text : L"";
    btn.color = color;
    buttons_.push_back(btn);
}

void ScreenManager::clearButtons() {
    buttons_.clear();
}

int ScreenManager::hitTestButton(int x, int y) {
    for (size_t i = 0; i < buttons_.size(); i++) {
        if (x >= buttons_[i].rect.left && x <= buttons_[i].rect.right &&
            y >= buttons_[i].rect.top && y <= buttons_[i].rect.bottom) {
            return buttons_[i].id;
        }
    }
    return 0;
}

void ScreenManager::handleButtonClick(int id) {
    switch (id) {
    case ID_BTN_START:
        initializeRoombas();
        addLog(L"Iniciando limpieza...");
        cleaningService_.startCleaning();
        changeScreen(SCREEN_CLEANING);
        break;

    case ID_BTN_CONFIG:
        changeScreen(SCREEN_CONFIG);
        break;

    case ID_BTN_EXIT:
        cleaningService_.stopCleaning();
        DestroyWindow(hwnd_);
        break;

    case ID_BTN_MINUS:
        if (roombaCount_ > 1) roombaCount_--;
        break;

    case ID_BTN_PLUS:
        if (roombaCount_ < 10) roombaCount_++;
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
        addLog(L"Iniciando limpieza...");
        cleaningService_.startCleaning();
        changeScreen(SCREEN_CLEANING);
        break;

    case ID_BTN_STOP:
        if (cleaningService_.isRunning()) {
            cleaningService_.stopCleaning();
            addLog(L"Limpieza detenida");
        }
        else {
            initializeRoombas();
            cleaningService_.startCleaning();
            addLog(L"Limpieza reiniciada");
        }
        break;

    case ID_BTN_MENU:
        cleaningService_.stopCleaning();
        resetZones();
        roombas_.clear();
        changeScreen(SCREEN_START);
        break;
    }
}

COLORREF ScreenManager::lightenColor(COLORREF color, int amount) {
    int r = std::min(255, (int)GetRValue(color) + amount);
    int g = std::min(255, (int)GetGValue(color) + amount);
    int b = std::min(255, (int)GetBValue(color) + amount);
    return RGB(r, g, b);
}