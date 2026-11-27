#include "raylib.h"
#include "game.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#ifdef __ANDROID__
#include "raymob.h"
#endif

// Cartella dei salvataggi: su Android lo storage interno dell'app, su Windows la cartella corrente (come prima).
static std::string DataDir() {
#ifdef __ANDROID__
    struct android_app* app = GetAndroidApp();
    if (app && app->activity && app->activity->internalDataPath) return std::string(app->activity->internalDataPath) + "/";
#endif
    return std::string();
}

int main() {
    std::srand((unsigned int)std::time(nullptr));
#ifdef __ANDROID__
    SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_WINDOW_HIGHDPI);
    InitWindow(0, 0, "Palla VS Palla 3D");
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Palla VS Palla 3D");
    SetWindowMinSize(800, 480);
#endif
    SetExitKey(KEY_NULL);                 // ESC / Indietro = torna al menu, non chiude il gioco
    InitAudioDevice(); SetTargetFPS(60);

    PallaGame game; game.Init(DataDir());
    bool wasDown = false; Vector2 lastRaw = { 0, 0 }, lastPos = { 0, 0 }, pressPos = { 0, 0 }; float dragDist = 0.0f;

    while (!WindowShouldClose()) {
        float dt = std::min(0.05f, GetFrameTime());
        int sw = std::max(1, GetScreenWidth()), sh = std::max(1, GetScreenHeight());
        // Il gioco si disegna direttamente a risoluzione nativa (nitido, MSAA su PC): layout virtuale alto 600 e largo quanto serve.
        float scale = std::min((float)sh / (float)VIRT_H, (float)sw / 980.0f), virtW = (float)sw / scale;

        int tc = GetTouchPointCount(); Vector2 raw = lastRaw; bool isDown;
#ifdef __ANDROID__
        if (tc > 0) raw = GetTouchPosition(0);
        isDown = tc > 0;
#else
        if (tc > 0) { raw = GetTouchPosition(0); isDown = true; }
        else { raw = GetMousePosition(); isDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT); }
#endif
        lastRaw = raw;
        InputState in{};
        in.pos = { raw.x / scale, raw.y / scale };
        in.pressed = isDown && !wasDown; in.released = !isDown && wasDown; in.down = isDown;
        if (in.pressed) { pressPos = in.pos; dragDist = 0.0f; lastPos = in.pos; }
        if (isDown && wasDown) { in.dragDx = in.pos.x - lastPos.x; in.dragDy = in.pos.y - lastPos.y; dragDist += std::sqrt(in.dragDx * in.dragDx + in.dragDy * in.dragDy); }
        if (in.released) in.pos = lastPos;   // al rilascio la posizione e' quella dell'ultimo frame premuto
        in.pressPos = pressPos; in.dragDist = dragDist; lastPos = isDown ? in.pos : lastPos; wasDown = isDown;
        in.wheel = GetMouseWheelMove();

        for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) {
            if (cp >= 32 && cp != 127) { int n = 0; const char* u = CodepointToUTF8(cp, &n); in.typed.append(u, n); }
        }
        in.backspaces = (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) ? 1 : 0;
        in.enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        in.ctrlV = (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V);
        in.back = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK);
        std::string nt; if (PollNativeInputDialog(nt)) { in.nativeTextReady = true; in.nativeText = nt; }

        BeginDrawing();
        game.Frame(dt, in, virtW, scale);
        EndDrawing();
    }
    game.Shutdown(); CloseAudioDevice(); CloseWindow();
    return 0;
}
