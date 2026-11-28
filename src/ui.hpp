#ifndef UI_HPP
#define UI_HPP
#include "raylib.h"
#include <string>

// Stato input in coordinate "virtuali" (altezza fissa 600, larghezza dinamica).
struct InputState {
    Vector2 pos{0, 0};        // posizione attuale
    Vector2 pressPos{0, 0};   // dove e' iniziato il tocco/click
    bool pressed = false;     // frame in cui inizia il tocco
    bool down = false;        // tocco/click in corso
    bool released = false;    // frame in cui finisce il tocco
    float wheel = 0.0f;       // rotellina mouse
    float dragDx = 0.0f, dragDy = 0.0f; // spostamento dall'ultimo frame mentre si tiene premuto
    float dragDist = 0.0f;    // distanza totale percorsa dall'inizio del tocco
    std::string typed;        // caratteri digitati in questo frame (UTF-8)
    int backspaces = 0;       // quanti backspace in questo frame
    bool enter = false, ctrlV = false, back = false;
    bool nativeTextReady = false; std::string nativeText; // risultato tastiera nativa Android
};

namespace Pal {
    constexpr Color Bg0{7, 10, 20, 255}, Bg1{15, 21, 40, 255};
    constexpr Color Panel{19, 26, 46, 240}, PanelHi{30, 40, 68, 255}, Border{62, 80, 124, 255};
    constexpr Color Text{236, 242, 255, 255}, Muted{148, 164, 200, 255}, Dim{96, 110, 144, 255};
    constexpr Color Cyan{34, 211, 238, 255}, Green{46, 229, 121, 255}, Red{255, 72, 102, 255};
    constexpr Color Gold{245, 180, 25, 255}, Purple{168, 85, 247, 255}, Blue{59, 130, 246, 255}, Orange{251, 146, 60, 255}, Pink{244, 114, 182, 255};
}

Color Mix(Color a, Color b, float t);
Color Lighten(Color c, float f);   // f>1 schiarisce, f<1 scurisce
Color Fade2(Color c, float a);     // alpha 0..1

class UI {
public:
    Font font{}, bold{};
    bool fontsLoaded = false;
    float W = 1067.0f, H = 600.0f, scale = 1.0f;
    InputState in;
    bool enabled = true;   // false: i widget ignorano i click (sotto un modale)
    Texture2D glow{};

    void Init();       // font di sistema + texture glow
    void Unload();
    void Begin(const InputState& input, float virtW, float uiScale);

    // Testo
    Vector2 Measure(const char* s, float sz, bool b = false) const;
    void Text(const char* s, float x, float y, float sz, Color c, bool b = false) const;
    void TextC(const char* s, float cx, float cy, float sz, Color c, bool b = true) const;
    void TextR(const char* s, float rx, float y, float sz, Color c, bool b = false) const;
    void TextOut(const char* s, float cx, float cy, float sz, Color c, float th = 1.6f) const;
    float Wrap(const std::string& text, float x, float y, float maxW, float sz, Color c, bool draw = true, bool b = false, int maxLines = 0) const;
    std::string Fit(const std::string& s, float maxW, float sz, bool b = false) const; // taglia da sinistra (UTF-8 safe)

    // Forme
    void Shadow(Rectangle r, float radius, float spread, unsigned char alpha) const;
    void Panel(Rectangle r, float radius, Color fill, Color border, float bw = 1.5f, bool shadow = true) const;
    void Pill(Rectangle r, const char* label, Color bg, Color fg, float sz) const;
    void Bar(Rectangle r, float t, Color fill, Color back) const;
    void Heart(float cx, float cy, float s, Color c) const;
    void Orb(float cx, float cy, float r, Color c) const;   // pallina 2D con gradiente

    // Interazione
    bool Inside(Rectangle r, Vector2 p) const;
    bool Held(Rectangle r) const;      // premuto sopra r
    bool Click(Rectangle r) const;     // rilasciato sopra r senza trascinare
    bool Button(Rectangle r, const char* label, Color base, Color text, float sz, bool disabled = false) const;
    bool Slider(Rectangle r, float& v, float mn, float mx, Color accent, bool integer = false) const;   // trascinabile; true se cambia
    bool Chip(Rectangle r, const char* label, Color accent, bool active, float sz) const;

    // Scissor in coordinate virtuali
    void ScissorBegin(Rectangle r) const;
    void ScissorEnd() const;
};

#endif
