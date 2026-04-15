#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <gdiplus.h>
#include "CleaningService.h"
#include "Zone.h"
#include "Roomba.h"

#pragma comment(lib, "gdiplus.lib")

#define COLOR_BG_DARK RGB(0xFC, 0xF9, 0xE6)
#define COLOR_BG_LIGHT RGB(0xF5, 0xF0, 0xD8)
#define COLOR_PANEL RGB(0xE8, 0xE3, 0xCC)
#define COLOR_PRIMARY RGB(0xB5, 0xB5, 0x6D)
#define COLOR_SECONDARY RGB(0x9A, 0x9A, 0x5C)
#define COLOR_SUCCESS RGB(0x8D, 0xB2, 0xA1)
#define COLOR_DANGER RGB(0xE4, 0x71, 0x4C)
#define COLOR_WARNING RGB(0xE8, 0xA8, 0x5C)
#define COLOR_TEXT RGB(0x81, 0x53, 0x2B)
#define COLOR_TEXT_DIM RGB(0xA0, 0x7B, 0x5B)
#define COLOR_ZONE1 RGB(0xB5, 0xB5, 0x6D)
#define COLOR_ZONE2 RGB(0x8D, 0xB2, 0xA1)
#define COLOR_ZONE3 RGB(0xB5, 0x9A, 0x7C)
#define COLOR_ZONE4 RGB(0xA8, 0xC5, 0xB8)
#define COLOR_TRAIL RGB(0xCE, 0xE7, 0xCA)

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

    ULONG_PTR gdiplusToken_;
    Gdiplus::Image* roombaImageBasic_;
    Gdiplus::Image* roombaImageAdvanced_;
    Gdiplus::Image* roombaImagePremium_;
    Gdiplus::Image* dirtImage1_;
    Gdiplus::Image* dirtImage2_;
    Gdiplus::Image* dirtImage3_;
    Gdiplus::Image* dirtImage4_;
    Gdiplus::Image* nenufar1_;
    Gdiplus::Image* nenufar2_;
    Gdiplus::Image* tronco1_;
    Gdiplus::Image* tronco2_;
    Gdiplus::Font* customFont_;

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

    bool loadImages();
    bool loadCustomFont();
    void drawRoombaImage(Gdiplus::Graphics& graphics, double x, double y, double angle,
        COLORREF tintColor, int size, Roomba::Type type);

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void paintScreen(HDC hdc, RECT& rect);
    void paintStartScreen(HDC hdc, RECT& rect);
    void paintConfigScreen(HDC hdc, RECT& rect);
    void paintCleaningScreen(HDC hdc, RECT& rect);
    void paintFinishedScreen(HDC hdc, RECT& rect);

    void drawZoneMiniMap(HDC hdc, std::shared_ptr<Zone> zone, int x, int y, int w, int h,
        COLORREF accentColor, size_t zoneIndex);
    void drawRoombaOnZone(HDC hdc, std::shared_ptr<Roomba> roomba, std::shared_ptr<Zone> zone,
        int zoneX, int zoneY, int zoneW, int zoneH);

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