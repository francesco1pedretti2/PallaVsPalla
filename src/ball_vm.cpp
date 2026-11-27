#include "ball_vm.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

const char* RarityName(Rarity r) {
    static const char* n[6] = { "RARO", "SUPER RARO", "EPICO", "MITICO", "LEGGENDARIO", "ULTRA" };
    return n[(int)r % 6];
}
Color RarityColor(Rarity r) {
    static const Color c[6] = { {59,130,246,255}, {16,185,129,255}, {168,85,247,255}, {255,55,115,255}, {245,180,25,255}, {34,211,238,255} };
    return c[(int)r % 6];
}
Rarity ParseRarityName(const std::string& s) {
    std::string u=s; for(char& c:u) c=(char)std::toupper((unsigned char)c);
    if (u.find("SUPER")!=std::string::npos) return Rarity::SuperRaro;
    if (u.find("EPIC")!=std::string::npos) return Rarity::Epico;
    if (u.find("MITIC")!=std::string::npos||u.find("MYTH")!=std::string::npos) return Rarity::Mitico;
    if (u.find("LEGG")!=std::string::npos||u.find("LEGEND")!=std::string::npos) return Rarity::Leggendario;
    if (u.find("ULTRA")!=std::string::npos) return Rarity::Ultra;
    return Rarity::Raro;
}
const char* VmTrigName(VmTrig t) { static const char* n[8]={"ON_START","EVERY","ON_FRAME","ON_WALL","ON_HIT","ON_HURT","ON_KILL","WHEN"}; return n[(int)t%8]; }
const char* VmActName(VmAct a) {
    static const char* n[30]={"SET_VAR","ADD_STAT","DAMAGE","HEAL","STEAL_HP","STEAL_ABILITY","PROJ","LASER","ULTRA_BEAM","BLACK_HOLE","WALL_BARRIER","GRAVITY_FLIP","TIME_SCALE","TELEPORT","DASH","INVERT_VEL","STUN","SLOW","POISON","FREEZE_PROJ","EXPLODE","SHOCKWAVE","SPAWN_MINION","SPAWN_TURRET","HAZARD","PULL","PUSH","SET_POS","SET_VEL","DRAW_FX"};
    return n[(int)a%30];
}
VmTrig ParseTrigName(const std::string& s) {
    std::string u=s; for(char& c:u) c=(char)std::toupper((unsigned char)c);
    for(int i=0;i<8;i++) if(u==VmTrigName((VmTrig)i)) return (VmTrig)i;
    return (VmTrig)std::max(0, std::min(7, std::atoi(u.c_str())));
}
VmAct ParseActName(const std::string& s) {
    std::string u=s; for(char& c:u) c=(char)std::toupper((unsigned char)c);
    for(int i=0;i<30;i++) if(u==VmActName((VmAct)i)) return (VmAct)i;
    return (VmAct)std::max(0, std::min(29, std::atoi(u.c_str())));
}

struct Parser {
    const char* p; const EvalContext& ctx; int depth = 0;
    void skip() { while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') p++; }
    float parseOr() { float v=parseAnd(); while(true){skip();if(p[0]=='|'&&p[1]=='|'){p+=2;float r=parseAnd();v=(v!=0||r!=0)?1.0f:0.0f;}else break;} return v; }
    float parseAnd() { float v=parseEq(); while(true){skip();if(p[0]=='&'&&p[1]=='&'){p+=2;float r=parseEq();v=(v!=0&&r!=0)?1.0f:0.0f;}else break;} return v; }
    float parseEq() { float v=parseRel(); while(true){skip();if(p[0]=='='&&p[1]=='='){p+=2;v=(fabsf(v-parseRel())<0.001f)?1.0f:0.0f;}else if(p[0]=='!'&&p[1]=='='){p+=2;v=(fabsf(v-parseRel())>=0.001f)?1.0f:0.0f;}else break;} return v; }
    float parseRel() { float v=parseAdd(); while(true){skip();if(p[0]=='<'&&p[1]=='='){p+=2;v=(v<=parseAdd())?1.0f:0.0f;}else if(p[0]=='>'&&p[1]=='='){p+=2;v=(v>=parseAdd())?1.0f:0.0f;}else if(*p=='<'){p++;v=(v<parseAdd())?1.0f:0.0f;}else if(*p=='>'){p++;v=(v>parseAdd())?1.0f:0.0f;}else break;} return v; }
    float parseAdd() { float v=parseMul(); while(true){skip();if(*p=='+'){p++;v+=parseMul();}else if(*p=='-'){p++;v-=parseMul();}else break;} return v; }
    float parseMul() { float v=parsePow(); while(true){skip();if(*p=='*'){p++;v*=parsePow();}else if(*p=='/'){p++;float r=parsePow();v=(fabsf(r)>0.0001f)?v/r:0;}else if(*p=='%'){p++;float r=parsePow();v=(fabsf(r)>0.0001f)?fmodf(v,r):0;}else break;} return v; }
    float parsePow() { float v=parseUnary(); skip(); if(*p=='^'){p++;v=powf(v,parseUnary());} return v; }
    float parseUnary() { skip(); if(*p=='-'){p++;return -parseUnary();} if(*p=='+'){p++;return parseUnary();} if(*p=='!'){p++;return(parseUnary()==0)?1.0f:0.0f;} return parsePrimary(); }
    float lookup(const std::string& id) {
        if(id=="hp")return ctx.hp; if(id=="maxhp")return ctx.maxHp; if(id=="hppct")return ctx.hppct;
        if(id=="dmg")return ctx.dmg; if(id=="speed")return ctx.speed; if(id=="r")return ctx.r;
        if(id=="x")return ctx.x; if(id=="y")return ctx.y; if(id=="vx")return ctx.vx; if(id=="vy")return ctx.vy;
        if(id=="blades")return ctx.blades; if(id=="shield")return ctx.shield;
        if(id=="ehp")return ctx.ehp; if(id=="emaxhp")return ctx.emaxHp; if(id=="ehppct")return ctx.ehppct;
        if(id=="ex")return ctx.ex; if(id=="ey")return ctx.ey; if(id=="evx")return ctx.evx; if(id=="evy")return ctx.evy; if(id=="er")return ctx.er;
        if(id=="dist")return ctx.dist; if(id=="angle")return ctx.angle; if(id=="time")return ctx.time;
        if(id=="bounces")return ctx.bounces; if(id=="hits")return ctx.hits; if(id=="hurts")return ctx.hurts;
        if(id=="ax")return ctx.ax; if(id=="ay")return ctx.ay; if(id=="aw")return ctx.aw; if(id=="ah")return ctx.ah;
        if(id=="allies")return ctx.allies; if(id=="enemies")return ctx.enemies; if(id=="i")return ctx.loopI;
        if(id=="pi")return PI; if(id=="rand")return (float)rand()/(float)RAND_MAX;
        if(id.size()==2&&id[0]=='v'&&id[1]>='0'&&id[1]<='5'&&ctx.vars) return ctx.vars[id[1]-'0'];
        return 0.0f;
    }
    float parsePrimary() {
        skip(); if (depth>32) return 0.0f;
        if (*p=='(') { p++; depth++; float v=parseOr(); depth--; skip(); if(*p==')')p++; return v; }
        if (std::isdigit((unsigned char)*p)||*p=='.') { char* e=nullptr; float v=std::strtof(p,&e); if(e>p)p=e; return v; }
        if (std::isalpha((unsigned char)*p)||*p=='_') {
            std::string id; while(std::isalnum((unsigned char)*p)||*p=='_'){id.push_back((char)std::tolower((unsigned char)*p));p++;} skip();
            if (*p=='(') {
                p++; depth++; std::vector<float> a; skip();
                if (*p!=')') { a.push_back(parseOr()); while(true){skip();if(*p==','){p++;a.push_back(parseOr());}else break;} }
                skip(); if(*p==')')p++; depth--;
                float a0=a.size()>0?a[0]:0, a1=a.size()>1?a[1]:0, a2=a.size()>2?a[2]:0;
                if(id=="sin")return sinf(a0); if(id=="cos")return cosf(a0); if(id=="abs")return fabsf(a0);
                if(id=="sqrt")return sqrtf(std::max(0.0f,a0)); if(id=="floor")return floorf(a0); if(id=="ceil")return ceilf(a0);
                if(id=="atan2")return atan2f(a0,a1); if(id=="atan")return atanf(a0); if(id=="tan")return tanf(a0); if(id=="pow")return powf(a0,a1); if(id=="round")return roundf(a0);
                if(id=="sign")return a0>0?1.0f:(a0<0?-1.0f:0.0f); if(id=="lerp")return a0+(a1-a0)*a2; if(id=="log")return a0>0?logf(a0):0.0f;
                if(id=="rand"||id=="random"){ float u=(float)rand()/(float)RAND_MAX; return a.size()>=2?a0+(a1-a0)*u:(a.size()==1?a0*u:u); }
                if(id=="min")return std::min(a0,a1); if(id=="max")return std::max(a0,a1); if(id=="clamp")return std::max(a1,std::min(a2,a0));
                return 0.0f;
            }
            return lookup(id);
        }
        return 0.0f;
    }
};
float EvalExpr(const std::string& expr, const EvalContext& ctx) { if(expr.empty())return 0.0f; float r=Parser{expr.c_str(),ctx,0}.parseOr(); return std::isfinite(r)?r:0.0f; }

static std::string Trim(std::string s) {
    while (!s.empty() && (s.front()==' '||s.front()=='\t'||s.front()=='\r'||s.front()=='\n')) s.erase(0,1);
    while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='\r'||s.back()=='\n')) s.pop_back();
    return s;
}
static bool IdCh(char c) { return std::isalnum((unsigned char)c) || c=='_'; }

// Legge "chiave=valore chiave2="valore con spazi" ..." in modo tollerante (valori non quotati con spazi inclusi).
static std::vector<std::pair<std::string, std::string>> SplitKV(const std::string& line) {
    std::vector<std::pair<std::string, std::string>> out; size_t i=0, n=line.size();
    while (i<n) {
        while (i<n && (line[i]==' '||line[i]=='\t'||line[i]==',')) i++;
        if (i>=n) break;
        size_t ks=i; while (i<n && IdCh(line[i])) i++;
        if (i==ks || i>=n || line[i]!='=') { while (i<n && line[i]!=' ' && line[i]!='\t') i++; continue; }
        std::string key=line.substr(ks,i-ks); for(char& c:key) c=(char)std::tolower((unsigned char)c);
        i++; std::string val;
        if (i<n && (line[i]=='"'||line[i]=='\''||(unsigned char)line[i]==0xE2)) {
            char q=line[i];
            if ((unsigned char)q==0xE2) { // virgolette tipografiche UTF-8 (U+201C/U+201D)
                if (i+2<n && (unsigned char)line[i+1]==0x80 && ((unsigned char)line[i+2]==0x9C||(unsigned char)line[i+2]==0x9D)) {
                    i+=3; while (i<n && !(i+2<n && (unsigned char)line[i]==0xE2 && (unsigned char)line[i+1]==0x80 && ((unsigned char)line[i+2]==0x9D||(unsigned char)line[i+2]==0x9C))) val.push_back(line[i++]);
                    if (i<n) i+=3;
                }
            } else {
                i++; while (i<n && line[i]!=q) { if (line[i]=='\\' && i+1<n) i++; val.push_back(line[i++]); }
                if (i<n) i++;
            }
        } else {
            while (i<n) {
                if (line[i]==' '||line[i]=='\t') {
                    size_t j=i; while (j<n && (line[j]==' '||line[j]=='\t')) j++;
                    size_t k=j; while (k<n && IdCh(line[k])) k++;
                    if (j>=n || (k>j && k<n && line[k]=='=')) break;   // inizia la chiave successiva
                    val.append(line, i, j-i); i=j; continue;
                }
                val.push_back(line[i++]);
            }
        }
        out.push_back({key, Trim(val)});
    }
    return out;
}

// Nomi azione/trigger: accetta sinonimi e varianti ("shoot", "time stop", "on-damage"). Sconosciuto = false (la regola viene scartata e segnalata).
static std::string NormName(const std::string& s) {
    std::string u; for (char c : s) { if (c==' '||c=='-') u.push_back('_'); else if (std::isalnum((unsigned char)c)||c=='_') u.push_back((char)std::toupper((unsigned char)c)); }
    return u;
}
static bool TryAct(const std::string& raw, VmAct& out) {
    std::string u=NormName(raw); if (u.empty()) return false;
    for (int i=0;i<30;i++) if (u==VmActName((VmAct)i)) { out=(VmAct)i; return true; }
    static const struct { const char* a; VmAct v; } al[] = {
        {"PROJECTILE",VmAct::PROJ},{"SHOOT",VmAct::PROJ},{"FIRE",VmAct::PROJ},{"BULLET",VmAct::PROJ},{"SHOOT_PROJECTILE",VmAct::PROJ},{"FIRE_PROJECTILE",VmAct::PROJ},
        {"BLACKHOLE",VmAct::BLACK_HOLE},{"GRAVITY_WELL",VmAct::BLACK_HOLE},{"BARRIER",VmAct::WALL_BARRIER},{"WALL",VmAct::WALL_BARRIER},{"SPAWN_WALL",VmAct::WALL_BARRIER},
        {"GRAVITY",VmAct::GRAVITY_FLIP},{"FLIP_GRAVITY",VmAct::GRAVITY_FLIP},{"REVERSE_GRAVITY",VmAct::GRAVITY_FLIP},
        {"TIME_STOP",VmAct::TIME_SCALE},{"STOP_TIME",VmAct::TIME_SCALE},{"FREEZE_TIME",VmAct::TIME_SCALE},{"TIMESCALE",VmAct::TIME_SCALE},{"SLOW_TIME",VmAct::TIME_SCALE},{"TIME",VmAct::TIME_SCALE},
        {"WARP",VmAct::TELEPORT},{"BLINK",VmAct::TELEPORT},{"CHARGE",VmAct::DASH},{"BOOST",VmAct::DASH},{"LUNGE",VmAct::DASH},
        {"REVERSE",VmAct::INVERT_VEL},{"REVERSE_VELOCITY",VmAct::INVERT_VEL},{"INVERT",VmAct::INVERT_VEL},{"INVERT_VELOCITY",VmAct::INVERT_VEL},
        {"FREEZE",VmAct::STUN},{"PARALYZE",VmAct::STUN},{"SLOWDOWN",VmAct::SLOW},{"DOT",VmAct::POISON},{"VENOM",VmAct::POISON},
        {"CLEAR_PROJECTILES",VmAct::FREEZE_PROJ},{"DESTROY_PROJ",VmAct::FREEZE_PROJ},{"ERASE_PROJ",VmAct::FREEZE_PROJ},{"DESTROY_PROJECTILES",VmAct::FREEZE_PROJ},
        {"BOMB",VmAct::EXPLODE},{"BLAST",VmAct::EXPLODE},{"EXPLOSION",VmAct::EXPLODE},{"WAVE",VmAct::SHOCKWAVE},
        {"MINION",VmAct::SPAWN_MINION},{"SPAWN",VmAct::SPAWN_MINION},{"CLONE",VmAct::SPAWN_MINION},{"SUMMON",VmAct::SPAWN_MINION},{"SPAWN_CLONE",VmAct::SPAWN_MINION},
        {"TURRET",VmAct::SPAWN_TURRET},{"SPAWN_CANNON",VmAct::SPAWN_TURRET},{"TRAP",VmAct::HAZARD},{"ZONE",VmAct::HAZARD},{"SPAWN_HAZARD",VmAct::HAZARD},
        {"ATTRACT",VmAct::PULL},{"GRAB",VmAct::PULL},{"HOOK",VmAct::PULL},{"REPEL",VmAct::PUSH},{"KNOCKBACK",VmAct::PUSH},
        {"MOVE_TO",VmAct::SET_POS},{"SETPOS",VmAct::SET_POS},{"SETVEL",VmAct::SET_VEL},{"SET_VELOCITY",VmAct::SET_VEL},
        {"TEXT",VmAct::DRAW_FX},{"SAY",VmAct::DRAW_FX},{"FX",VmAct::DRAW_FX},{"BEAM",VmAct::ULTRA_BEAM},{"ULTRA",VmAct::ULTRA_BEAM},{"MEGA_BEAM",VmAct::ULTRA_BEAM},
        {"LIFESTEAL",VmAct::STEAL_HP},{"DRAIN",VmAct::STEAL_HP},{"VAMPIRE",VmAct::STEAL_HP},{"COPY_ABILITY",VmAct::STEAL_ABILITY},{"STEAL_SKILL",VmAct::STEAL_ABILITY},{"COPY",VmAct::STEAL_ABILITY},
        {"LASER_BEAM",VmAct::LASER},{"RAY",VmAct::LASER},{"DEAL_DAMAGE",VmAct::DAMAGE},{"STRIKE",VmAct::DAMAGE},{"HURT",VmAct::DAMAGE},{"HEAL_SELF",VmAct::HEAL},{"REGEN",VmAct::HEAL},
        {"BUFF",VmAct::ADD_STAT},{"STAT",VmAct::ADD_STAT},{"MODIFY_STAT",VmAct::ADD_STAT},{"VAR",VmAct::SET_VAR},{"SET",VmAct::SET_VAR} };
    for (const auto& e : al) if (u==e.a) { out=e.v; return true; }
    bool digits=true; for (char c:u) if (!std::isdigit((unsigned char)c)) digits=false;
    if (digits) { int n=std::atoi(u.c_str()); if (n>=0&&n<30) { out=(VmAct)n; return true; } }
    return false;
}
static bool TryTrig(const std::string& raw, VmTrig& out) {
    std::string u=NormName(raw); if (u.empty()) return false;
    for (int i=0;i<8;i++) if (u==VmTrigName((VmTrig)i)) { out=(VmTrig)i; return true; }
    static const struct { const char* a; VmTrig v; } al[] = {
        {"START",VmTrig::ON_START},{"ROUND_START",VmTrig::ON_START},{"ON_ROUND_START",VmTrig::ON_START},{"INIT",VmTrig::ON_START},
        {"TIMER",VmTrig::EVERY},{"INTERVAL",VmTrig::EVERY},{"PERIODIC",VmTrig::EVERY},{"ON_TIMER",VmTrig::EVERY},{"REPEAT",VmTrig::EVERY},
        {"FRAME",VmTrig::ON_FRAME},{"ALWAYS",VmTrig::ON_FRAME},{"TICK",VmTrig::ON_FRAME},{"ON_TICK",VmTrig::ON_FRAME},{"ON_UPDATE",VmTrig::ON_FRAME},
        {"ON_BOUNCE",VmTrig::ON_WALL},{"WALL",VmTrig::ON_WALL},{"ON_WALL_HIT",VmTrig::ON_WALL},{"ON_BOUNCE_WALL",VmTrig::ON_WALL},{"ON_WALL_BOUNCE",VmTrig::ON_WALL},
        {"ON_COLLIDE",VmTrig::ON_HIT},{"ON_CONTACT",VmTrig::ON_HIT},{"ON_ATTACK",VmTrig::ON_HIT},{"HIT",VmTrig::ON_HIT},{"ON_COLLISION",VmTrig::ON_HIT},{"ON_ENEMY_HIT",VmTrig::ON_HIT},
        {"ON_DAMAGE",VmTrig::ON_HURT},{"ON_DAMAGED",VmTrig::ON_HURT},{"HURT",VmTrig::ON_HURT},{"ON_TAKE_DAMAGE",VmTrig::ON_HURT},
        {"KILL",VmTrig::ON_KILL},{"ON_ELIMINATE",VmTrig::ON_KILL},{"CONDITION",VmTrig::WHEN},{"ON_CONDITION",VmTrig::WHEN},{"IF",VmTrig::WHEN},{"ON_WHEN",VmTrig::WHEN} };
    for (const auto& e : al) if (u==e.a) { out=e.v; return true; }
    return false;
}

static std::string Upper(std::string s) { for (char& c:s) c=(char)std::toupper((unsigned char)c); return s; }
// Toglie decorazioni markdown/elenchi dall'inizio riga: "**NAME:** x", "- RULE: ...", "1. HP: 100"
static std::string CleanLine(std::string l) {
    l=Trim(l);
    for (;;) {
        size_t before=l.size();
        while (!l.empty() && (l[0]=='*'||l[0]=='-'||l[0]=='#'||l[0]=='>'||l[0]=='`'||l[0]==' ')) l.erase(0,1);
        if (l.size()>=3 && (unsigned char)l[0]==0xE2 && (unsigned char)l[1]==0x80 && (unsigned char)l[2]==0xA2) l.erase(0,3); // elenco puntato
        size_t d=0; while (d<l.size() && std::isdigit((unsigned char)l[d])) d++;
        if (d>0 && d<3 && d<l.size() && (l[d]=='.'||l[d]==')') && d+1<l.size() && l[d+1]==' ') l.erase(0,d+1);
        if (l.size()==before) break;
    }
    size_t c=l.find(':'); // "**NAME**: x" / "NAME**: x" -> togli gli asterischi attorno alla chiave
    if (c!=std::string::npos) { std::string k=l.substr(0,c), rest=l.substr(c); k.erase(std::remove(k.begin(),k.end(),'*'),k.end()); l=k+rest; }
    if (c!=std::string::npos) { size_t c2=l.find(':'); size_t p=c2+1; while (p<l.size() && (l[p]=='*'||l[p]==' ')) p++; l=l.substr(0,c2+1)+" "+l.substr(p); }
    return Trim(l);
}
static std::string StripQuotes(std::string v) {
    v=Trim(v); while (!v.empty() && (v.front()=='"'||v.front()=='\''||v.front()=='*')) v.erase(0,1);
    while (!v.empty() && (v.back()=='"'||v.back()=='\''||v.back()=='*')) v.pop_back();
    return Trim(v);
}
static bool KnownStyle(const std::string& s) {
    static const char* st[]={"none","blade","axe","spear","mg","orbit","cactus","lightwing","shackles","zone"};
    for (const char* x:st) if (s==x) return true; return false;
}

ParsedBallScript ParseAIScript(const std::string& raw) {
    ParsedBallScript res; std::istringstream iss(raw); std::string line;
    while (std::getline(iss, line)) {
        line=CleanLine(line); if (line.empty()) continue;
        size_t c=line.find(':'); if (c==std::string::npos) continue;
        std::string key=Upper(Trim(line.substr(0,c))), val=Trim(line.substr(c+1));
        while (!key.empty() && (std::isdigit((unsigned char)key.back())||key.back()==' ')) key.pop_back(); // "RULE 1" -> "RULE"
        if (key=="CHAT") res.chatMsg=StripQuotes(val);
        else if (key=="NAME") { res.name=StripQuotes(val); if (res.name.size()>22) { size_t k=22; while (k>0 && ((unsigned char)res.name[k]&0xC0)==0x80) k--; res.name.resize(k); } }
        else if (key=="RARITY") res.rarity=ParseRarityName(val);
        else if (key=="HP") res.hp=std::max(40,std::min(500,std::atoi(val.c_str())));
        else if (key=="DMG") res.dmg=std::max(1,std::min(60,std::atoi(val.c_str())));
        else if (key=="SPEED") res.speed=std::max(0.5f,std::min(2.4f,std::strtof(val.c_str(),nullptr)));
        else if (key=="SIZE") res.size=std::max(0.75f,std::min(1.7f,std::strtof(val.c_str(),nullptr)));
        else if (key=="STYLE") { std::string st=StripQuotes(val); for(char& ch:st) ch=(char)std::tolower((unsigned char)ch); res.style=KnownStyle(st)?st:"none"; }
        else if (key=="COLOR") {
            std::string v=val; for(char& ch:v) if(!std::isdigit((unsigned char)ch)) ch=' ';
            int r=34,g=211,b=238; std::istringstream cs(v);
            if (cs>>r>>g>>b) res.color={(unsigned char)std::max(0,std::min(255,r)),(unsigned char)std::max(0,std::min(255,g)),(unsigned char)std::max(0,std::min(255,b)),255};
        } else if (key=="RULE" && res.rules.size()<12) {
            ScriptRule r; bool badRule=false; bool haveAct=false;
            for (const auto& kv : SplitKV(val)) {
                const auto &k=kv.first, &v=kv.second;
                if (k=="trig") { VmTrig t; if (TryTrig(v,t)) r.trig=t; else { badRule=true; res.warnings.push_back("trigger sconosciuto '"+v+"'"); } }
                else if (k=="act") { VmAct a; if (TryAct(v,a)) { r.act=a; haveAct=true; } else { badRule=true; res.warnings.push_back("azione sconosciuta '"+v+"'"); } }
                else if (k=="cond") r.cond=v; else if (k=="int") r.interval=v;
                else if (k=="amt"||k=="val") r.amt=v; else if (k=="dur") r.dur=v; else if (k=="cnt") r.cnt=v;
                else if (k=="x") r.x=v; else if (k=="y") r.y=v; else if (k=="ang") r.ang=v;
                else if (k=="spd") r.spd=v; else if (k=="rad") r.rad=v; else if (k=="w") r.w=v; else if (k=="h") r.h=v;
                else if (k=="target") r.target=v; else if (k=="str") r.strArg=v;
                else if (k=="var") r.varIdx=std::max(0,std::min(5,std::atoi(v.c_str())));
                else if (k=="bnc") r.bnc=std::max(0,std::min(8,std::atoi(v.c_str())));
                else if (k=="rep") r.repeat=std::max(1,std::min(12,std::atoi(v.c_str())));
                else if (k=="home") r.homing=(std::atoi(v.c_str())!=0); else if (k=="pierce") r.pierce=(std::atoi(v.c_str())!=0);
                else if (k=="once") r.once=(std::atoi(v.c_str())!=0); else if (k=="track") r.track=(std::atoi(v.c_str())!=0);
                else if (k=="ign") r.ignoreShield=(std::atoi(v.c_str())!=0);
            }
            if (!haveAct && !badRule) { badRule=true; res.warnings.push_back("regola senza azione"); }
            if (!badRule) res.rules.push_back(r);
        }
    }
    res.ok = !res.rules.empty();
    return res;
}
static std::string Q(const std::string& s) { std::string out="\""; for(char c:s){if(c=='"')out.push_back('\'');else if(c!='\n'&&c!='\r')out.push_back(c);} out.push_back('"'); return out; }
std::string SerializeScriptRules(const std::vector<ScriptRule>& rules) {
    std::ostringstream os;
    for (const auto& r : rules) {
        os<<"RULE: trig="<<VmTrigName(r.trig)<<" act="<<VmActName(r.act)<<" cond="<<Q(r.cond)<<" int="<<Q(r.interval)
          <<" amt="<<Q(r.amt)<<" dur="<<Q(r.dur)<<" cnt="<<Q(r.cnt)<<" x="<<Q(r.x)<<" y="<<Q(r.y)<<" ang="<<Q(r.ang)
          <<" spd="<<Q(r.spd)<<" rad="<<Q(r.rad)<<" w="<<Q(r.w)<<" h="<<Q(r.h)<<" target="<<Q(r.target)<<" str="<<Q(r.strArg)
          <<" var="<<r.varIdx<<" bnc="<<r.bnc<<" rep="<<r.repeat<<" home="<<(r.homing?1:0)<<" pierce="<<(r.pierce?1:0)
          <<" once="<<(r.once?1:0)<<" track="<<(r.track?1:0)<<" ign="<<(r.ignoreShield?1:0)<<"\n";
    }
    return os.str();
}
std::vector<ScriptRule> DeserializeScriptRules(const std::string& block) { return ParseAIScript(block).rules; }