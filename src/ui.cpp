#include "ui.hpp"
#include "font_data.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static float Cl01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static unsigned char Cb(float v) { return (unsigned char)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v)); }
static float Rn(Rectangle r, float radius) { float m = std::min(r.width, r.height) * 0.5f; return m <= 0.0f ? 0.0f : std::min(1.0f, radius / m); }
static float Sp(float sz) { return sz * 0.012f; }

Color Mix(Color a, Color b, float t) {
    t = Cl01(t);
    return { Cb(a.r + (b.r - a.r) * t), Cb(a.g + (b.g - a.g) * t), Cb(a.b + (b.b - a.b) * t), Cb(a.a + (b.a - a.a) * t) };
}
Color Lighten(Color c, float f) {
    if (f >= 1.0f) { float t = f - 1.0f; return { Cb(c.r + (255 - c.r) * t), Cb(c.g + (255 - c.g) * t), Cb(c.b + (255 - c.b) * t), c.a }; }
    return { Cb(c.r * f), Cb(c.g * f), Cb(c.b * f), c.a };
}
Color Fade2(Color c, float a) { return { c.r, c.g, c.b, Cb(a * 255.0f) }; }

static std::string Lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }

#ifdef __ANDROID__
static std::string FindAndroidFont(bool wantBold) {
    FilePathList fl = LoadDirectoryFiles("/system/fonts");
    std::string best; int bestScore = -1000;
    for (unsigned int i = 0; i < fl.count; i++) {
        std::string p = fl.paths[i], low = Lower(p);
        if (low.size() < 4 || low.substr(low.size() - 4) != ".ttf") continue;
        int sc = 0;
        if (low.find("roboto") != std::string::npos) sc += 20;
        else if (low.find("samsung") != std::string::npos || low.find("notosans-") != std::string::npos || low.find("droidsans") != std::string::npos) sc += 8;
        else continue;
        if (low.find("mono") != std::string::npos || low.find("condensed") != std::string::npos || low.find("serif") != std::string::npos ||
            low.find("slab") != std::string::npos || low.find("flex") != std::string::npos) sc -= 40;
        if (low.find("italic") != std::string::npos) sc -= 30;
        if (wantBold) { if (low.find("bold") != std::string::npos) sc += 12; if (low.find("black") != std::string::npos || low.find("extra") != std::string::npos) sc -= 6; }
        else { if (low.find("regular") != std::string::npos) sc += 12; if (low.find("bold") != std::string::npos || low.find("light") != std::string::npos || low.find("thin") != std::string::npos) sc -= 12; }
        if (sc > bestScore) { bestScore = sc; best = p; }
    }
    UnloadDirectoryFiles(fl);
    return best;
}
#endif

static bool TryLoadFont(Font& out, const std::vector<std::string>& paths, const std::vector<int>& cps) {
    for (const auto& p : paths) {
        if (p.empty() || !FileExists(p.c_str())) continue;
        Font f = LoadFontEx(p.c_str(), 64, const_cast<int*>(cps.data()), (int)cps.size());
        if (f.texture.id > 0 && f.glyphCount > 0) {
#ifndef __ANDROID__
            GenTextureMipmaps(&f.texture);
            SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
#else
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
#endif
            out = f; return true;
        }
    }
    return false;
}

static bool TryLoadMem(Font& out, const unsigned char* data, int size, const std::vector<int>& cps) {
    Font f = LoadFontFromMemory(".ttf", data, size, 64, const_cast<int*>(cps.data()), (int)cps.size());
    if (f.texture.id > 0 && f.glyphCount > 0) {
#ifndef __ANDROID__
        GenTextureMipmaps(&f.texture); SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
#else
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
#endif
        out = f; return true;
    }
    return false;
}

void UI::Init() {
    std::vector<int> cps;
    for (int c = 32; c <= 126; c++) cps.push_back(c);
    for (int c = 160; c <= 255; c++) cps.push_back(c);
    const int extra[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC, 0x00D7 };
    for (int c : extra) if (c > 255 || c == 0x00D7) { if (std::find(cps.begin(), cps.end(), c) == cps.end()) cps.push_back(c); }

    std::vector<std::string> regular, boldp;
#ifdef __ANDROID__
    regular = { "/system/fonts/Roboto-Regular.ttf", "/system/fonts/RobotoStatic-Regular.ttf", FindAndroidFont(false), "/system/fonts/DroidSans.ttf" };
    boldp   = { "/system/fonts/Roboto-Bold.ttf", "/system/fonts/RobotoStatic-Bold.ttf", FindAndroidFont(true), "/system/fonts/DroidSans-Bold.ttf" };
#else
    regular = { "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/tahoma.ttf",
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/System/Library/Fonts/Helvetica.ttc" };
    boldp   = { "C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/seguisb.ttf", "C:/Windows/Fonts/arialbd.ttf", "C:/Windows/Fonts/tahomabd.ttf",
                "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" };
#endif
    bool okR = false, okB = false;
#ifdef __ANDROID__
    okR = TryLoadMem(font, kFontRegularData, (int)kFontRegularSize, cps);   // Android: font incorporato (affidabile)
    okB = TryLoadMem(bold, kFontBoldData, (int)kFontBoldSize, cps);
    if (!okR) okR = TryLoadFont(font, regular, cps);
    if (!okB) okB = TryLoadFont(bold, boldp, cps);
#else
    okR = TryLoadFont(font, regular, cps);                                   // Windows: Segoe UI di sistema, con ripiego incorporato
    okB = TryLoadFont(bold, boldp, cps);
    if (!okR) okR = TryLoadMem(font, kFontRegularData, (int)kFontRegularSize, cps);
    if (!okB) okB = TryLoadMem(bold, kFontBoldData, (int)kFontBoldSize, cps);
#endif
    if (!okR && okB) font = bold;
    if (!okB && okR) bold = font;
    fontsLoaded = okR || okB;
    if (!fontsLoaded) { font = GetFontDefault(); bold = font; }

    Image g = GenImageGradientRadial(128, 128, 0.0f, WHITE, { 255, 255, 255, 0 });
    glow = LoadTextureFromImage(g); UnloadImage(g);
    SetTextureFilter(glow, TEXTURE_FILTER_BILINEAR);
}

void UI::Unload() {
    if (fontsLoaded) { if (bold.texture.id != font.texture.id) UnloadFont(bold); UnloadFont(font); fontsLoaded = false; }
    if (glow.id > 0) UnloadTexture(glow);
}

void UI::Begin(const InputState& input, float virtW, float uiScale) { in = input; W = virtW; scale = uiScale; enabled = true; }

Vector2 UI::Measure(const char* s, float sz, bool b) const { return MeasureTextEx(b ? bold : font, s, sz, Sp(sz)); }
void UI::Text(const char* s, float x, float y, float sz, Color c, bool b) const { DrawTextEx(b ? bold : font, s, { x, y }, sz, Sp(sz), c); }
void UI::TextC(const char* s, float cx, float cy, float sz, Color c, bool b) const {
    Vector2 m = Measure(s, sz, b); DrawTextEx(b ? bold : font, s, { std::floor(cx - m.x * 0.5f), std::floor(cy - m.y * 0.5f + sz * 0.03f) }, sz, Sp(sz), c);
}
void UI::TextR(const char* s, float rx, float y, float sz, Color c, bool b) const {
    Vector2 m = Measure(s, sz, b); DrawTextEx(b ? bold : font, s, { rx - m.x, y }, sz, Sp(sz), c);
}
void UI::TextOut(const char* s, float cx, float cy, float sz, Color c, float th) const {
    Vector2 m = Measure(s, sz, true); float x = cx - m.x * 0.5f, y = cy - m.y * 0.5f + sz * 0.03f;
    Color o = { 6, 8, 16, (unsigned char)(c.a * 0.9f) };
    for (int i = 0; i < 8; i++) {
        float a = i * 0.7853982f; DrawTextEx(bold, s, { x + cosf(a) * th, y + sinf(a) * th }, sz, Sp(sz), o);
    }
    DrawTextEx(bold, s, { x, y }, sz, Sp(sz), c);
}

std::string UI::Fit(const std::string& s, float maxW, float sz, bool b) const {
    std::string t = s;
    while (!t.empty() && Measure(t.c_str(), sz, b).x > maxW) {
        size_t i = 1; while (i < t.size() && ((unsigned char)t[i] & 0xC0) == 0x80) i++;
        t.erase(0, i);
    }
    return t;
}

float UI::Wrap(const std::string& text, float x, float y, float maxW, float sz, Color c, bool draw, bool b, int maxLines) const {
    std::vector<std::string> lines; std::string line, word;
    auto flush = [&]() { lines.push_back(line); line.clear(); };
    auto addWord = [&](const std::string& w) {
        if (w.empty()) return;
        std::string test = line.empty() ? w : line + " " + w;
        if (Measure(test.c_str(), sz, b).x <= maxW) { line = test; return; }
        if (!line.empty()) flush();
        if (Measure(w.c_str(), sz, b).x <= maxW) { line = w; return; }
        std::string chunk; // parola lunghissima: spezza per caratteri
        for (size_t i = 0; i < w.size();) {
            size_t n = 1; while (i + n < w.size() && ((unsigned char)w[i + n] & 0xC0) == 0x80) n++;
            std::string t2 = chunk + w.substr(i, n);
            if (Measure(t2.c_str(), sz, b).x > maxW && !chunk.empty()) { lines.push_back(chunk); chunk = w.substr(i, n); } else chunk = t2;
            i += n;
        }
        line = chunk;
    };
    for (size_t i = 0; i <= text.size(); i++) {
        char ch = (i < text.size()) ? text[i] : '\0';
        if (ch == ' ' || ch == '\0' || ch == '\n') {
            addWord(word); word.clear();
            if (ch == '\n') flush();
        } else if (ch != '\r') word.push_back(ch);
    }
    if (!line.empty() || lines.empty()) flush();
    if (maxLines > 0 && (int)lines.size() > maxLines) {
        lines.resize(maxLines);
        std::string& last = lines.back();
        while (!last.empty() && Measure((last + "...").c_str(), sz, b).x > maxW) {
            size_t k = last.size() - 1; while (k > 0 && ((unsigned char)last[k] & 0xC0) == 0x80) k--;
            last.erase(k);
        }
        last += "...";
    }
    float lh = sz * 1.28f;
    if (draw) for (size_t i = 0; i < lines.size(); i++) Text(lines[i].c_str(), x, y + i * lh, sz, c, b);
    return lines.size() * lh;
}

void UI::Shadow(Rectangle r, float radius, float spread, unsigned char alpha) const {
    for (int i = 3; i >= 1; i--) {
        float g = spread * (float)i / 3.0f;
        Rectangle q = { r.x - g, r.y - g + spread * 0.55f, r.width + 2 * g, r.height + 2 * g };
        DrawRectangleRounded(q, Rn(q, radius + g), 10, { 0, 0, 0, (unsigned char)(alpha / (i + 1)) });
    }
}

void UI::Panel(Rectangle r, float radius, Color fill, Color border, float bw, bool shadow) const {
    if (shadow) Shadow(r, radius, 10.0f, 120);
    DrawRectangleRounded(r, Rn(r, radius), 12, fill);
    Rectangle hi = { r.x + 2, r.y + 2, r.width - 4, std::min(r.height * 0.35f, 60.0f) };
    DrawRectangleRounded(hi, Rn(hi, radius), 12, { 255, 255, 255, 7 });
    if (bw > 0.0f) DrawRectangleRoundedLinesEx(r, Rn(r, radius), 12, bw, border);
}

void UI::Pill(Rectangle r, const char* label, Color bg, Color fg, float sz) const {
    DrawRectangleRounded(r, 1.0f, 10, bg);
    TextC(label, r.x + r.width * 0.5f, r.y + r.height * 0.5f, sz, fg, true);
}

void UI::Bar(Rectangle r, float t, Color fill, Color back) const {
    DrawRectangleRounded(r, 1.0f, 8, back);
    t = Cl01(t);
    if (t > 0.0f) { Rectangle f = { r.x, r.y, std::max(r.height, r.width * t), r.height }; DrawRectangleRounded(f, 1.0f, 8, fill); }
}

void UI::Heart(float cx, float cy, float s, Color c) const {
    float R = s * 0.56f;
    DrawCircleV({ cx - s * 0.48f, cy - s * 0.30f }, R, c);
    DrawCircleV({ cx + s * 0.48f, cy - s * 0.30f }, R, c);
    DrawTriangle({ cx, cy + s * 1.02f }, { cx + s * 0.99f, cy - s * 0.04f }, { cx - s * 0.99f, cy - s * 0.04f }, c);
}

void UI::Orb(float cx, float cy, float r, Color c) const {
    DrawCircleV({ cx, cy + r * 0.18f }, r * 1.02f, { 0, 0, 0, 90 });
    DrawCircleGradient((int)cx, (int)cy, r, Lighten(c, 1.28f), Lighten(c, 0.55f));
    DrawCircleV({ cx - r * 0.34f, cy - r * 0.36f }, r * 0.26f, { 255, 255, 255, 120 });
}

bool UI::Inside(Rectangle r, Vector2 p) const { return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height; }
bool UI::Held(Rectangle r) const { return enabled && in.down && Inside(r, in.pressPos) && Inside(r, in.pos); }
bool UI::Click(Rectangle r) const { return enabled && in.released && in.dragDist < 14.0f && Inside(r, in.pressPos) && Inside(r, in.pos); }

bool UI::Button(Rectangle r, const char* label, Color base, Color text, float sz, bool disabled) const {
    bool held = !disabled && Held(r);
    Color b = disabled ? Color{ 38, 46, 70, 255 } : base;
    Color t = disabled ? Color{ 108, 120, 154, 255 } : text;
    float rad = std::min(14.0f, r.height * 0.32f), depth = disabled ? 0.0f : (held ? 1.0f : 3.5f);
    Rectangle q = r; q.y += (held ? 2.5f : 0.0f);
    if (!disabled) Shadow(r, rad, 7.0f, 95);
    if (depth > 0.0f) DrawRectangleRounded({ q.x, q.y + depth, q.width, q.height }, Rn(q, rad), 12, Lighten(b, 0.5f));
    DrawRectangleRounded(q, Rn(q, rad), 12, held ? Lighten(b, 0.92f) : b);
    Rectangle hi = { q.x + 2, q.y + 2, q.width - 4, q.height * 0.46f };
    DrawRectangleRounded(hi, Rn(hi, rad), 12, { 255, 255, 255, (unsigned char)(disabled ? 6 : 34) });
    DrawRectangleRoundedLinesEx(q, Rn(q, rad), 12, 1.5f, { 255, 255, 255, (unsigned char)(disabled ? 18 : 62) });
    if (label && label[0]) TextC(label, q.x + q.width * 0.5f, q.y + q.height * 0.5f, sz, t, true);
    return !disabled && Click(r);
}

bool UI::Slider(Rectangle r, float& v, float mn, float mx, Color accent, bool integer) const {
    bool changed = false; float pad = 11.0f, tw = r.width - pad * 2.0f;
    if (enabled && in.down && Inside(r, in.pressPos)) {
        float t = Cl01((in.pos.x - (r.x + pad)) / std::max(1.0f, tw)), nv = mn + t * (mx - mn);
        if (integer) nv = std::floor(nv + 0.5f);
        if (nv != v) { v = nv; changed = true; }
    }
    float t = Cl01((v - mn) / std::max(0.0001f, mx - mn)), cy = r.y + r.height * 0.5f, hx = r.x + pad + tw * t;
    DrawRectangleRounded({ r.x + pad, cy - 4, tw, 8 }, 1.0f, 8, { 36, 44, 70, 255 });
    DrawRectangleRounded({ r.x + pad, cy - 4, std::max(8.0f, tw * t), 8 }, 1.0f, 8, accent);
    DrawCircleV({ hx, cy + 1.5f }, 11, { 0, 0, 0, 80 }); DrawCircleV({ hx, cy }, 11, Lighten(accent, 1.35f)); DrawCircleV({ hx, cy }, 6.5f, accent);
    return changed;
}

bool UI::Chip(Rectangle r, const char* label, Color accent, bool active, float sz) const {
    bool held = Held(r);
    if (active) {
        DrawRectangleRounded(r, 1.0f, 10, accent);
        TextC(label, r.x + r.width * 0.5f, r.y + r.height * 0.5f, sz, { 10, 14, 26, 255 }, true);
    } else {
        DrawRectangleRounded(r, 1.0f, 10, held ? Color{ 40, 52, 86, 255 } : Color{ 24, 32, 56, 235 });
        DrawRectangleRoundedLinesEx(r, 1.0f, 10, 1.5f, Fade2(accent, 0.75f));
        TextC(label, r.x + r.width * 0.5f, r.y + r.height * 0.5f, sz, Mix(accent, WHITE, 0.45f), true);
    }
    return Click(r);
}

// Ritaglio in coordinate virtuali. NON usa BeginScissorMode: su Android con HIGHDPI raylib moltiplica le coordinate per la densita'
// dello schermo (le liste comparivano spostate e tagliate). Qui si parla direttamente con OpenGL, in pixel del framebuffer.
void UI::ScissorBegin(Rectangle r) const {
    rlDrawRenderBatchActive();
    int sh = std::max(1, GetScreenHeight()); float f = (float)GetRenderHeight() / (float)sh;   // pixel framebuffer / unita' schermo
    int x = (int)std::floor(r.x * scale * f), w = std::max(0, (int)std::ceil(r.width * scale * f)), h = std::max(0, (int)std::ceil(r.height * scale * f));
    int y = (int)std::floor((float)sh * f - (r.y + r.height) * scale * f);
    rlEnableScissorTest(); rlScissor(x, y, w, h);
}
void UI::ScissorEnd() const { rlDrawRenderBatchActive(); rlDisableScissorTest(); }
