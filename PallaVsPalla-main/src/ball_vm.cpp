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
                if(id=="min")return std::min(a0,a1); if(id=="max")return std::max(a0,a1); if(id=="clamp")return std::max(a1,std::min(a2,a0));
                return 0.0f;
            }
            return lookup(id);
        }
        return 0.0f;
    }
};
float EvalExpr(const std::string& expr, const EvalContext& ctx) { if(expr.empty())return 0.0f; float r=Parser{expr.c_str(),ctx,0}.parseOr(); return std::isfinite(r)?r:0.0f; }

static std::vector<std::pair<std::string, std::string>> SplitKV(const std::string& line) {
    std::vector<std::pair<std::string, std::string>> out; size_t i=0, n=line.size();
    while (i<n) {
        while (i<n&&(line[i]==' '||line[i]=='\t')) i++; if (i>=n) break;
        size_t eq=line.find('=',i); if (eq==std::string::npos) break;
        std::string key=line.substr(i,eq-i); i=eq+1; std::string val;
        if (i<n&&(line[i]=='"'||line[i]=='\'')) {
            char q=line[i++]; while(i<n&&line[i]!=q){if(line[i]=='\\'&&i+1<n)i++; val.push_back(line[i++]);}
            if(i<n&&line[i]==q)i++;
        } else while(i<n&&line[i]!=' '&&line[i]!='\t') val.push_back(line[i++]);
        out.push_back({key, val});
    }
    return out;
}
static std::string Trim(std::string s) {
    while (!s.empty()&&(s.front()==' '||s.front()=='\t')) s.erase(0,1);
    while (!s.empty()&&(s.back()==' '||s.back()=='\t'||s.back()=='\r')) s.pop_back();
    return s;
}

ParsedBallScript ParseAIScript(const std::string& raw) {
    ParsedBallScript res; std::istringstream iss(raw); std::string line;
    while (std::getline(iss, line)) {
        line=Trim(line); if (line.empty()) continue;
        if (line.rfind("CHAT:",0)==0) res.chatMsg=Trim(line.substr(5));
        else if (line.rfind("NAME:",0)==0) res.name=Trim(line.substr(5)).substr(0,22);
        else if (line.rfind("RARITY:",0)==0) res.rarity=ParseRarityName(line.substr(7));
        else if (line.rfind("HP:",0)==0) res.hp=std::max(40,std::min(999,std::atoi(line.substr(3).c_str())));
        else if (line.rfind("DMG:",0)==0) res.dmg=std::max(1,std::min(150,std::atoi(line.substr(4).c_str())));
        else if (line.rfind("SPEED:",0)==0) res.speed=std::max(0.5f,std::min(2.6f,std::strtof(line.substr(6).c_str(),nullptr)));
        else if (line.rfind("SIZE:",0)==0) res.size=std::max(0.75f,std::min(1.8f,std::strtof(line.substr(5).c_str(),nullptr)));
        else if (line.rfind("STYLE:",0)==0) res.style=Trim(line.substr(6));
        else if (line.rfind("COLOR:",0)==0) {
            std::string v=line.substr(6); for(char& c:v)if(c==',')c=' '; int r=34,g=211,b=238; std::istringstream cs(v);
            if (cs>>r>>g>>b) res.color={(unsigned char)std::max(0,std::min(255,r)),(unsigned char)std::max(0,std::min(255,g)),(unsigned char)std::max(0,std::min(255,b)),255};
        } else if (line.rfind("RULE:",0)==0) {
            ScriptRule r;
            for (const auto& kv : SplitKV(line.substr(5))) {
                const auto &k=kv.first, &v=kv.second;
                if (k=="trig") r.trig=ParseTrigName(v); else if (k=="act") r.act=ParseActName(v);
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
            res.rules.push_back(r);
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