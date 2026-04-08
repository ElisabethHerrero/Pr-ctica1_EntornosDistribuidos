#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include "CleaningService.h"
#include "Database.h"

#define SCREEN_START 0
#define SCREEN_CONFIG 1
#define SCREEN_CLEANING 2

#define COLOR_BG_DARK      RGB(30, 30, 46)
#define COLOR_BG_LIGHT     RGB(49, 50, 68)
#define COLOR_PRIMARY      RGB(137, 180, 250)
#define COLOR_SECONDARY    RGB(166, 227, 161)
#define COLOR_TEXT         RGB(205, 214, 244)
#define COLOR_TEXT_DIM     RGB(147, 153, 178)
#define COLOR_SUCCESS      RGB(166, 227, 161)
#define COLOR_WARNING      RGB(249, 226, 175)
#define COLOR_DANGER       RGB(243, 139, 168)
#define COLOR_ZONE1        RGB(137, 180, 250)
#define COLOR_ZONE2        RGB(166, 227, 161)
#define COLOR_ZONE3        RGB(249, 226, 175)
#define COLOR_ZONE4        RGB(243, 139, 168)

class ScreenManager {
public:
    ScreenManager();
    ~ScreenManager();

    bool initialize();
    void run();

private:
    HWND hwnd_;
    HINSTANCE hInstance_;
    int currentScreen_;

    CleaningService cleaningService_;
    std::vector<std::shared_ptr<Zone>> zones_;
    std::vector<std::shared_ptr<Roomba>> roombas_;
    std::vector<std::wstring> logs_;
    std::mutex logMutex_;

    int roombaCount_;
    Roomba::Type roombaType_;

    struct Button {
        int id;
        RECT rect;
        std::wstring text;
        COLORREF color;
    };
    std::vector<Button> buttons_;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void initializeZones();
    void initializeRoombas();
    void changeScreen(int screenId);
    void addLog(const wchar_t* msg);

    void paintScreen(HDC hdc, RECT& rect);
    void paintStartScreen(HDC hdc, RECT& rect);
    void paintConfigScreen(HDC hdc, RECT& rect);
    void paintCleaningScreen(HDC hdc, RECT& rect);

    void drawRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border);
    void drawText(HDC hdc, const wchar_t* text, int x, int y, COLORREF color, int size, bool bold);
    void drawButton(HDC hdc, const Button& btn);
    void drawProgressBar(HDC hdc, int x, int y, int w, int h, double progress, COLORREF color);

    void addButton(int id, int x, int y, int w, int h, const wchar_t* text, COLORREF color);
    void clearButtons();
    int hitTestButton(int x, int y);
    void handleButtonClick(int id);
};