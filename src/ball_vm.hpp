#ifndef BALL_VM_HPP
#define BALL_VM_HPP
#include "raylib.h"
#include <string>
#include <vector>

enum class Rarity { Raro=0, SuperRaro, Epico, Mitico, Leggendario, Ultra };
enum class VmTrig { ON_START=0, EVERY, ON_FRAME, ON_WALL, ON_HIT, ON_HURT, ON_KILL, WHEN, TRIG_COUNT };
enum class VmAct {
    SET_VAR=0, ADD_STAT, DAMAGE, HEAL, STEAL_HP, STEAL_ABILITY,
    PROJ, LASER, ULTRA_BEAM, BLACK_HOLE, WALL_BARRIER, GRAVITY_FLIP,
    TIME_SCALE, TELEPORT, DASH, INVERT_VEL, STUN, SLOW, POISON,
    FREEZE_PROJ, EXPLODE, SHOCKWAVE, SPAWN_MINION, SPAWN_TURRET,
    HAZARD, PULL, PUSH, SET_POS, SET_VEL, DRAW_FX, ACT_COUNT
};

struct ScriptRule {
    VmTrig trig = VmTrig::EVERY; VmAct act = VmAct::PROJ;
    std::string cond="1", interval="1.5", amt="10", dur="3.0", cnt="1";
    std::string x="x", y="y", ang="angle", spd="12", rad="2.5", w="3.0", h="0.6";
    std::string target="nearest", strArg="dmg";
    int varIdx=0, bnc=0, repeat=1;
    bool homing=false, pierce=false, once=false, track=true, ignoreShield=false;
};

struct EvalContext {
    float hp=100, maxHp=100, hppct=100, dmg=10, speed=9, r=1.15f, x=0, y=0, vx=0, vy=0, blades=0, shield=0;
    float ehp=100, emaxHp=100, ehppct=100, ex=0, ey=0, evx=0, evy=0, er=1.15f, dist=5, angle=0;
    float time=0, bounces=0, hits=0, hurts=0, ax=-7, ay=-7, aw=14, ah=14, allies=1, enemies=1, loopI=0;
    const float* vars=nullptr;
};

float EvalExpr(const std::string& expr, const EvalContext& ctx);
const char* VmTrigName(VmTrig t);
const char* VmActName(VmAct a);
VmTrig ParseTrigName(const std::string& s);
VmAct ParseActName(const std::string& s);
const char* RarityName(Rarity r);
Color RarityColor(Rarity r);
Rarity ParseRarityName(const std::string& s);

struct ParsedBallScript {
    bool ok=false; std::string chatMsg, name; Rarity rarity=Rarity::Ultra;
    int hp=130, dmg=10; float speed=1.0f, size=1.15f;
    Color color={34,211,238,255}; std::string style="none";
    std::vector<ScriptRule> rules;
    std::vector<std::string> warnings;   // regole scartate (azione/trigger sconosciuti)
};

ParsedBallScript ParseAIScript(const std::string& raw);
std::string SerializeScriptRules(const std::vector<ScriptRule>& rules);
std::vector<ScriptRule> DeserializeScriptRules(const std::string& block);
#endif