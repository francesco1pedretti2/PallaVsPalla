#include "render.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// ============================================================================
//  Shader di illuminazione (GLSL 330 su Windows, GLSL ES 1.00 su Android)
// ============================================================================
#ifdef __ANDROID__
static const char* kVS = R"(#version 100
attribute vec3 vertexPosition; attribute vec2 vertexTexCoord; attribute vec3 vertexNormal;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
varying vec3 fragPos; varying vec3 fragNormal; varying vec2 fragUV;
void main(){
  fragPos = vec3(matModel*vec4(vertexPosition,1.0));
  fragNormal = normalize(vec3(matNormal*vec4(vertexNormal,0.0)));
  fragUV = vertexTexCoord;
  gl_Position = mvp*vec4(vertexPosition,1.0);
})";
static const char* kFS = R"(#version 100
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying vec3 fragPos; varying vec3 fragNormal; varying vec2 fragUV;
uniform sampler2D texture0; uniform vec4 colDiffuse; uniform vec3 viewPos; uniform vec3 lightDir;
uniform float spec; uniform float rim; uniform vec3 rimColor; uniform vec3 ambSky; uniform vec3 ambGnd;
void main(){
  vec4 tex = texture2D(texture0, fragUV)*colDiffuse;
  vec3 N = normalize(fragNormal); vec3 V = normalize(viewPos-fragPos); vec3 L = normalize(lightDir);
  float ndl = dot(N,L); float diff = max(ndl,0.0); float wrap = clamp(ndl*0.5+0.5,0.0,1.0);
  vec3 amb = mix(ambGnd, ambSky, N.y*0.5+0.5);
  vec3 H = normalize(L+V);
  float s = pow(max(dot(N,H),0.0), 70.0)*spec;
  float fres = pow(1.0-max(dot(N,V),0.0), 2.6);
  vec3 col = tex.rgb*(amb + vec3(1.0,0.97,0.92)*(diff*0.78 + wrap*0.22)) + vec3(s) + rimColor*fres*rim;
  gl_FragColor = vec4(col, tex.a);
})";
#else
static const char* kVS = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
out vec3 fragPos; out vec3 fragNormal; out vec2 fragUV;
void main(){
  fragPos = vec3(matModel*vec4(vertexPosition,1.0));
  fragNormal = normalize(vec3(matNormal*vec4(vertexNormal,0.0)));
  fragUV = vertexTexCoord;
  gl_Position = mvp*vec4(vertexPosition,1.0);
})";
static const char* kFS = R"(#version 330
in vec3 fragPos; in vec3 fragNormal; in vec2 fragUV;
uniform sampler2D texture0; uniform vec4 colDiffuse; uniform vec3 viewPos; uniform vec3 lightDir;
uniform float spec; uniform float rim; uniform vec3 rimColor; uniform vec3 ambSky; uniform vec3 ambGnd;
out vec4 finalColor;
void main(){
  vec4 tex = texture(texture0, fragUV)*colDiffuse;
  vec3 N = normalize(fragNormal); vec3 V = normalize(viewPos-fragPos); vec3 L = normalize(lightDir);
  float ndl = dot(N,L); float diff = max(ndl,0.0); float wrap = clamp(ndl*0.5+0.5,0.0,1.0);
  vec3 amb = mix(ambGnd, ambSky, N.y*0.5+0.5);
  vec3 H = normalize(L+V);
  float s = pow(max(dot(N,H),0.0), 70.0)*spec;
  float fres = pow(1.0-max(dot(N,V),0.0), 2.6);
  vec3 col = tex.rgb*(amb + vec3(1.0,0.97,0.92)*(diff*0.78 + wrap*0.22)) + vec3(s) + rimColor*fres*rim;
  finalColor = vec4(col, tex.a);
})";
#endif

struct Gfx {
    bool ready = false;
    Shader lit{}; int locView = -1, locLight = -1, locSpec = -1, locRim = -1, locRimCol = -1, locSky = -1, locGnd = -1;
    Model sphere{}, cube{}, cylY{}, cylX{}, coneY{}, coneX{}, plane{};      // illuminati
    Model fSphere{}, fPlaneShadow{}, fPlaneGlow{}, fPlaneDisc{};            // piatti (shader di default)
    Texture2D floorTex{}, shadowTex{}, glowTex{}, discTex{};
    float lastSpec = -1, lastRim = -1; Color lastRimC{0, 0, 0, 0};
} G;

static Color Mixc(Color a, Color b, float t) { t = std::max(0.0f, std::min(1.0f, t)); return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t), (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t) }; }
static Color Alpha(Color c, float a) { return { c.r, c.g, c.b, (unsigned char)(std::max(0.0f, std::min(1.0f, a)) * 255.0f) }; }
static float Rf(float a, float b) { return a + (b - a) * ((float)GetRandomValue(0, 10000) / 10000.0f); }

static Model MakeLit(Mesh m) { Model md = LoadModelFromMesh(m); md.materials[0].shader = G.lit; return md; }

static Texture2D MakeRadial(float density, Color inner, Color outer, int size) {
    Image im = GenImageGradientRadial(size, size, density, inner, outer);
    Texture2D t = LoadTextureFromImage(im); UnloadImage(im); SetTextureFilter(t, TEXTURE_FILTER_BILINEAR); SetTextureWrap(t, TEXTURE_WRAP_CLAMP); return t;
}

static Texture2D MakeFloorTexture() {
    const int S = 1024, N = 14; const float T = (float)S / N;
    Image im = GenImageColor(S, S, { 30, 42, 74, 255 });
    for (int ty = 0; ty < N; ty++) for (int tx = 0; tx < N; tx++) {
        bool a = ((tx + ty) & 1) == 0; Color c = a ? Color{ 31, 44, 78, 255 } : Color{ 37, 52, 92, 255 };
        int x0 = (int)(tx * T), y0 = (int)(ty * T), x1 = (int)((tx + 1) * T), y1 = (int)((ty + 1) * T);
        ImageDrawRectangle(&im, x0, y0, x1 - x0, y1 - y0, c);
    }
    Color seam = { 58, 78, 124, 255 };
    for (int k = 0; k <= N; k++) {
        int p = std::min(S - 2, (int)(k * T));
        ImageDrawLine(&im, p, 0, p, S - 1, seam); ImageDrawLine(&im, p + 1, 0, p + 1, S - 1, seam);
        ImageDrawLine(&im, 0, p, S - 1, p, seam); ImageDrawLine(&im, 1, p, S - 1, p, seam);
    }
    Color mark = { 96, 132, 196, 255 }; int cx = S / 2, cy = S / 2;
    for (int d = -2; d <= 2; d++) ImageDrawCircleLines(&im, cx, cy, (int)(T * 2.4f) + d, mark);
    for (int d = -2; d <= 2; d++) ImageDrawCircleLines(&im, cx, cy, (int)(T * 0.35f) + d, mark);
    ImageDrawLine(&im, cx - 1, 0, cx - 1, S - 1, mark); ImageDrawLine(&im, cx, 0, cx, S - 1, mark); ImageDrawLine(&im, cx + 1, 0, cx + 1, S - 1, mark);
    // tinte squadra + vignettatura + grana: tutto in un passaggio sui pixel
    unsigned char* px = (unsigned char*)im.data; unsigned int seed = 12345u;
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        unsigned char* p = px + (y * S + x) * 4; float nx = (x - cx) / (float)cx, ny = (y - cy) / (float)cy;
        float edge = std::max(std::fabs(nx), std::fabs(ny)); float vig = 1.0f - 0.30f * edge * edge * edge * edge;
        float r = p[0], g = p[1], b = p[2];
        float zl = std::max(0.0f, (-nx - 0.30f) / 0.70f), zr = std::max(0.0f, (nx - 0.30f) / 0.70f);
        g += zl * 26.0f; r -= zl * 8.0f; b -= zl * 6.0f;           // lato alleato: verde
        r += zr * 30.0f; g -= zr * 8.0f; b -= zr * 8.0f;           // lato nemico: rosso
        seed = seed * 1664525u + 1013904223u; float nz = (float)((seed >> 24) & 7) - 3.5f;
        p[0] = (unsigned char)std::max(0.0f, std::min(255.0f, r * vig + nz)); p[1] = (unsigned char)std::max(0.0f, std::min(255.0f, g * vig + nz)); p[2] = (unsigned char)std::max(0.0f, std::min(255.0f, b * vig + nz));
    }
    Texture2D t = LoadTextureFromImage(im); UnloadImage(im);
#ifndef __ANDROID__
    GenTextureMipmaps(&t); SetTextureFilter(t, TEXTURE_FILTER_ANISOTROPIC_8X);
#else
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
#endif
    SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
    return t;
}

void GfxInit() {
    if (G.ready) return;
    G.lit = LoadShaderFromMemory(kVS, kFS);
    G.locView = GetShaderLocation(G.lit, "viewPos"); G.locLight = GetShaderLocation(G.lit, "lightDir");
    G.locSpec = GetShaderLocation(G.lit, "spec"); G.locRim = GetShaderLocation(G.lit, "rim"); G.locRimCol = GetShaderLocation(G.lit, "rimColor");
    G.locSky = GetShaderLocation(G.lit, "ambSky"); G.locGnd = GetShaderLocation(G.lit, "ambGnd");
    G.sphere = MakeLit(GenMeshSphere(1.0f, 40, 40));
    G.cube = MakeLit(GenMeshCube(1.0f, 1.0f, 1.0f));
    G.cylY = MakeLit(GenMeshCylinder(0.5f, 1.0f, 28));
    G.coneY = MakeLit(GenMeshCone(0.5f, 1.0f, 20));
    G.cylX = MakeLit(GenMeshCylinder(0.5f, 1.0f, 20)); G.cylX.transform = MatrixRotateZ(-PI * 0.5f);   // asse lungo +X
    G.coneX = MakeLit(GenMeshCone(0.5f, 1.0f, 20)); G.coneX.transform = MatrixRotateZ(-PI * 0.5f);
    G.plane = MakeLit(GenMeshPlane(1.0f, 1.0f, 1, 1));
    G.fSphere = LoadModelFromMesh(GenMeshSphere(1.0f, 16, 16));
    G.fPlaneShadow = LoadModelFromMesh(GenMeshPlane(1.0f, 1.0f, 1, 1));
    G.fPlaneGlow = LoadModelFromMesh(GenMeshPlane(1.0f, 1.0f, 1, 1));
    G.fPlaneDisc = LoadModelFromMesh(GenMeshPlane(1.0f, 1.0f, 1, 1));
    G.floorTex = MakeFloorTexture(); SetMaterialTexture(&G.plane.materials[0], MATERIAL_MAP_DIFFUSE, G.floorTex);
    G.shadowTex = MakeRadial(0.15f, { 0, 0, 0, 255 }, { 0, 0, 0, 0 }, 128); SetMaterialTexture(&G.fPlaneShadow.materials[0], MATERIAL_MAP_DIFFUSE, G.shadowTex);
    G.glowTex = MakeRadial(0.0f, WHITE, { 255, 255, 255, 0 }, 128); SetMaterialTexture(&G.fPlaneGlow.materials[0], MATERIAL_MAP_DIFFUSE, G.glowTex);
    G.discTex = MakeRadial(0.90f, WHITE, { 255, 255, 255, 0 }, 256); SetMaterialTexture(&G.fPlaneDisc.materials[0], MATERIAL_MAP_DIFFUSE, G.discTex);
    G.ready = true;
}

// ============================================================================
//  Helper di disegno
// ============================================================================
static void SceneBegin(const Camera3D& cam) {
    Vector3 L = Vector3Normalize({ -0.45f, 0.80f, -0.40f });   // luce da in alto a sinistra (schermo)
    float sky[3] = { 0.58f, 0.64f, 0.82f }, gnd[3] = { 0.20f, 0.22f, 0.32f };
    SetShaderValue(G.lit, G.locLight, &L, SHADER_UNIFORM_VEC3);
    SetShaderValue(G.lit, G.locView, &cam.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(G.lit, G.locSky, sky, SHADER_UNIFORM_VEC3); SetShaderValue(G.lit, G.locGnd, gnd, SHADER_UNIFORM_VEC3);
    G.lastSpec = -1; G.lastRim = -1; G.lastRimC = { 1, 2, 3, 4 };
}
static void Mat(float spec, float rim, Color rc) {
    if (spec != G.lastSpec) { SetShaderValue(G.lit, G.locSpec, &spec, SHADER_UNIFORM_FLOAT); G.lastSpec = spec; }
    if (rim != G.lastRim) { SetShaderValue(G.lit, G.locRim, &rim, SHADER_UNIFORM_FLOAT); G.lastRim = rim; }
    if (rc.r != G.lastRimC.r || rc.g != G.lastRimC.g || rc.b != G.lastRimC.b) { float v[3] = { rc.r / 255.0f, rc.g / 255.0f, rc.b / 255.0f }; SetShaderValue(G.lit, G.locRimCol, v, SHADER_UNIFORM_VEC3); G.lastRimC = rc; }
}
static const Vector3 UP = { 0, 1, 0 };
static void LSphere(Vector3 c, Vector3 radii, Color col, float spec = 0.8f, float rim = 0.35f, Color rc = { 200, 220, 255, 255 }) { Mat(spec, rim, rc); DrawModelEx(G.sphere, c, UP, 0, radii, col); }
static void LCube(Vector3 c, Vector3 size, Color col, float yawDeg = 0, float spec = 0.35f, float rim = 0.12f) { Mat(spec, rim, { 160, 190, 255, 255 }); DrawModelEx(G.cube, c, UP, yawDeg, size, col); }
static void LCylY(Vector3 base, float r, float h, Color col, float spec = 0.5f, float rim = 0.2f) { Mat(spec, rim, { 170, 200, 255, 255 }); DrawModelEx(G.cylY, base, UP, 0, { r * 2, h, r * 2 }, col); }
static void LConeY(Vector3 base, float r, float h, Color col, float spec = 0.7f, float rim = 0.25f) { Mat(spec, rim, { 220, 230, 255, 255 }); DrawModelEx(G.coneY, base, UP, 0, { r * 2, h, r * 2 }, col); }
// Asta/lama orizzontale: parte da 'base', direzione 'ang' (rad), lunghezza len, spessore verticale 'thick', larghezza 'wide'.
static void ArmX(const Model& m, Vector3 base, float ang, float len, float thick, float wide, Color col, float spec = 0.9f, float rim = 0.25f) {
    Mat(spec, rim, { 220, 235, 255, 255 }); DrawModelEx(m, base, UP, -ang * RAD2DEG, { len, thick, wide }, col);
}
static void FSphere(Vector3 c, float r, Color col) { DrawModelEx(G.fSphere, c, UP, 0, { r, r, r }, col); }
static void Plane(Model& m, float x, float y, float z, float sx, float sz, Color col, float yawDeg = 0) { DrawModelEx(m, { x, y, z }, UP, yawDeg, { sx, 1.0f, sz }, col); }
static void Disc(float x, float z, float r, Color col, float y = 0.03f) { Plane(G.fPlaneDisc, x, y, z, r * 2, r * 2, col); }
static void Halo(float x, float z, float r, Color col, float y = 0.04f) { Plane(G.fPlaneGlow, x, y, z, r * 2, r * 2, col); }
static void Ring(Vector3 c, float rIn, float rOut, Color col, float a0 = 0.0f, float a1 = 6.2831853f) {
    int seg = std::max(28, (int)(rOut * 16.0f * (a1 - a0) / 6.2831853f));
    rlDisableBackfaceCulling(); rlBegin(RL_TRIANGLES); rlColor4ub(col.r, col.g, col.b, col.a);
    for (int i = 0; i < seg; i++) {
        float s0 = a0 + (a1 - a0) * i / seg, s1 = a0 + (a1 - a0) * (i + 1) / seg;
        float c0 = cosf(s0), n0 = sinf(s0), c1 = cosf(s1), n1 = sinf(s1);
        rlVertex3f(c.x + c0 * rIn, c.y, c.z + n0 * rIn); rlVertex3f(c.x + c0 * rOut, c.y, c.z + n0 * rOut); rlVertex3f(c.x + c1 * rIn, c.y, c.z + n1 * rIn);
        rlVertex3f(c.x + c1 * rIn, c.y, c.z + n1 * rIn); rlVertex3f(c.x + c0 * rOut, c.y, c.z + n0 * rOut); rlVertex3f(c.x + c1 * rOut, c.y, c.z + n1 * rOut);
    }
    rlEnd(); rlEnableBackfaceCulling();
}
static void GlowBB(const Camera3D& cam, Vector3 p, float size, Color c) { DrawBillboard(cam, G.glowTex, p, size, c); }

Camera3D MakeArenaCamera(float shake) {
    Camera3D cam{}; float ox = shake > 0.001f ? Rf(-shake, shake) : 0.0f, oz = shake > 0.001f ? Rf(-shake, shake) : 0.0f;
    cam.position = { ox, 23.0f, 0.01f + oz }; cam.target = { ox, 0.0f, oz }; cam.up = { 0.0f, 0.0f, -1.0f };
    cam.fovy = 43.0f; cam.projection = CAMERA_PERSPECTIVE; return cam;
}

// ============================================================================
//  Palla 3D: corpo lucido + armi (mesh vere, illuminate)
// ============================================================================
static void DrawBallBody(const Camera3D& cam, const BallDef* def, const BallDef* fused, Vector2 p, float r, float angle, float look,
                         const Color* teamCol, float sqX, float sqY, float hitFlash, int blades, int shield, float time) {
    Color col = (hitFlash > 0.0f) ? Mixc(def->color, WHITE, 0.85f) : def->color;
    const std::string& bid = def->id;
    auto hasSkill = [&](const char* s) { return bid == s || def->visualStyle == s || (fused && (fused->id == s || fused->visualStyle == s)); };
    float cy = r * sqY;

    // --- a terra: ombra morbida, anello squadra, aure ---
    rlDisableDepthMask();
    Plane(G.fPlaneShadow, p.x + r * 0.20f, 0.012f, p.y + r * 0.24f, r * 2.9f, r * 2.9f, { 0, 0, 0, 125 });
    if (teamCol) { Ring({ p.x, 0.045f, p.y }, r * 1.10f, r * 1.24f, Alpha(*teamCol, 0.95f)); Halo(p.x, p.y, r * 1.9f, Alpha(*teamCol, 0.22f), 0.03f); }
    if (hasSkill("zone") && teamCol) {
        float pu = r * 2.5f + sinf(angle * 4.0f) * 0.18f;
        Disc(p.x, p.y, pu, { 139, 92, 246, 46 }, 0.035f); Ring({ p.x, 0.07f, p.y }, pu - 0.1f, pu, { 190, 150, 255, 200 });
    }
    if (hasSkill("range") && teamCol) { float rr = r + 0.4f + fmodf(time * 2.8f, 3.2f); Ring({ p.x, 0.08f, p.y }, rr - 0.08f, rr, { 45, 212, 191, 190 }); }
    rlEnableDepthMask();

    // --- corpo ---
    Color rimc = Mixc(col, WHITE, 0.55f);
    LSphere({ p.x, cy, p.y }, { r * sqX, r * sqY, r * sqX }, col, 0.95f, 0.55f, rimc);
    if (fused) LSphere({ p.x + r * 0.30f, cy, p.y }, { r * 0.80f, r * 0.80f * sqY, r * 0.80f }, fused->color, 0.9f, 0.5f, Mixc(fused->color, WHITE, 0.5f));

    // --- armi ---
    if (hasSkill("lightwing")) {
        float fl = sinf(time * 10.0f) * 0.12f;
        for (int s = -1; s <= 1; s += 2) {
            LSphere({ p.x + s * (r + 0.62f), cy + 0.18f + fl, p.y + 0.15f }, { 0.78f, 0.07f, 0.34f }, { 254, 240, 138, 255 }, 0.6f, 0.9f, { 255, 255, 220, 255 });
            LSphere({ p.x + s * (r + 1.05f), cy + 0.22f + fl, p.y + 0.28f }, { 0.50f, 0.05f, 0.22f }, { 255, 250, 200, 255 }, 0.6f, 0.9f, WHITE);
        }
    }
    if (hasSkill("cactus")) {
        for (int i = 0; i < 8; i++) { float a = i * (PI * 0.25f) + angle * 0.2f; ArmX(G.coneX, { p.x + cosf(a) * r * 0.80f, cy, p.y + sinf(a) * r * 0.80f }, a, 0.60f, 0.16f, 0.16f, { 163, 230, 53, 255 }, 0.5f, 0.3f); }
    }
    if (hasSkill("blade") || blades > 0) {
        int cnt = std::max(hasSkill("blade") ? 2 : 0, blades);
        for (int i = 0; i < cnt; i++) {
            float a = angle + i * (2 * PI / std::max(1, cnt));
            ArmX(G.coneX, { p.x + cosf(a) * (r * 0.9f), cy, p.y + sinf(a) * (r * 0.9f) }, a, 1.35f, 0.07f, 0.40f, { 225, 242, 255, 255 }, 1.0f, 0.5f);
            ArmX(G.cylX, { p.x + cosf(a) * (r * 0.9f), cy, p.y + sinf(a) * (r * 0.9f) }, a, 0.14f, 0.14f, 0.52f, { 70, 80, 110, 255 }, 0.4f, 0.1f);
        }
    }
    if (hasSkill("orbit")) {
        for (int i = 0; i < 3; i++) {
            float a = angle + i * (2 * PI / 3); Vector3 c = { p.x + cosf(a) * (r + 1.35f), cy, p.y + sinf(a) * (r + 1.35f) };
            LSphere(c, { 0.30f, 0.30f, 0.30f }, { 216, 180, 254, 255 }, 1.0f, 0.8f, { 240, 220, 255, 255 });
        }
    }
    if (hasSkill("axe")) {
        Vector3 b = { p.x + cosf(angle) * r * 0.7f, cy, p.y + sinf(angle) * r * 0.7f };
        ArmX(G.cylX, b, angle, r + 0.35f, 0.15f, 0.15f, { 139, 90, 43, 255 }, 0.3f, 0.1f);
        Vector3 h = { p.x + cosf(angle) * (r + 0.95f), cy, p.y + sinf(angle) * (r + 0.95f) };
        Mat(1.0f, 0.5f, WHITE);
        DrawModelEx(G.sphere, h, UP, -angle * RAD2DEG, { 0.20f, 0.08f, 0.52f }, { 226, 232, 240, 255 });
    }
    if (hasSkill("spear")) {
        float a = angle * 0.8f; Vector3 b = { p.x + cosf(a) * r * 0.6f, cy, p.y + sinf(a) * r * 0.6f };
        ArmX(G.cylX, b, a, r + 0.65f, 0.11f, 0.11f, { 160, 120, 70, 255 }, 0.3f, 0.1f);
        ArmX(G.coneX, { p.x + cosf(a) * (r + 0.95f), cy, p.y + sinf(a) * (r + 0.95f) }, a, 0.62f, 0.20f, 0.20f, { 226, 232, 240, 255 }, 1.0f, 0.4f);
    }
    if (hasSkill("mg") || hasSkill("cannon") || hasSkill("ray")) {
        ArmX(G.cylX, { p.x + cosf(look) * r * 0.6f, cy, p.y + sinf(look) * r * 0.6f }, look, r * 0.55f + 0.55f, 0.40f, 0.40f, { 52, 62, 80, 255 }, 0.9f, 0.4f);
        ArmX(G.cylX, { p.x + cosf(look) * (r + 0.45f), cy, p.y + sinf(look) * (r + 0.45f) }, look, 0.16f, 0.50f, 0.50f, { 90, 102, 126, 255 }, 0.9f, 0.5f);
    }
    if (hasSkill("shackles") && teamCol) {
        for (int i = 0; i < 4; i++) { float a = -angle * 1.4f + i * 1.57f; LCube({ p.x + cosf(a) * (r + 0.35f), cy, p.y + sinf(a) * (r + 0.35f) }, { 0.26f, 0.26f, 0.26f }, { 255, 55, 115, 255 }, -a * RAD2DEG, 0.9f, 0.6f); }
    }
    // --- scudo: bolla traslucida ---
    if (shield > 0 && teamCol) {
        LSphere({ p.x, cy, p.y }, { r * 1.26f, r * 1.26f, r * 1.26f }, { 70, 220, 255, 60 }, 1.0f, 1.4f, { 160, 245, 255, 255 });
    }
    (void)cam;
}

// ============================================================================
//  Scena arena
// ============================================================================
static void DrawStatic(const ArenaMatch& m, float t) {
    // base + pavimento
    LCube({ 0, -0.64f, 0 }, { 16.6f, 1.2f, 16.6f }, { 10, 14, 26, 255 }, 0, 0.2f, 0.05f);
    Color tint = m.suddenDeath ? Color{ 255, 150, 150, 255 } : WHITE;
    Mat(0.28f, 0.0f, { 0, 0, 0, 255 }); DrawModelEx(G.plane, { 0, 0, 0 }, UP, 0, { 14.0f, 1.0f, 14.0f }, tint);

    // muri con striscia neon che si accende all'impatto
    Color wall = { 24, 32, 52, 255 }; Color neon = m.suddenDeath ? Color{ 255, 70, 90, 255 } : Color{ 48, 200, 235, 255 };
    struct W { float x, z, sx, sz; int i; } ws[4] = { { -7.35f, 0, 0.7f, 15.4f, 0 }, { 7.35f, 0, 0.7f, 15.4f, 1 }, { 0, -7.35f, 14.0f, 0.7f, 2 }, { 0, 7.35f, 14.0f, 0.7f, 3 } };
    for (auto& w : ws) {
        float f = std::min(1.0f, m.wallFlash[w.i]); Color wc = Mixc(wall, { 120, 150, 210, 255 }, f * 0.8f);
        LCube({ w.x, 0.45f, w.z }, { w.sx, 0.9f, w.sz }, wc, 0, 0.5f, 0.25f);
    }
    for (auto& w : ws) {
        float f = std::min(1.0f, m.wallFlash[w.i]); float pulse = m.suddenDeath ? 0.5f + 0.5f * sinf(t * 8.0f) : 0.0f;
        Color sc = Mixc(neon, WHITE, std::max(f, pulse * 0.4f)); float th = 0.07f + f * 0.10f;
        DrawCube({ w.x * 0.985f, 0.93f, w.z * 0.985f }, w.sx > w.sz ? w.sx : th, th, w.sx > w.sz ? th : w.sz, sc);
    }
    // pilastri agli angoli
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) {
        LCylY({ sx * 7.35f, 0, sz * 7.35f }, 0.55f, 1.25f, { 34, 44, 70, 255 }, 0.8f, 0.4f);
        LSphere({ sx * 7.35f, 1.35f, sz * 7.35f }, { 0.34f, 0.34f, 0.34f }, Mixc(neon, WHITE, 0.4f), 1.0f, 0.8f, WHITE);
    }
}

static void DrawDecals(const ArenaMatch& m, float t) {
    rlDisableDepthMask();
    if (!m.timeFields.empty()) Disc(0, 0, 9.0f, { 40, 80, 220, 40 }, 0.02f);
    for (const auto& h : m.hazards) {
        switch (h.type) {
            case HazardType::WEB: Disc(h.pos.x, h.pos.y, h.r, { 220, 230, 245, 70 }); Ring({ h.pos.x, 0.05f, h.pos.y }, h.r - 0.07f, h.r, { 235, 240, 250, 190 }); break;
            case HazardType::DARK_WEB: Disc(h.pos.x, h.pos.y, h.r, { 90, 38, 120, 120 }); Ring({ h.pos.x, 0.05f, h.pos.y }, h.r - 0.07f, h.r, { 180, 90, 230, 200 }); break;
            case HazardType::SHACKLE: Ring({ h.pos.x, 0.06f, h.pos.y }, h.r - 0.10f, h.r, { 255, 55, 115, 255 }); Disc(h.pos.x, h.pos.y, h.r, { 255, 55, 115, 36 }); break;
            case HazardType::FROST: Disc(h.pos.x, h.pos.y, h.r, { 125, 211, 252, 90 }); Ring({ h.pos.x, 0.05f, h.pos.y }, h.r - 0.07f, h.r, { 200, 240, 255, 200 }); break;
            case HazardType::ACID: case HazardType::POTION_POISON: Disc(h.pos.x, h.pos.y, h.r, { 132, 204, 22, 120 }); Ring({ h.pos.x, 0.05f, h.pos.y }, h.r - 0.07f, h.r, { 190, 240, 60, 200 }); break;
            case HazardType::POTION_HEAL: Disc(h.pos.x, h.pos.y, h.r, { 46, 229, 121, 105 }); Ring({ h.pos.x, 0.05f, h.pos.y }, h.r - 0.07f, h.r, { 130, 255, 180, 200 }); break;
            case HazardType::BOMB: Ring({ h.pos.x, 0.06f, h.pos.y }, 3.30f, 3.40f, { 255, 72, 102, 100 }); break;
            case HazardType::RAIL: DrawCube({ h.pos.x, 0.05f, h.pos.y }, h.w, 0.08f, h.h, h.val ? Color{ 56, 189, 248, 140 } : Color{ 245, 158, 11, 140 }); break;
            default: break;
        }
    }
    rlEnableDepthMask();
    (void)t;
}

static void DrawSolids(const ArenaMatch& m, float t) {
    for (const auto& bh : m.blackHoles) { (void)bh; }
    for (const auto& tw : m.tempWalls) {
        Vector3 cp = { tw.rect.x + tw.rect.width * 0.5f, 0.65f, tw.rect.y + tw.rect.height * 0.5f };
        LCube(cp, { tw.rect.width, 1.3f, tw.rect.height }, { tw.color.r, tw.color.g, tw.color.b, 200 }, 0, 0.8f, 0.9f);
    }
    for (const auto& h : m.hazards) {
        switch (h.type) {
            case HazardType::DICE: case HazardType::WDC_CELL: LCube({ h.pos.x, 0.38f, h.pos.y }, { 0.75f, 0.75f, 0.75f }, h.type == HazardType::WDC_CELL ? Color{ 234, 179, 8, 255 } : Color{ 245, 245, 250, 255 }, t * 40.0f, 0.9f, 0.4f); break;
            case HazardType::BOMB: { bool flash = h.life < 0.45f && ((int)(t * 18.0f) & 1); LSphere({ h.pos.x, 0.48f, h.pos.y }, { 0.48f, 0.48f, 0.48f }, h.val ? Color{ 96, 165, 250, 255 } : (flash ? Color{ 255, 70, 70, 255 } : Color{ 45, 55, 72, 255 }), 1.0f, 0.5f); break; }
            case HazardType::CACTUS: case HazardType::SPIKE: LConeY({ h.pos.x, 0, h.pos.y }, 0.38f, 0.95f, h.type == HazardType::CACTUS ? Color{ 120, 200, 70, 255 } : Color{ 160, 172, 190, 255 }); break;
            case HazardType::POISON_SPIKE: LConeY({ h.pos.x, 0, h.pos.y }, 0.38f, 0.95f, { 190, 110, 255, 255 }); break;
            case HazardType::SLIME_BLOB: LSphere({ h.pos.x, 0.32f, h.pos.y }, { h.r, h.r * 0.7f, h.r }, { 52, 211, 153, 215 }, 1.0f, 0.9f, { 200, 255, 230, 255 }); break;
            case HazardType::WEB: case HazardType::DARK_WEB: case HazardType::SHACKLE: case HazardType::FROST: case HazardType::ACID:
            case HazardType::POTION_POISON: case HazardType::POTION_HEAL: case HazardType::RAIL: break;
            default: LCube({ h.pos.x, 0.45f, h.pos.y }, { 0.75f, 0.9f, 0.75f }, { 107, 114, 128, 255 }, 0, 0.5f, 0.2f); break;
        }
    }
}

static void DrawFightersScene(const ArenaMatch& m, const Camera3D& cam, float t) {
    for (const auto& f : m.fighters) {
        if (f.hp <= 0) continue;
        if (f.def->id == "snake" || (f.fusedDef && f.fusedDef->id == "snake"))
            for (size_t i = 3; i < f.tail.size(); i += 2) LSphere({ f.tail[i].x, 0.45f, f.tail[i].y }, { 0.42f, 0.42f, 0.42f }, { 16, 185, 129, 255 }, 0.9f, 0.5f, { 150, 255, 210, 255 });
        Color tc = (f.team == 0) ? Color{ 46, 229, 121, 255 } : Color{ 255, 72, 102, 255 };
        if (f.def->isDummy) {   // bersaglio a terra
            rlDisableDepthMask();
            for (int k = 0; k < 3; k++) Ring({ f.pos.x, 0.035f + k * 0.004f, f.pos.y }, f.r * (1.45f + k * 0.55f) - 0.10f, f.r * (1.45f + k * 0.55f), (k & 1) ? Color{ 255, 255, 255, 190 } : Color{ 255, 72, 102, 220 });
            rlEnableDepthMask();
        }
        DrawBallBody(cam, f.def, f.fusedDef, f.pos, f.r, f.angle, f.look, &tc, f.sqX, f.sqY, f.hitFlash, f.blades, f.shield, t);
    }
}

static void DrawFx(const ArenaMatch& m, const Camera3D& cam, float t, const AimInfo* aim) {
    rlDisableDepthMask(); BeginBlendMode(BLEND_ADDITIVE);
    // scie dietro le palle
    for (const auto& f : m.fighters) {
        if (f.hp <= 0 || f.isMinion) continue;
        for (size_t i = 0; i < f.trail.size(); i++) {
            float k = (float)(i + 1) / (float)f.trail.size();
            GlowBB(cam, { f.trail[i].x, f.r * 0.9f, f.trail[i].y }, f.r * (0.9f + 1.0f * k), Alpha(f.def->color, 0.10f + 0.20f * k));
        }
    }
    for (const auto& bh : m.blackHoles) {
        float pulse = 1.0f + 0.08f * sinf(t * 10.0f);
        Halo(bh.pos.x, bh.pos.y, bh.rad * 1.1f, { 120, 50, 200, 120 }, 0.05f);
        for (int k = 0; k < 3; k++) {
            float a0 = t * (2.4f + k * 0.8f) + k * 2.1f, rr = bh.rad * (0.45f + 0.22f * k) * pulse;
            Ring({ bh.pos.x, 0.12f + k * 0.03f, bh.pos.y }, rr - 0.12f, rr, { 200, 130, 255, 220 }, a0, a0 + 4.2f);
        }
    }
    for (const auto& sw : m.shockwaves) {
        float k = sw.life / sw.maxLife; Color c = sw.color; c.a = (unsigned char)(230 * k);
        Ring({ sw.pos.x, 0.14f, sw.pos.y }, std::max(0.0f, sw.r - 0.22f), sw.r, c);
        Halo(sw.pos.x, sw.pos.y, sw.r, Alpha(sw.color, 0.16f * k), 0.05f);
    }
    for (const auto& l : m.lasers) {
        float k = std::min(1.0f, l.life * 5.0f);
        DrawCylinderEx({ l.p1.x, 0.7f, l.p1.y }, { l.p2.x, 0.7f, l.p2.y }, 0.30f, 0.30f, 10, Alpha(l.color, 0.40f * k));
        DrawCylinderEx({ l.p1.x, 0.7f, l.p1.y }, { l.p2.x, 0.7f, l.p2.y }, 0.14f, 0.14f, 10, Alpha(Mixc(l.color, WHITE, 0.5f), 0.9f * k));
        DrawCylinderEx({ l.p1.x, 0.7f, l.p1.y }, { l.p2.x, 0.7f, l.p2.y }, 0.06f, 0.06f, 8, Alpha(WHITE, k));
    }
    for (const auto& cb : m.contBeams) {
        if (cb.ownerIdx < 0 || cb.ownerIdx >= (int)m.fighters.size() || m.fighters[cb.ownerIdx].hp <= 0) continue;
        auto pts = m.ComputeBounceRay(m.fighters[cb.ownerIdx].pos, cb.track ? m.fighters[cb.ownerIdx].look : cb.spinAngle, cb.bounces);
        float pr = 0.28f + sinf(t * 24.0f) * 0.06f;
        for (size_t s = 0; s + 1 < pts.size(); s++) {
            Vector3 a = { pts[s].x, 0.75f, pts[s].y }, b = { pts[s + 1].x, 0.75f, pts[s + 1].y };
            DrawCylinderEx(a, b, pr + 0.20f, pr + 0.20f, 10, Alpha(cb.color, 0.28f)); DrawCylinderEx(a, b, pr, pr, 10, Alpha(cb.color, 0.75f));
            DrawCylinderEx(a, b, 0.10f, 0.10f, 8, WHITE);
        }
    }
    for (const auto& p : m.projectiles) {
        Vector3 c = { p.pos.x, 0.75f, p.pos.y };
        GlowBB(cam, c, p.r * 7.0f, Alpha(p.color, 0.55f)); FSphere(c, p.r * 0.85f, Mixc(p.color, WHITE, 0.6f));
    }
    for (const auto& pt : m.particles) {
        float k = std::max(0.0f, pt.life / pt.maxLife);
        GlowBB(cam, { pt.pos.x, pt.y, pt.pos.y }, pt.r * 7.0f * (0.4f + 0.6f * k), Alpha(pt.color, 0.85f * k));
    }
    if (aim) {
        auto pts = m.ComputeBounceRay(aim->from, aim->angle, 1);
        float dist = 0.9f; float pulse = 0.5f + 0.5f * sinf(aim->time * 6.0f);
        for (size_t s = 0; s + 1 < pts.size(); s++) {
            Vector2 d = { pts[s + 1].x - pts[s].x, pts[s + 1].y - pts[s].y }; float len = sqrtf(d.x * d.x + d.y * d.y); if (len < 0.01f) continue;
            for (float u = dist; u < len; u += 0.62f) {
                float k = 1.0f - std::min(1.0f, (u + (float)s * 8.0f) / 14.0f); Vector3 c = { pts[s].x + d.x / len * u, 0.55f, pts[s].y + d.y / len * u };
                float ph = fmodf(u * 1.3f - aim->time * 5.0f, 3.0f); float bump = ph < 1.0f ? (1.0f - ph) * 0.12f : 0.0f;
                GlowBB(cam, c, 0.55f + bump * 3.0f, Alpha({ 120, 235, 255, 255 }, 0.30f + 0.65f * k));
                FSphere(c, 0.075f + bump, Alpha(WHITE, 0.4f + 0.6f * k));
            }
            dist = 0.0f;
        }
        Ring({ aim->from.x, 0.06f, aim->from.y }, 1.45f, 1.55f + 0.06f * pulse, Alpha({ 120, 235, 255, 255 }, 0.7f));
    }
    EndBlendMode(); rlEnableDepthMask();
}

void RenderArena3D(const ArenaMatch& m, const Camera3D& cam, float t, const AimInfo* aim) {
    SceneBegin(cam);
    BeginMode3D(cam);
    DrawStatic(m, t);
    DrawDecals(m, t);
    DrawSolids(m, t);
    DrawFightersScene(m, cam, t);
    DrawFx(m, cam, t, aim);
    EndMode3D();
}

void RenderArenaOverlay(const ArenaMatch& m, const Camera3D& cam, const UI& ui) {
    for (const auto& f : m.fighters) {
        if (f.hp <= 0) continue;
        Vector2 sp = GetWorldToScreen({ f.pos.x, f.r * f.sqY, f.pos.y }, cam), sp2 = GetWorldToScreen({ f.pos.x + f.r, f.r * f.sqY, f.pos.y }, cam);
        float pr = std::fabs(sp2.x - sp.x) / ui.scale; float fs = std::max(13.0f, std::min(44.0f, pr * (f.isMinion ? 0.95f : 1.05f)));
        char ht[16]; std::snprintf(ht, sizeof(ht), "%d", f.hp);
        ui.TextOut(ht, sp.x / ui.scale, sp.y / ui.scale, fs, WHITE, std::max(1.2f, fs * 0.07f));
    }
    for (const auto& tx : m.texts) {
        Vector2 sp = GetWorldToScreen({ tx.pos.x, tx.y, tx.pos.y }, cam);
        Color c = tx.color; c.a = (unsigned char)(255.0f * std::max(0.0f, std::min(1.0f, tx.life * 2.5f)));
        ui.TextOut(tx.text.c_str(), sp.x / ui.scale, sp.y / ui.scale, 22.0f, c, 1.6f);
    }
}

// ============================================================================
//  Sprite/anteprima di una palla (carte, scelta, studio AI)
// ============================================================================
void RenderBallSprite(const BallDef* def, RenderTexture2D rt, float angle, bool withBackground) {
    if (!def || rt.id == 0 || !G.ready) return;
    Camera3D c{}; c.position = { 0.0f, 4.7f, 6.8f }; c.target = { 0.0f, 0.55f, 0.0f }; c.up = { 0, 1, 0 }; c.fovy = 45.0f; c.projection = CAMERA_PERSPECTIVE;
    SceneBegin(c);
    BeginTextureMode(rt);
    ClearBackground(withBackground ? Color{ 14, 20, 38, 255 } : BLANK);
    BeginMode3D(c);
    Color rc = RarityColor(def->rarity);
    rlDisableDepthMask(); BeginBlendMode(BLEND_ADDITIVE);
    Halo(0, 0, 3.4f, Alpha(rc, 0.30f), 0.01f); Halo(0, 0, 2.0f, Alpha(def->color, 0.30f), 0.012f);
    EndBlendMode(); rlEnableDepthMask();
    float r = def->baseRadius; float look = angle * 0.5f;
    int blades = (def->visualStyle == "blade") ? 2 : 0;
    DrawBallBody(c, def, nullptr, { 0.0f, 0.0f }, r, angle, look, &rc, 1, 1, 0, blades, 0, angle * 0.4f);
    EndMode3D();
    EndTextureMode();
}

void DrawSprite(RenderTexture2D rt, Rectangle dst, Color tint) {
    if (rt.id == 0) return;
    DrawTexturePro(rt.texture, { 0, 0, (float)rt.texture.width, -(float)rt.texture.height }, dst, { 0, 0 }, 0.0f, tint);
}

// ============================================================================
//  Sfondo UI
// ============================================================================
void DrawBackdrop(const UI& ui, float t) {
    DrawRectangleGradientV(0, 0, (int)std::ceil(ui.W), (int)std::ceil(ui.H), { 9, 13, 28, 255 }, { 17, 24, 50, 255 });
    BeginBlendMode(BLEND_ADDITIVE);
    const Color cols[5] = { { 40, 90, 200, 40 }, { 120, 50, 200, 34 }, { 20, 170, 190, 30 }, { 60, 60, 190, 36 }, { 190, 60, 140, 22 } };
    for (int i = 0; i < 5; i++) {
        float px = ui.W * (0.5f + 0.42f * sinf(t * 0.07f * (i + 1) + i * 1.7f)), py = ui.H * (0.5f + 0.38f * cosf(t * 0.05f * (i + 2) + i * 2.3f));
        float s = 380.0f + 110.0f * i; Rectangle dst = { px - s * 0.5f, py - s * 0.5f, s, s };
        DrawTexturePro(ui.glow, { 0, 0, (float)ui.glow.width, (float)ui.glow.height }, dst, { 0, 0 }, 0.0f, cols[i]);
    }
    EndBlendMode();
}

void DrawVignette(const UI& ui, unsigned char alpha) {
    int w = (int)std::ceil(ui.W), h = (int)std::ceil(ui.H); int bw = (int)(ui.W * 0.16f), bh = (int)(ui.H * 0.20f);
    Color o = { 0, 0, 0, alpha }, z = { 0, 0, 0, 0 };
    DrawRectangleGradientH(0, 0, bw, h, o, z); DrawRectangleGradientH(w - bw, 0, bw, h, z, o);
    DrawRectangleGradientV(0, 0, w, bh, o, z); DrawRectangleGradientV(0, h - bh, w, bh, z, o);
}
