#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "CleaningService.h"
#include "Zone.h"
#include "Roomba.h"

#define COLOR_BG_DARK RGB(20, 22, 35)
#define COLOR_BG_LIGHT RGB(30, 32, 48)
#define COLOR_PANEL RGB(40, 42, 60)
#define COLOR_PRIMARY RGB(52, 152, 219)
#define COLOR_SECONDARY RGB(41, 128, 185)
#define COLOR_SUCCESS RGB(46, 204, 113)
#define COLOR_DANGER RGB(231, 76, 60)
#define COLOR_WARNING RGB(241, 196, 15)
#define COLOR_TEXT RGB(236, 240, 241)
#define COLOR_TEXT_DIM RGB(149, 165, 166)
#define COLOR_ZONE1 RGB(52, 152, 219)
#define COLOR_ZONE2 RGB(46, 204, 113)
#define COLOR_ZONE3 RGB(155, 89, 182)
#define COLOR_ZONE4 RGB(230, 126, 34)

class ScreenManager {
public:
    ScreenManager();
    ~ScreenManager();

    bool initialize();
    void run();

    static const int SCREEN_START = 0;
    static const int SCREEN_CONFIG = 1;
    static const int SCREEN_CLEANING = 2;
    static const int SCREEN_FINISHED = 3;

private:
    HWND hwnd_;
    HINSTANCE hInstance_;
    int currentScreen_;

    std::unique_ptr<CleaningService> cleaningService_;
    std::vector<std::shared_ptr<Zone>> zones_;
    std::vector<std::shared_ptr<Roomba>> roombas_;

    int roombaCount_;
    int roombaType_;

    std::vector<std::wstring> logs_;
    mutable std::mutex logMutex_;

    struct Button {
        int id;
        RECT rect;
        std::wstring text;
        COLORREF color;
    };
    std::vector<Button> buttons_;

    void initializeZones();
    void resetZones();
    void initializeRoombas();
    void changeScreen(int screenId);
    void addLog(const wchar_t* msg);

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void paintScreen(HDC hdc, RECT& rect);
    void paintStartScreen(HDC hdc, RECT& rect);
    void paintConfigScreen(HDC hdc, RECT& rect);
    void paintCleaningScreen(HDC hdc, RECT& rect);
    void paintFinishedScreen(HDC hdc, RECT& rect);

    void drawZoneMiniMap(HDC hdc, std::shared_ptr<Zone> zone, int x, int y, int w, int h, COLORREF accentColor, size_t zoneIndex);
    void drawRoombaOnZone(HDC hdc, std::shared_ptr<Roomba> roomba, std::shared_ptr<Zone> zone, int zoneX, int zoneY, int zoneW, int zoneH);

    void drawRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border);
    void drawText(HDC hdc, const wchar_t* text, int x, int y, COLORREF color, int size, bool bold);
    void drawButton(HDC hdc, const Button& btn);
    void drawProgressBar(HDC hdc, int x, int y, int w, int h, double progress, COLORREF color);

    void addButton(int id, int x, int y, int w, int h, const wchar_t* text, COLORREF color);
    void clearButtons();
    int hitTestButton(int x, int y);
    void handleButtonClick(int id);

    COLORREF lightenColor(COLORREF color, int amount);
};