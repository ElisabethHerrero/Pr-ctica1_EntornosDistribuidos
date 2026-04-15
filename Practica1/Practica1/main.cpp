#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include "ScreenManager.h"
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    ScreenManager app;

    if (!app.initialize()) {
        MessageBox(nullptr, L"Error al inicializar la aplicacion", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    app.run();

    return 0;
}