#include "raylib.h"
#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>

#ifdef __ANDROID__
#include "raymob.h"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <fstream>
#endif

static std::string GetDataFile(const char* name) {
#ifdef __ANDROID__
    struct android_app* app = GetAndroidApp();
    if (app && app->activity && app->activity->internalDataPath)
        return std::string(app->activity->internalDataPath) + "/" + name;
#endif
    return std::string(name);
}

static int ComputeVirtW(int sw, int sh) {
    if (sh <= 0) return 1066;
    return std::max(980, std::min(1500, (int)std::lround((float)VIRT_H * (float)sw / (float)sh)));
}

// Legge il clipboard Windows come UTF-8. Su altri OS ritorna stringa vuota.
static std::string GetClipboardUTF8() {
#ifdef _WIN32
    std::string out;
    if (OpenClipboard(nullptr)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* wstr = (wchar_t*)GlobalLock(hData);
            if (wstr) {
                int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    out.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &out[0], len, nullptr, nullptr);
                }
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
    return out;
#else
    return "";
#endif
}

int main() {
    std::srand((unsigned int)std::time(nullptr));

#ifdef __ANDROID__
    SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_WINDOW_HIGHDPI);
    InitWindow(0, 0, "Palla VS Palla 3D");
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Palla VS Palla 3D");
    SetWindowMinSize(800, 450);
#endif

    InitAudioDevice();
    SetTargetFPS(60);

    int virtW = ComputeVirtW(GetScreenWidth(), GetScreenHeight());
    RenderTexture2D target = LoadRenderTexture(virtW, VIRT_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    PallaGame game;
    game.Init(GetDataFile("palla_vs_palla_save.txt"),
              GetDataFile("palla_vs_palla_custom.txt"),
              GetDataFile("palla_gemini_key.txt"));

    bool wasDown = false;
    float lastDragY = 0.0f;

    while (!WindowShouldClose()) {
        float dt = std::min(0.05f, GetFrameTime());
        int sw = GetScreenWidth(), sh = GetScreenHeight(), wantedW = ComputeVirtW(sw, sh);

        if (wantedW != virtW) {
            virtW = wantedW;
            UnloadRenderTexture(target);
            target = LoadRenderTexture(virtW, VIRT_H);
            SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
        }

        Vector2 rawPos = (GetTouchPointCount() > 0) ? GetTouchPosition(0) : GetMousePosition();
        bool isDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || (GetTouchPointCount() > 0);
        bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || (isDown && !wasDown);

        InputState in{};
        in.pos = { rawPos.x * ((float)virtW / (float)std::max(1, sw)),
                   rawPos.y * ((float)VIRT_H / (float)std::max(1, sh)) };
        in.pressed = pressed;
        in.down = isDown;
        in.wheel = GetMouseWheelMove();
        if (isDown && wasDown) in.dragDeltaY = in.pos.y - lastDragY;
        in.keyChar = GetCharPressed();
        in.backspace = IsKeyPressed(KEY_BACKSPACE);
        in.enter = IsKeyPressed(KEY_ENTER);

        // Android: input nativo dal dialog JNI
        std::string nt;
        if (PollNativeInputDialog(nt)) {
            in.nativeTextReady = true;
            in.nativeText = nt;
        }

        // Windows: Ctrl+V paste del clipboard
#ifdef _WIN32
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
            std::string clip = GetClipboardUTF8();
            if (!clip.empty()) {
                in.nativeText = clip;
                in.nativeTextReady = true;
            }
        }
#endif

        wasDown = isDown;
        lastDragY = in.pos.y;

        game.Update(dt, in, virtW);

        BeginTextureMode(target);
        game.Draw(virtW);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(target.texture,
                       {0, 0, (float)virtW, -(float)VIRT_H},
                       {0, 0, (float)sw, (float)sh},
                       {0, 0}, 0, WHITE);
        EndDrawing();
    }

    game.Shutdown();
    UnloadRenderTexture(target);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}