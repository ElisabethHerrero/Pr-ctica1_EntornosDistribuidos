#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ScreenManager.h"
#include <windowsx.h>
#include <commctrl.h>
#include <random>
#include <cmath>

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
    : hwnd_(nullptr), hInstance_(nullptr), currentScreen_(SCREEN_START),
    roombaCount_(3), roombaType_(Roomba::ADVANCED) {
}

ScreenManager::~ScreenManager() {
    cleaningService_.stopCleaning();
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

        double maxX = zone->getLength() * 0.5;
        double maxY = zone->getWidth() * 0.5;

        if (maxX > 40 && maxY > 40) {
            std::uniform_real_distribution<double> disX(30.0, maxX);
            std::uniform_real_distribution<double> disY(30.0, maxY);
            std::uniform_real_distribution<double> disSize(25.0, 45.0);

            int numObs = (zone->getArea() > 50000) ? 2 : 1;

            for (int j = 0; j < numObs; j++) {
                auto obs = std::make_shared<Obstacle>(
                    disX(gen), disY(gen), disSize(gen), disSize(gen), Obstacle::FURNITURE
                );
                zone->addObstacle(obs);
            }
        }
    }
}

void ScreenManager::initializeRoombas() {
    cleaningService_.stopCleaning();
    roombas_.clear();

    for (size_t i = 0; i < zones_.size(); i++) {
        if (zones_[i]) {
            zones_[i]->setCleanedPercentage(0.0);
            zones_[i]->clearTrail();
        }
    }

    for (int i = 1; i <= roombaCount_; i++) {
        roombas_.push_back(std::make_shared<Roomba>(i, roombaType_));
    }

    wchar_t msg[128];
    wsprintf(msg, L"Configuradas %d Roombas", roombaCount_);
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
    if (logs_.size() > 30) {
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
    case SCREEN_START: paintStartScreen(hdc, rect); break;
    case SCREEN_CONFIG: paintConfigScreen(hdc, rect); break;
    case SCREEN_CLEANING: paintCleaningScreen(hdc, rect); break;
    }
}

void ScreenManager::paintStartScreen(HDC hdc, RECT& rect) {
    int centerX = rect.right / 2;
    int centerY = rect.bottom / 2;

    drawText(hdc, L"SISTEMA DISTRIBUIDO", centerX - 180, centerY - 200, COLOR_TEXT, 32, true);
    drawText(hdc, L"ROOMBA", centerX - 80, centerY - 140, COLOR_PRIMARY, 48, true);
    drawText(hdc, L"Programacion en Entornos Distribuidos", centerX - 180, centerY - 80, COLOR_TEXT_DIM, 16, false);

    HBRUSH roomBrush = CreateSolidBrush(COLOR_PRIMARY);
    HPEN roomPen = CreatePen(PS_SOLID, 3, COLOR_SECONDARY);
    SelectObject(hdc, roomBrush);
    SelectObject(hdc, roomPen);
    Ellipse(hdc, centerX - 50, centerY - 30, centerX + 50, centerY + 70);
    DeleteObject(roomBrush);
    DeleteObject(roomPen);

    HBRUSH eyeBrush = CreateSolidBrush(COLOR_BG_DARK);
    SelectObject(hdc, eyeBrush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, centerX - 25, centerY + 5, centerX - 10, centerY + 20);
    Ellipse(hdc, centerX + 10, centerY + 5, centerX + 25, centerY + 20);
    DeleteObject(eyeBrush);

    int btnY = centerY + 120;
    addButton(ID_BTN_START, centerX - 140, btnY, 280, 55, L"INICIAR", COLOR_SUCCESS);
    addButton(ID_BTN_CONFIG, centerX - 140, btnY + 70, 280, 55, L"CONFIGURAR", COLOR_PRIMARY);
    addButton(ID_BTN_EXIT, centerX - 140, btnY + 140, 280, 55, L"SALIR", COLOR_DANGER);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }

    drawText(hdc, L"v2.0", rect.right - 60, rect.bottom - 30, COLOR_TEXT_DIM, 12, false);
}

void ScreenManager::paintConfigScreen(HDC hdc, RECT& rect) {
    int centerX = rect.right / 2;

    drawText(hdc, L"CONFIGURACION", centerX - 100, 30, COLOR_TEXT, 28, true);
    drawRoundRect(hdc, centerX - 320, 80, 640, 500, 15, COLOR_BG_LIGHT, COLOR_TEXT_DIM);

    drawText(hdc, L"Numero de Roombas:", centerX - 280, 120, COLOR_TEXT, 18, true);
    addButton(ID_BTN_MINUS, centerX - 80, 155, 50, 50, L"-", COLOR_DANGER);

    wchar_t countStr[8];
    wsprintf(countStr, L"%d", roombaCount_);
    drawText(hdc, countStr, centerX - 8, 167, COLOR_PRIMARY, 28, true);

    addButton(ID_BTN_PLUS, centerX + 30, 155, 50, 50, L"+", COLOR_SUCCESS);

    drawText(hdc, L"Tipo de Roomba:", centerX - 280, 240, COLOR_TEXT, 18, true);

    COLORREF c1 = (roombaType_ == Roomba::BASIC) ? COLOR_PRIMARY : RGB(70, 70, 90);
    COLORREF c2 = (roombaType_ == Roomba::ADVANCED) ? COLOR_SUCCESS : RGB(70, 70, 90);
    COLORREF c3 = (roombaType_ == Roomba::PREMIUM) ? COLOR_DANGER : RGB(70, 70, 90);

    addButton(ID_BTN_BASIC, centerX - 280, 280, 180, 65, L"BASICA", c1);
    addButton(ID_BTN_ADVANCED, centerX - 90, 280, 180, 65, L"AVANZADA", c2);
    addButton(ID_BTN_PREMIUM, centerX + 100, 280, 180, 65, L"PREMIUM", c3);

    drawText(hdc, L"Zonas:", centerX - 280, 380, COLOR_TEXT, 16, true);

    COLORREF zoneColors[] = { COLOR_ZONE1, COLOR_ZONE2, COLOR_ZONE3, COLOR_ZONE4 };
    for (size_t i = 0; i < zones_.size(); i++) {
        auto& zone = zones_[i];
        if (!zone) continue;

        int zy = 410 + (int)i * 22;
        HBRUSH zb = CreateSolidBrush(zoneColors[i]);
        RECT zr = { centerX - 280, zy, centerX - 265, zy + 15 };
        FillRect(hdc, &zr, zb);
        DeleteObject(zb);

        wchar_t info[128];
        wsprintf(info, L"%s - %d cm2", zone->getName(), (int)zone->getArea());
        drawText(hdc, info, centerX - 255, zy, COLOR_TEXT, 12, false);
    }

    addButton(ID_BTN_BACK, centerX - 280, 530, 180, 45, L"VOLVER", COLOR_TEXT_DIM);
    addButton(ID_BTN_BEGIN, centerX + 100, 530, 180, 45, L"COMENZAR", COLOR_SUCCESS);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }
}

void ScreenManager::paintCleaningScreen(HDC hdc, RECT& rect) {
    drawText(hdc, L"LIMPIEZA EN PROGRESO", 20, 12, COLOR_TEXT, 22, true);

    int zoneW = 400, zoneH = 280, gap = 15;
    int startX = 20, startY = 50;

    COLORREF zoneColors[] = { COLOR_ZONE1, COLOR_ZONE2, COLOR_ZONE3, COLOR_ZONE4 };
    COLORREF trailColors[] = { RGB(100, 150, 220), RGB(100, 200, 130), RGB(220, 200, 100), RGB(220, 120, 130) };

    for (size_t i = 0; i < zones_.size(); i++) {
        auto& zone = zones_[i];
        if (!zone) continue;

        int col = i % 2;
        int row = i / 2;
        int x = startX + col * (zoneW + gap);
        int y = startY + row * (zoneH + gap);

        HBRUSH zoneBg = CreateSolidBrush(RGB(35, 37, 50));
        RECT zoneRect = { x, y, x + zoneW, y + zoneH };
        FillRect(hdc, &zoneRect, zoneBg);
        DeleteObject(zoneBg);

        HPEN borderPen = CreatePen(PS_SOLID, 2, zoneColors[i]);
        SelectObject(hdc, borderPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, x, y, x + zoneW, y + zoneH, 8, 8);
        DeleteObject(borderPen);

        drawText(hdc, zone->getName(), x + 10, y + 5, zoneColors[i], 13, true);

        wchar_t pct[16];
        wsprintf(pct, L"%.0f%%", zone->getCleanedPercentage());
        drawText(hdc, pct, x + zoneW - 50, y + 5, zoneColors[i], 13, true);

        double scaleX = (zoneW - 20.0) / zone->getLength();
        double scaleY = (zoneH - 35.0) / zone->getWidth();
        int contentY = y + 25;

        auto trail = zone->getTrailPointsCopy();
        size_t start = (trail.size() > 500) ? trail.size() - 500 : 0;
        for (size_t j = start; j < trail.size(); j++) {
            auto& pt = trail[j];
            int px = x + 10 + (int)(pt.x * scaleX);
            int py = contentY + (int)(pt.y * scaleY);

            int colorIdx = (pt.roombaId - 1) % 4;
            if (colorIdx < 0) colorIdx = 0;

            HBRUSH tb = CreateSolidBrush(trailColors[colorIdx]);
            SelectObject(hdc, tb);
            SelectObject(hdc, GetStockObject(NULL_PEN));
            Ellipse(hdc, px - 2, py - 2, px + 2, py + 2);
            DeleteObject(tb);
        }

        for (size_t oi = 0; oi < zone->getObstacles().size(); oi++) {
            auto& obs = zone->getObstacles()[oi];
            if (!obs) continue;
            int ox = x + 10 + (int)(obs->getX() * scaleX);
            int oy = contentY + (int)(obs->getY() * scaleY);
            int ow = (int)(obs->getWidth() * scaleX);
            int oh = (int)(obs->getHeight() * scaleY);

            HBRUSH obsBr = CreateSolidBrush(RGB(70, 70, 90));
            RECT obsRect = { ox, oy, ox + ow, oy + oh };
            FillRect(hdc, &obsRect, obsBr);
            DeleteObject(obsBr);
        }

        for (size_t ri = 0; ri < roombas_.size(); ri++) {
            auto& roomba = roombas_[ri];
            if (!roomba) continue;
            if (roomba->getCurrentZone() != zone) continue;
            if (roomba->getState() != Roomba::CLEANING) continue;

            int rx = x + 10 + (int)(roomba->getX() * scaleX);
            int ry = contentY + (int)(roomba->getY() * scaleY);

            HBRUSH shadow = CreateSolidBrush(RGB(0, 0, 0));
            SelectObject(hdc, shadow);
            Ellipse(hdc, rx - 8, ry - 8, rx + 12, ry + 12);
            DeleteObject(shadow);

            HBRUSH body = CreateSolidBrush(roomba->getColor());
            HPEN bodyPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(hdc, body);
            SelectObject(hdc, bodyPen);
            Ellipse(hdc, rx - 10, ry - 10, rx + 10, ry + 10);
            DeleteObject(body);
            DeleteObject(bodyPen);

            double ang = roomba->getAngle();
            HPEN dirPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(hdc, dirPen);
            MoveToEx(hdc, rx, ry, nullptr);
            LineTo(hdc, rx + (int)(cos(ang) * 10), ry + (int)(sin(ang) * 10));
            DeleteObject(dirPen);

            wchar_t idStr[8];
            wsprintf(idStr, L"%d", roomba->getId());
            drawText(hdc, idStr, rx - 4, ry - 6, RGB(255, 255, 255), 10, true);
        }
    }

    int panelX = 855, panelY = 50, panelW = 520;
    drawRoundRect(hdc, panelX, panelY, panelW, 620, 12, COLOR_BG_LIGHT, COLOR_TEXT_DIM);

    bool isRunning = cleaningService_.isRunning();
    drawText(hdc, L"Estado:", panelX + 15, panelY + 15, COLOR_TEXT, 16, true);
    drawText(hdc, isRunning ? L"LIMPIANDO" : L"DETENIDO", panelX + 90, panelY + 15,
        isRunning ? COLOR_SUCCESS : COLOR_WARNING, 16, true);

    double totalProg = 0;
    for (size_t i = 0; i < zones_.size(); i++) {
        if (zones_[i]) totalProg += zones_[i]->getCleanedPercentage();
    }
    if (!zones_.empty()) totalProg /= zones_.size();

    drawText(hdc, L"Progreso Total:", panelX + 15, panelY + 50, COLOR_TEXT, 14, false);
    drawProgressBar(hdc, panelX + 15, panelY + 72, panelW - 30, 22, totalProg, COLOR_SUCCESS);

    wchar_t progStr[16];
    wsprintf(progStr, L"%.1f%%", totalProg);
    drawText(hdc, progStr, panelX + panelW / 2 - 25, panelY + 74, COLOR_BG_DARK, 12, true);

    drawText(hdc, L"Por Zona:", panelX + 15, panelY + 110, COLOR_TEXT, 14, true);
    for (size_t i = 0; i < zones_.size(); i++) {
        auto& z = zones_[i];
        if (!z) continue;
        int zy = panelY + 135 + (int)i * 38;
        drawText(hdc, z->getName(), panelX + 15, zy, COLOR_TEXT, 12, false);
        drawProgressBar(hdc, panelX + 15, zy + 16, panelW - 30, 14, z->getCleanedPercentage(), zoneColors[i]);
    }

    drawText(hdc, L"Roombas:", panelX + 15, panelY + 300, COLOR_TEXT, 14, true);
    for (size_t i = 0; i < roombas_.size(); i++) {
        auto& r = roombas_[i];
        if (!r) continue;
        int ry = panelY + 325 + (int)i * 24;

        HBRUSH rb = CreateSolidBrush(r->getColor());
        SelectObject(hdc, rb);
        Ellipse(hdc, panelX + 15, ry, panelX + 28, ry + 13);
        DeleteObject(rb);

        wchar_t info[64];
        wsprintf(info, L"#%d - %s", r->getId(), r->getStateName());
        drawText(hdc, info, panelX + 35, ry, COLOR_TEXT, 11, false);
    }

    drawText(hdc, L"Log:", panelX + 15, panelY + 480, COLOR_TEXT, 14, true);
    {
        std::lock_guard<std::mutex> lock(logMutex_);
        int logY = panelY + 505;
        int cnt = 0;
        for (int idx = (int)logs_.size() - 1; idx >= 0 && cnt < 5; idx--, cnt++) {
            drawText(hdc, logs_[idx].c_str(), panelX + 15, logY + cnt * 15, COLOR_TEXT_DIM, 10, false);
        }
    }

    addButton(ID_BTN_STOP, panelX + 15, panelY + 590, 150, 40, isRunning ? L"DETENER" : L"REINICIAR",
        isRunning ? COLOR_DANGER : COLOR_SUCCESS);
    addButton(ID_BTN_MENU, panelX + panelW - 165, panelY + 590, 150, 40, L"MENU", COLOR_TEXT_DIM);

    for (size_t i = 0; i < buttons_.size(); i++) {
        drawButton(hdc, buttons_[i]);
    }
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
    HFONT font = CreateFont(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
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

    if (progress > 0) {
        int fillW = (int)(w * progress / 100.0);
        if (fillW > w) fillW = w;
        if (fillW > 0) {
            HBRUSH fillBrush = CreateSolidBrush(color);
            RECT fillRect = { x, y, x + fillW, y + h };
            FillRect(hdc, &fillRect, fillBrush);
            DeleteObject(fillBrush);
        }
    }
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
            for (size_t i = 0; i < zones_.size(); i++) {
                if (zones_[i]) {
                    zones_[i]->setCleanedPercentage(0.0);
                    zones_[i]->clearTrail();
                }
            }
            initializeRoombas();
            cleaningService_.startCleaning();
            addLog(L"Limpieza reiniciada");
        }
        break;

    case ID_BTN_MENU:
        cleaningService_.stopCleaning();
        for (size_t i = 0; i < zones_.size(); i++) {
            if (zones_[i]) {
                zones_[i]->setCleanedPercentage(0.0);
                zones_[i]->clearTrail();
            }
        }
        roombas_.clear();
        changeScreen(SCREEN_START);
        break;
    }
}