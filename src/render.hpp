#ifndef RENDER_HPP
#define RENDER_HPP
#include "game.hpp"

struct AimInfo { Vector2 from; float angle; float time; };

void GfxInit();                                   // dopo InitWindow: shader, mesh, texture
Camera3D MakeArenaCamera(float shake);            // arena 14x14 vista dritta dall'alto
void RenderArena3D(const ArenaMatch& m, const Camera3D& cam, float t, const AimInfo* aim = nullptr);
void RenderArenaOverlay(const ArenaMatch& m, const Camera3D& cam, const UI& ui);   // numeri HP + testi danno
void RenderBallSprite(const BallDef* def, RenderTexture2D rt, float angle, bool withBackground);
void DrawSprite(RenderTexture2D rt, Rectangle dst, Color tint = WHITE);
void DrawBackdrop(const UI& ui, float t);
void DrawVignette(const UI& ui, unsigned char alpha);
#endif
