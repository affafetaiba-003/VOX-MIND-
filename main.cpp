// ============================================================
//  VoxMind Qt — Entry Point
//  OOP Project 2026  |  Team of 3
//
//  CRITICAL: CoInitializeEx MUST be called BEFORE QApplication
//  and BEFORE any SAPI objects are created.
//  COINIT_APARTMENTTHREADED is required for SAPI voice output.
//  This is why Narrator works but basic CoInitialize(NULL) did not.
// ============================================================

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QApplication>
#include <QFont>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    // ── Step 1: Init COM with APARTMENT threading (SAPI requires this) ──
    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if(FAILED(hrCom)){
        // Fallback to single-threaded apartment
        CoInitialize(NULL);
    }

    // ── Step 2: Create Qt application ───────────────────────────────────
    QApplication app(argc, argv);
    app.setApplicationName("VoxMind");
    app.setApplicationVersion("2.0");
    app.setOrganizationName("OOP Project 2026");

    // Set default font
    QFont font("Segoe UI", 10);
    app.setFont(font);

    // ── Step 3: Create and show main window ─────────────────────────────
    MainWindow window;
    window.show();

    // ── Step 4: Run event loop ──────────────────────────────────────────
    int result = app.exec();

    // ── Step 5: Uninitialize COM ────────────────────────────────────────
    CoUninitialize();

    return result;
}
