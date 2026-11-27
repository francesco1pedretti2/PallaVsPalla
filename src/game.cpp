#include "game.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <sstream>
#ifndef PI
#define PI 3.14159265358979323846f
#endif

std::deque<BallDef>& GetBallCatalog() {
    static std::deque<BallDef> db; static bool built = false;
    if (!built) {
        built = true;
        auto add = [&](const char* id, const char* nm, Rarity rar, Color col, int hp, int dmg, const char* desc) {
            BallDef b; b.id=id; b.name=nm; b.rarity=rar; b.color=col; b.hp=hp; b.dmg=dmg; b.desc=desc; db.push_back(b);
        };
        // 1. RARO
        add("blade","Palla Lama",Rarity::Raro,{56,189,248,255},110,7,"Lame 3D orbitano attorno a te; nuove lame ogni 4s.");
        add("vampire","Palla Vampiro",Rarity::Raro,{210,38,56,255},105,9,"Zanne assetate: ti curi del 55% del danno inflitto.");
        add("spider","Palla Ragno",Rarity::Raro,{95,110,130,255},108,7,"Sui muri piazza ragnatele che rallentano e feriscono.");
        add("mg","Mitragliatrice",Rarity::Raro,{40,195,95,255},98,5,"Torretta 3D rotante: spara raffiche verso il nemico.");
        add("zone","Palla Zona",Rarity::Raro,{139,92,246,255},115,6,"Aura viola che brucia costantemente i nemici vicini.");
        add("electric","Palla Elettrica",Rarity::Raro,{250,204,21,255},104,7,"Gli impatti folgorano e bloccano il nemico sul posto.");
        add("cell","Palla Cellula",Rarity::Raro,{74,222,128,255},112,6,"Cresce col tempo e si divide in 2 mini-cellule.");
        add("charge","Palla Carica",Rarity::Raro,{244,63,94,255},108,4,"Più aspetti tra i colpi, più il prossimo urto esplode!");
        add("burst","Palla Scatto",Rarity::Raro,{251,146,60,255},102,7,"Scatta a doppia velocità potenziando l'urto.");
        // 2. SUPER RARO
        add("bow","Palla Arco",Rarity::SuperRaro,{163,230,53,255},100,7,"Scocca frecce veloci che rimbalzano sui muri.");
        add("bigspike","Grandi Spuntoni",Rarity::SuperRaro,{148,163,184,255},112,6,"Ogni rimbalzo pianta grossi spuntoni lungo le pareti.");
        add("electroking","Re Elettrico",Rarity::SuperRaro,{56,189,248,255},108,7,"Sui muri genera griglie elettriche ad alto voltaggio.");
        add("hook","Palla Uncino",Rarity::SuperRaro,{192,132,252,255},105,7,"Lancia un uncino che strattona verso di te il nemico.");
        add("range","Palla Campo",Rarity::SuperRaro,{45,212,191,255},106,6,"Onda d'urto espansiva che colpisce a medio raggio.");
        add("frost","Palla Gelo",Rarity::SuperRaro,{125,211,252,255},106,6,"Lascia scie glaciali che congelano e rallentano.");
        add("acid","Palla Acido",Rarity::SuperRaro,{132,204,22,255},104,6,"Sputa pozze d'acido corrosive sul pavimento 60x60.");
        add("ray","Palla Raggio",Rarity::SuperRaro,{248,113,113,255},100,6,"Piazza torrette rapide che sparano laser continui.");
        add("slime","Palla Slime",Rarity::SuperRaro,{52,211,153,255},114,6,"Lascia slime rimbalzanti che rallentano i nemici.");
        // 3. EPICO
        add("axe","Palla Ascia",Rarity::Epico,{249,115,22,255},110,8,"Ascia 3D rotante potente; gira 2x veloce sotto 50% HP!");
        add("thief","Palla Ladro",Rarity::Epico,{75,85,105,255},98,6,"Lancia pugnali mirati; raddoppia cadenza sotto 50% HP.");
        add("laser","Palla Laser",Rarity::Epico,{6,182,212,255},105,7,"Ogni rimbalzo traccia un raggio laser 3D nell'arena.");
        add("dice","Palla Dado",Rarity::Epico,{215,225,240,255},108,7,"Lascia dadi (1-6) che esplodono al contatto.");
        add("snake","Palla Serpente",Rarity::Epico,{16,185,129,255},108,8,"Fa crescere una coda 3D articolata che ferisce i nemici.");
        add("bomb","Palla Bomba",Rarity::Epico,{55,65,85,255},114,8,"Piazza bombe a miccia ad ampio raggio ad alto danno.");
        add("shuriken","Palla Shuriken",Rarity::Epico,{99,102,241,255},102,6,"Scaglia shuriken affilati che rimbalzano sulle pareti.");
        add("cactus","Palla Cactus",Rarity::Epico,{101,163,13,255},118,7,"Semina trappole spinose e riflette danni al contatto.");
        add("poisonspike","Spina Veleno",Rarity::Epico,{168,85,247,255},106,6,"Piazza spine velenose sui muri con danno nel tempo.");
        // 4. MITICO
        add("icecone","Cono Ghiaccio",Rarity::Mitico,{96,165,250,255},108,7,"Sgancia bombe glaciali che esplodono in 6 schegge.");
        add("cannon","Palla Cannone",Rarity::Mitico,{71,85,105,255},112,7,"Sui muri piazza cannoni 3D automatici a palle pesanti.");
        add("virus","Palla Virus",Rarity::Mitico,{192,38,211,255},108,7,"Contatto infettivo: applica un virus velenoso.");
        add("apple","Palla Mela",Rarity::Mitico,{239,68,68,255},112,7,"Lancia mele: feriscono i nemici e ti curano.");
        add("wdc","Palla WDC",Rarity::Mitico,{234,179,8,255},106,6,"Evoca celle numerate (1-9) con danno casuale.");
        add("spear","Palla Lancia",Rarity::Mitico,{226,232,240,255},108,8,"Lunga lancia 3D rotante che trafigge e respinge.");
        add("reforge","Palla Forgia",Rarity::Mitico,{217,119,6,255},112,7,"Sui muri potenzia un martello orbitale sempre più forte.");
        add("conductor","Conduttore",Rarity::Mitico,{245,158,11,255},112,8,"Traccia binari ad alta tensione che folgorano l'arena.");
        add("shackles","Palla Catene",Rarity::Mitico,{255,50,105,255},124,9,"Evoca trappole di catene che STORDISCONO e schiacciano!");
        // 5. LEGGENDARIO
        add("fibonacci","Palla Fibonacci",Rarity::Leggendario,{234,179,8,255},108,5,"Il danno scala con Fibonacci (1, 2, 3, 5, 8, 13, 21...).");
        add("glass","Palla Vetro",Rarity::Leggendario,{125,211,252,255},95,8,"Ogni impatto scaglia 6 schegge di vetro perforanti.");
        add("orbit","Palla Orbitale",Rarity::Leggendario,{192,132,252,255},116,7,"3 lune 3D orbitali ad ampio raggio travolgono l'arena.");
        add("chess","Palla Scacchi",Rarity::Leggendario,{241,245,249,255},110,9,"Scatta a croce o a L come Torre e Cavallo.");
        add("beam","Palla Fascio",Rarity::Leggendario,{34,211,238,255},104,6,"Carica e spara un raggio laser mirato potente.");
        add("harpoon","Palla Arpione",Rarity::Leggendario,{14,165,233,255},114,8,"Arpiona i nemici trascinandoli contro di te.");
        add("hive","Palla Alveare",Rarity::Leggendario,{250,204,21,255},110,6,"Rilascia api velenose a ricerca automatica.");
        // 6. ULTRA
        add("verity","Palla Verità",Rarity::Ultra,{244,114,182,255},106,6,"Subendo danni ha il 45% di salire di Livello (+HP/+DMG).");
        add("potion","Palla Pozione",Rarity::Ultra,{167,139,250,255},108,7,"Lancia pozioni casuali di Veleno, Cura o Esplosione.");
        add("blackspider","Ragno Nero",Rarity::Ultra,{45,35,65,255},120,8,"Ragnatele oscure: feriscono i nemici e TI CURANO!");
        add("grave","Palla Tomba",Rarity::Ultra,{107,114,128,255},122,8,"Pianta lapidi 3D che evocano fantasmi a ricerca.");
        add("lightwing","Ali di Luce",Rarity::Ultra,{253,224,71,255},116,8,"Ali 3D angeliche: vola veloce e lancia ventagli di luce.");
    }
    return db;
}

const BallDef* FindBallById(const std::string& id) { for (const auto& b : GetBallCatalog()) if (b.id == id) return &b; return &GetBallCatalog()[0]; }
static float RandF(float a, float b) { return a + ((float)rand() / (float)RAND_MAX) * (b - a); }
[[maybe_unused]] static Color Shade(Color c, float f) { auto cl=[](float v)->unsigned char{return(unsigned char)(v<0?0:(v>255?255:v));}; return {cl(c.r*f),cl(c.g*f),cl(c.b*f),c.a}; }
static float PtSegDist(Vector2 p, Vector2 a, Vector2 b) {
    float dx=b.x-a.x, dy=b.y-a.y, l2=dx*dx+dy*dy; if (l2<=0.0001f) return hypotf(p.x-a.x,p.y-a.y);
    float t=std::max(0.0f,std::min(1.0f,((p.x-a.x)*dx+(p.y-a.y)*dy)/l2));
    return hypotf(p.x-(a.x+t*dx),p.y-(a.y+t*dy));
}

Fighter ArenaMatch::MakeFighter(const BallDef* def, const BallDef* fused, int team, float x, float y, float ang, bool minion) {
    Fighter f; f.def=def; f.fusedDef=fused; f.team=team; f.isMinion=minion; f.pos={x,y};
    f.speedMult=fused?((def->speedMult+fused->speedMult)*0.5f):def->speedMult;
    float spd=(minion?10.5f:9.5f)*f.speedMult; f.vel={cosf(ang)*spd,sinf(ang)*spd}; f.r=minion?0.58f:def->baseRadius;
    int baseHp=fused?(int)roundf((def->hp+fused->hp)*0.65f):def->hp; if (minion) baseHp=28;
    f.maxHp=(int)(baseHp*((ranked&&team==1)?1.12f:1.0f)); f.hp=f.maxHp;
    f.blades=(def->id=="blade"||def->visualStyle=="blade"||(fused&&(fused->id=="blade"||fused->visualStyle=="blade")))?2:0;
    f.timerA=RandF(0,0.35f); f.timerB=RandF(0,0.35f); f.look=ang;
    f.runtimeRules=def->scriptRules;
    if (fused) f.runtimeRules.insert(f.runtimeRules.end(), fused->scriptRules.begin(), fused->scriptRules.end());
    f.ruleTimers.assign(f.runtimeRules.size(), 0.0f); f.ruleOnce.assign(f.runtimeRules.size(), false); f.rulePrevWhen.assign(f.runtimeRules.size(), false);
    return f;
}

void ArenaMatch::Init(const std::vector<std::pair<const BallDef*, const BallDef*>>& allies, const std::vector<std::pair<const BallDef*, const BallDef*>>& enemies, bool isDemo, bool isRanked, float allyAimAng, int allyUpgHp, int allyUpgDmg, float allyUpgSpd, float allyRegen) {
    bounds={-7.0f,-7.0f,14.0f,14.0f}; demo=isDemo; ranked=isRanked; time=0; hitstop=0; shake=0; suddenDeath=false; ended=false; winnerTeam=-1;
    for (float& wf : wallFlash) wf=0;
    fighters.clear(); fighters.reserve(64); /* niente riallocazioni: i Fighter& restano validi quando nascono minion o cellule */ projectiles.clear(); hazards.clear(); lasers.clear(); contBeams.clear();
    blackHoles.clear(); tempWalls.clear(); timeFields.clear(); gravFields.clear();
    particles.clear(); shockwaves.clear(); texts.clear(); cooldowns.clear();
    for (size_t i=0;i<allies.size();i++) {
        float yy = allies.size()>1 ? (-2.8f + 5.6f * (float)i) : 0.0f;
        Fighter f=MakeFighter(allies[i].first,allies[i].second,0,-3.8f,yy,(i==0&&!isDemo)?allyAimAng:RandF(-0.8f,0.8f),false);
        f.maxHp+=allyUpgHp; f.hp=f.maxHp; f.bonusDmg=allyUpgDmg; f.speedMult*=allyUpgSpd; f.regen=allyRegen; f.vel.x*=allyUpgSpd; f.vel.y*=allyUpgSpd; fighters.push_back(f);
    }
    for (size_t i=0;i<enemies.size();i++) {
        float yy = enemies.size()>1 ? (-2.8f + 5.6f * (float)i) : 0.0f;
        fighters.push_back(MakeFighter(enemies[i].first,enemies[i].second,1,3.8f,yy,RandF(PI-0.8f,PI+0.8f),false));
    }
}

int ArenaMatch::NearestEnemy(int idx) const {
    int best=-1; float bestD=1e9f; const auto& f=fighters[idx];
    for (int i=0;i<(int)fighters.size();i++) if (fighters[i].team!=f.team && fighters[i].hp>0) {
        float d=hypotf(fighters[i].pos.x-f.pos.x,fighters[i].pos.y-f.pos.y); if (d<bestD) { bestD=d; best=i; }
    }
    return best;
}

EvalContext ArenaMatch::MakeCtx(int idx, int enemyIdx, float loopI) const {
    EvalContext ctx{}; const auto& f=fighters[idx];
    ctx.hp=(float)f.hp; ctx.maxHp=(float)f.maxHp; ctx.hppct=100.0f*(float)f.hp/std::max(1,f.maxHp);
    ctx.dmg=(float)(f.def->dmg+f.bonusDmg); ctx.speed=hypotf(f.vel.x,f.vel.y); ctx.r=f.r;
    ctx.x=f.pos.x; ctx.y=f.pos.y; ctx.vx=f.vel.x; ctx.vy=f.vel.y; ctx.blades=(float)f.blades; ctx.shield=(float)f.shield;
    ctx.time=time; ctx.bounces=(float)f.bounceCount; ctx.hits=(float)f.hitCount; ctx.hurts=(float)f.hurtCount;
    ctx.ax=bounds.x; ctx.ay=bounds.y; ctx.aw=bounds.width; ctx.ah=bounds.height; ctx.loopI=loopI; ctx.vars=f.vars;
    int al=0, en=0; for (const auto& ft:fighters) if (ft.hp>0) { if (ft.team==f.team) al++; else en++; }
    ctx.allies=(float)al; ctx.enemies=(float)en;
    int ei = (enemyIdx>=0&&enemyIdx<(int)fighters.size()) ? enemyIdx : NearestEnemy(idx);
    if (ei>=0) {
        const auto& e=fighters[ei];
        ctx.ehp=(float)e.hp; ctx.emaxHp=(float)e.maxHp; ctx.ehppct=100.0f*(float)e.hp/std::max(1,e.maxHp);
        ctx.ex=e.pos.x; ctx.ey=e.pos.y; ctx.evx=e.vel.x; ctx.evy=e.vel.y; ctx.er=e.r;
        ctx.dist=hypotf(e.pos.x-f.pos.x,e.pos.y-f.pos.y); ctx.angle=atan2f(e.pos.y-f.pos.y,e.pos.x-f.pos.x);
    } else { ctx.dist=5; ctx.angle=f.look; }
    return ctx;
}

void ArenaMatch::SpawnParticles(Vector2 p, Color c, int count, float speed) {
    for (int i=0;i<count;i++) { float a=RandF(0,2*PI), s=RandF(1.5f,speed); particles.push_back({p,{cosf(a)*s,sinf(a)*s},0.7f,RandF(2.5f,6.0f),0.45f,0.45f,c,RandF(0.10f,0.22f)}); }
}
void ArenaMatch::AddText(Vector2 p, const std::string& txt, Color c) { if (!demo) texts.push_back({p,1.8f,txt,c,0.85f}); }
bool ArenaMatch::CooldownReady(int a, int b, int slot, float minGap) {
    for (auto& cd : cooldowns) if (cd.a==a&&cd.b==b&&cd.slot==slot) { if (time-cd.lastTime>minGap) { cd.lastTime=time; return true; } return false; }
    cooldowns.push_back({a,b,slot,time}); return true;
}

std::vector<Vector2> ArenaMatch::ComputeBounceRay(Vector2 start, float angle, int bounces) const {
    std::vector<Vector2> pts; pts.push_back(start);
    Vector2 cur=start, dir={cosf(angle),sinf(angle)};
    float minX=bounds.x+0.05f, maxX=bounds.x+bounds.width-0.05f, minY=bounds.y+0.05f, maxY=bounds.y+bounds.height-0.05f;
    for (int b=0;b<=std::min(6,std::max(0,bounces));b++) {
        float bestT=1e9f; bool hitX=false;
        if (fabsf(dir.x)>0.0001f) { float tx=((dir.x>0?maxX:minX)-cur.x)/dir.x; if (tx>0.02f&&tx<bestT) { bestT=tx; hitX=true; } }
        if (fabsf(dir.y)>0.0001f) { float ty=((dir.y>0?maxY:minY)-cur.y)/dir.y; if (ty>0.02f&&ty<bestT) { bestT=ty; hitX=false; } }
        if (bestT>1e8f) break;
        cur={cur.x+dir.x*bestT,cur.y+dir.y*bestT}; pts.push_back(cur);
        if (hitX) dir.x*=-1.0f; else dir.y*=-1.0f;
    }
    return pts;
}

void ArenaMatch::Damage(int targetIdx, int amount, int attackerIdx, const char* label, bool heavy, bool ignoreShield, const GameAudio* audio, bool soundOn) {
    auto& target=fighters[targetIdx]; if (target.hp<=0||ended) return;
    int dmg=std::max(1,(int)roundf(amount*(suddenDeath?1.5f:1.0f)));
    if (!ignoreShield && target.shield>0) {
        int abs=std::min(target.shield,dmg); target.shield-=abs; dmg-=abs;
        if (dmg<=0) { AddText(target.pos,"SCUDO!",{34,211,238,255}); return; }
    }
    target.hp=std::max(0,target.hp-dmg); target.hurtCount++; target.hitFlash=0.13f; target.sqX=1.20f; target.sqY=0.82f;
    SpawnParticles(target.pos,target.def->color,heavy?12:7,6.0f); char buf[32]; std::snprintf(buf,32,"-%d",dmg);
    AddText({target.pos.x+RandF(-0.4f,0.4f),target.pos.y},label?label:buf,{255,105,125,255});
    if (!demo) { if (heavy) { hitstop=std::max(hitstop,0.035f); shake=std::max(shake,0.22f); Audio_Play(audio,SFX_CRIT,soundOn); } else Audio_Play(audio,SFX_HIT,soundOn); }
    if (target.hp>0) TriggerVmEvent(targetIdx, VmTrig::ON_HURT, attackerIdx, audio, soundOn);
    if (target.customReflect>0 && attackerIdx>=0 && attackerIdx!=targetIdx && fighters[attackerIdx].hp>0) {
        fighters[attackerIdx].hp=std::max(0,fighters[attackerIdx].hp-target.customReflect);
        std::snprintf(buf,32,"-%d RIFL!",target.customReflect); AddText(fighters[attackerIdx].pos,buf,{250,204,21,255});
    }
    if ((target.def->id=="verity"||(target.fusedDef&&target.fusedDef->id=="verity"))&&target.hp>0&&target.verityLvl<6&&(rand()%100)<45) {
        target.verityLvl++; target.maxHp+=12; target.hp=std::min(target.maxHp,target.hp+12); target.bonusDmg+=2; AddText(target.pos,"VERITY LV+!",{244,114,182,255});
    }
    if ((target.def->id=="cactus"||(target.fusedDef&&target.fusedDef->id=="cactus"))&&attackerIdx>=0&&attackerIdx!=targetIdx&&fighters[attackerIdx].hp>0) {
        fighters[attackerIdx].hp=std::max(0,fighters[attackerIdx].hp-3); AddText(fighters[attackerIdx].pos,"-3 Spine",{163,230,53,255});
    }
    if (attackerIdx>=0&&(fighters[attackerIdx].def->id=="vampire"||(fighters[attackerIdx].fusedDef&&fighters[attackerIdx].fusedDef->id=="vampire"))&&fighters[attackerIdx].hp>0) {
        int heal=std::max(1,(int)roundf(dmg*0.55f)); fighters[attackerIdx].hp=std::min(fighters[attackerIdx].maxHp,fighters[attackerIdx].hp+heal);
        std::snprintf(buf,32,"+%d",heal); AddText(fighters[attackerIdx].pos,buf,{46,229,121,255});
    }
    if (target.hp<=0) {
        SpawnParticles(target.pos,WHITE,28,8.0f); shockwaves.push_back({target.pos,0.5f,3.4f,target.def->color,0.45f,0.45f});
        if (!demo) { shake=0.36f; Audio_Play(audio,SFX_KO,soundOn); }
        if (attackerIdx>=0&&attackerIdx<(int)fighters.size()&&fighters[attackerIdx].hp>0) TriggerVmEvent(attackerIdx,VmTrig::ON_KILL,targetIdx,audio,soundOn);
    }
}

void ArenaMatch::ExecuteVmRule(int idx, const ScriptRule& r, int otherIdx, const GameAudio* audio, bool soundOn) {
    if (idx<0||idx>=(int)fighters.size()||fighters[idx].hp<=0) return;
    int tIdx=(otherIdx>=0&&otherIdx<(int)fighters.size()&&fighters[otherIdx].team!=fighters[idx].team&&fighters[otherIdx].hp>0)?otherIdx:NearestEnemy(idx);
    int reps=std::max(1,std::min(12,r.repeat));
    for (int rep=0;rep<reps;rep++) {
        if (fighters[idx].hp<=0) break;
        EvalContext ctx=MakeCtx(idx,tIdx,(float)rep);
        if (EvalExpr(r.cond,ctx)==0.0f) continue;
        float amt=EvalExpr(r.amt,ctx), dur=EvalExpr(r.dur,ctx), rad=EvalExpr(r.rad,ctx);
        amt=std::max(-200.0f,std::min(200.0f,amt)); dur=std::max(0.0f,std::min(12.0f,dur)); rad=std::max(0.0f,std::min(12.0f,rad));
        float spd=EvalExpr(r.spd,ctx), ang=EvalExpr(r.ang,ctx), px=EvalExpr(r.x,ctx), py=EvalExpr(r.y,ctx);
        spd=std::max(-40.0f,std::min(40.0f,spd));
        int cnt=std::max(1,std::min(16,(int)roundf(EvalExpr(r.cnt,ctx))));
        auto& f=fighters[idx];
        switch (r.act) {
            case VmAct::SET_VAR: f.vars[std::max(0,std::min(5,r.varIdx))]=amt; break;
            case VmAct::ADD_STAT: {
                int v=(int)roundf(amt);
                if (r.strArg=="hp") f.hp=std::max(1,std::min(f.maxHp,f.hp+v));
                else if (r.strArg=="maxhp") { f.maxHp=std::max(20,f.maxHp+v); f.hp=std::min(f.maxHp,std::max(1,f.hp+v)); }
                else if (r.strArg=="dmg") f.bonusDmg+=v;
                else if (r.strArg=="speed") { f.speedMult=std::max(0.3f,std::min(3.5f,f.speedMult+amt)); f.vel.x*=1.0f+amt; f.vel.y*=1.0f+amt; }
                else if (r.strArg=="r") f.r=std::max(0.5f,std::min(2.4f,f.r+amt));
                else if (r.strArg=="blades") f.blades=std::min(14,std::max(0,f.blades+v));
                else if (r.strArg=="shield") f.shield=std::max(0,f.shield+v);
                else if (r.strArg=="regen") f.regen+=amt;
                else if (r.strArg=="reflect") f.customReflect=std::max(0,f.customReflect+v);
                break;
            }
            case VmAct::DAMAGE: {
                int d=std::max(1,(int)roundf(amt));
                if (r.target=="all") { for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=f.team&&fighters[j].hp>0) { lasers.push_back({f.team,idx,f.pos,fighters[j].pos,0.16f,0,f.def->color,{}}); Damage(j,d,idx,nullptr,d>=12,r.ignoreShield,audio,soundOn); } }
                else if (tIdx>=0) { lasers.push_back({f.team,idx,f.pos,fighters[tIdx].pos,0.16f,0,f.def->color,{}}); Damage(tIdx,d,idx,nullptr,d>=12,r.ignoreShield,audio,soundOn); }
                break;
            }
            case VmAct::HEAL: { int h=std::max(1,(int)roundf(amt)); f.hp=std::min(f.maxHp,f.hp+h); char hb[20]; std::snprintf(hb,20,"+%d HP",h); AddText(f.pos,hb,{46,229,121,255}); break; }
            case VmAct::STEAL_HP: if (tIdx>=0) { int d=std::max(1,(int)roundf(amt)); Damage(tIdx,d,idx,nullptr,false,r.ignoreShield,audio,soundOn); f.hp=std::min(f.maxHp,f.hp+d); } break;
            case VmAct::STEAL_ABILITY: if (tIdx>=0&&fighters[tIdx].def!=f.stolenDef) { f.stolenDef=fighters[tIdx].def; for (const auto& sr:fighters[tIdx].runtimeRules) { f.runtimeRules.push_back(sr); f.ruleTimers.push_back(0); f.ruleOnce.push_back(false); f.rulePrevWhen.push_back(false); } AddText(f.pos,"ABILITÀ RUBATA!",{250,204,21,255}); } break;
            case VmAct::PROJ: {
                float sp=(cnt>1)?0.24f:0.0f, pspd=(spd>1)?spd:13.0f, prad=(rad>0.08f&&rad<1.0f)?rad:0.24f; int pd=std::max(1,(int)roundf(amt));
                if (projectiles.size()>1200) break; /* tetto anti-catena */
                for (int k=0;k<cnt;k++) { float a=ang+(k-(cnt-1)*0.5f)*sp; projectiles.push_back({f.team,idx,{f.pos.x+cosf(a)*(f.r+0.25f),f.pos.y+sinf(a)*(f.r+0.25f)},{cosf(a)*pspd,sinf(a)*pspd},pd,f.def->color,prad,r.bnc,3.4f,r.homing,r.pierce,false,false,false,{}}); }
                if (!demo) Audio_Play(audio,SFX_SHOOT,soundOn); break;
            }
            case VmAct::LASER: {
                int ld=std::max(1,(int)roundf(amt));
                for (int k=0;k<std::min(8,cnt);k++) { float a=ang+(k-(std::min(8,cnt)-1)*0.5f)*0.25f; auto pts=ComputeBounceRay(f.pos,a,r.bnc); for (size_t s=0;s+1<pts.size();s++) lasers.push_back({f.team,idx,pts[s],pts[s+1],0.32f,ld,f.def->color,{}}); }
                if (!demo) Audio_Play(audio,SFX_LASER,soundOn); break;
            }
            case VmAct::ULTRA_BEAM: { contBeams.push_back({f.team,idx,std::max(0.5f,dur),0,ang,r.track?0.0f:1.6f,std::max(1,(int)roundf(amt)),r.bnc,r.track,f.def->color}); AddText(f.pos,"ULTRA RAGGIO!",{34,211,238,255}); if (!demo) Audio_Play(audio,SFX_LASER,soundOn); break; }
            case VmAct::BLACK_HOLE: { blackHoles.push_back({f.team,idx,{px,py},std::max(0.5f,dur),0,spd!=0?spd:12.0f,std::max(1.2f,rad),std::max(1,(int)roundf(amt)),f.def->color}); AddText({px,py},"BUCO NERO!",{168,85,247,255}); break; }
            case VmAct::WALL_BARRIER: { float ww=std::max(0.4f,EvalExpr(r.w,ctx)), hh=std::max(0.4f,EvalExpr(r.h,ctx)); tempWalls.push_back({f.team,{px-ww*0.5f,py-hh*0.5f,ww,hh},std::max(0.5f,dur),r.bnc>0,f.def->color}); break; }
            case VmAct::GRAVITY_FLIP: { gravFields.push_back({f.team,std::max(0.5f,dur),px,py,r.target=="all"}); AddText(f.pos,"GRAVITÀ!",{56,189,248,255}); break; }
            case VmAct::TIME_SCALE: { timeFields.push_back({f.team,std::max(0.5f,dur),std::max(0.0f,std::min(2.0f,amt)),r.target=="all"}); AddText(f.pos,amt<=0.1f?"ZA WARUDO!":"TEMPO ALTERATO!",{56,189,248,255}); if (!demo) Audio_Play(audio,SFX_TIME,soundOn); break; }
            case VmAct::TELEPORT: {
                if (r.target=="behind"&&tIdx>=0) { Vector2 en=fighters[tIdx].pos; float ea=atan2f(fighters[tIdx].vel.y,fighters[tIdx].vel.x); f.pos={std::max(bounds.x+f.r,std::min(bounds.x+bounds.width-f.r,en.x-cosf(ea)*2.4f)),std::max(bounds.y+f.r,std::min(bounds.y+bounds.height-f.r,en.y-sinf(ea)*2.4f))}; }
                else f.pos={std::max(bounds.x+f.r,std::min(bounds.x+bounds.width-f.r,px)),std::max(bounds.y+f.r,std::min(bounds.y+bounds.height-f.r,py))};
                shockwaves.push_back({f.pos,0.4f,2.6f,f.def->color,0.3f,0.3f}); break;
            }
            case VmAct::DASH: { float ds=(spd>2)?spd:15.0f; f.vel={cosf(ang)*ds,sinf(ang)*ds}; f.dashBoost=0.65f; break; }
            case VmAct::INVERT_VEL: { if (r.target=="self") { f.vel.x*=-1; f.vel.y*=-1; } else if (tIdx>=0) { fighters[tIdx].vel.x*=-1; fighters[tIdx].vel.y*=-1; AddText(fighters[tIdx].pos,"INVERTITO!",{250,204,21,255}); } break; }
            case VmAct::STUN: if (tIdx>=0) { fighters[tIdx].stun=std::max(fighters[tIdx].stun,std::max(0.2f,dur)); AddText(fighters[tIdx].pos,"STUN!",{250,204,21,255}); } break;
            case VmAct::SLOW: if (tIdx>=0) fighters[tIdx].slow=std::max(fighters[tIdx].slow,std::max(0.2f,dur)); break;
            case VmAct::POISON: if (tIdx>=0) { fighters[tIdx].poison=std::max(fighters[tIdx].poison,std::max(0.5f,dur)); fighters[tIdx].poisonDps=std::max(1.0f,amt); } break;
            case VmAct::FREEZE_PROJ: { for (int i=(int)projectiles.size()-1;i>=0;i--) if (projectiles[i].team!=f.team) { SpawnParticles(projectiles[i].pos,WHITE,4,3); projectiles.erase(projectiles.begin()+i); } AddText(f.pos,"PROIETTILI DISTRUTTI!",{125,211,252,255}); break; }
            case VmAct::EXPLODE: case VmAct::SHOCKWAVE: {
                float er=std::max(1.5f,rad); int ed=std::max(1,(int)roundf(amt)); shockwaves.push_back({f.pos,f.r,er,f.def->color,0.4f,0.4f}); SpawnParticles(f.pos,f.def->color,18,7);
                for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=f.team&&fighters[j].hp>0&&hypotf(fighters[j].pos.x-f.pos.x,fighters[j].pos.y-f.pos.y)<er) Damage(j,ed,idx,nullptr,r.act==VmAct::EXPLODE,r.ignoreShield,audio,soundOn);
                if (!demo&&r.act==VmAct::EXPLODE) Audio_Play(audio,SFX_BOOM,soundOn); break;
            }
            case VmAct::SPAWN_MINION: {
                int mc=0; for (const auto& ft:fighters) if (ft.team==f.team&&ft.hp>0) mc++;
                for (int k=0;k<std::min(3,cnt)&&mc<9;k++,mc++) { Fighter m=MakeFighter(f.def,nullptr,f.team,f.pos.x+RandF(-0.8f,0.8f),f.pos.y+RandF(-0.8f,0.8f),RandF(0,2*PI),true); if (amt>=10) { m.maxHp=(int)amt; m.hp=m.maxHp; } if (r.strArg!="1") m.runtimeRules.clear(); fighters.push_back(m); } break;
            }
            case VmAct::SPAWN_TURRET: if (hazards.size()<24) hazards.push_back({HazardType::TURRET,f.team,idx,f.pos,{0,0},0,0,0.65f,std::max(4,(int)amt),std::max(1.0f,dur),0.2f}); break;
            case VmAct::HAZARD: {
                HazardType ht=HazardType::WEB;
                if (r.strArg=="darkweb") ht=HazardType::DARK_WEB; else if (r.strArg=="frost") ht=HazardType::FROST; else if (r.strArg=="acid") ht=HazardType::ACID;
                else if (r.strArg=="shackle") ht=HazardType::SHACKLE; else if (r.strArg=="dice") ht=HazardType::DICE; else if (r.strArg=="spike") ht=HazardType::SPIKE; else if (r.strArg=="bomb") ht=HazardType::BOMB;
                hazards.push_back({ht,f.team,idx,{px,py},{0,0},0,0,std::max(0.5f,rad),(int)amt,std::max(0.5f,dur),0}); break;
            }
            case VmAct::PULL: case VmAct::PUSH: if (tIdx>=0) { Vector2 d={f.pos.x-fighters[tIdx].pos.x,f.pos.y-fighters[tIdx].pos.y}; float l=hypotf(d.x,d.y), dir=(r.act==VmAct::PULL)?1.0f:-1.0f, sp=(spd>2)?spd:12.0f; if (l>0.1f) fighters[tIdx].vel={(d.x/l)*sp*dir,(d.y/l)*sp*dir}; } break;
            case VmAct::SET_POS: f.pos={std::max(bounds.x+f.r,std::min(bounds.x+bounds.width-f.r,px)),std::max(bounds.y+f.r,std::min(bounds.y+bounds.height-f.r,py))}; break;
            case VmAct::SET_VEL: f.vel={px,py}; break;
            case VmAct::DRAW_FX: AddText(f.pos,r.strArg.empty()?"FX!":r.strArg,f.def->color); break;
            default: break;
        }
    }
}

void ArenaMatch::TriggerVmEvent(int idx, VmTrig trig, int otherIdx, const GameAudio* audio, bool soundOn) {
    if (idx<0||idx>=(int)fighters.size()) return;
    size_t nRules=fighters[idx].runtimeRules.size();
    for (size_t ri=0;ri<nRules;ri++) {
        const ScriptRule r=fighters[idx].runtimeRules[ri];
        if (r.trig==trig) {
            if (r.once && ri<fighters[idx].ruleOnce.size() && fighters[idx].ruleOnce[ri]) continue;
            if (r.once && ri<fighters[idx].ruleOnce.size()) fighters[idx].ruleOnce[ri]=true;
            ExecuteVmRule(idx,r,otherIdx,audio,soundOn);
        }
    }
}

void ArenaMatch::RunWallAbility(int idx, const BallDef* bdef, const GameAudio* audio, bool soundOn) {
    if (!bdef) return; auto& f=fighters[idx]; const auto& id=bdef->id;
    float cx=std::max(bounds.x+1.1f,std::min(bounds.x+bounds.width-1.1f,f.pos.x)), cy=std::max(bounds.y+1.1f,std::min(bounds.y+bounds.height-1.1f,f.pos.y));
    if (id=="spider"||id=="blackspider") hazards.push_back({id=="blackspider"?HazardType::DARK_WEB:HazardType::WEB,f.team,idx,{cx,cy},{0,0},0,0,id=="blackspider"?1.45f:1.25f,0,6.5f,0});
    else if (id=="laser") { int t=NearestEnemy(idx); float a=(t>=0)?atan2f(fighters[t].pos.y-f.pos.y,fighters[t].pos.x-f.pos.x):RandF(0,2*PI); auto pts=ComputeBounceRay(f.pos,a,0); if (pts.size()>1) lasers.push_back({f.team,idx,pts[0],pts[1],0.35f,9,{6,182,212,255},{}}); if (!demo) Audio_Play(audio,SFX_LASER,soundOn); }
    else if (id=="conductor"||id=="electroking") { bool h=(rand()%2==0); hazards.push_back({HazardType::RAIL,f.team,idx,{h?0.0f:f.pos.x,h?f.pos.y:0.0f},{0,0},h?bounds.width:0.7f,h?0.7f:bounds.height,0,id=="electroking"?1:0,5,0}); }
    else if (id=="bigspike") hazards.push_back({HazardType::SPIKE,f.team,idx,{cx,cy},{0,0},0,0,0.85f,9,8,0});
    else if (id=="poisonspike") hazards.push_back({HazardType::POISON_SPIKE,f.team,idx,{cx,cy},{0,0},0,0,0.8f,6,8,0});
    else if (id=="cannon") hazards.push_back({HazardType::CANNON,f.team,idx,{cx,cy},{0,0},0,0,0.68f,0,9,0.5f});
    else if (id=="reforge") { f.reforgeLvl=std::min(8,f.reforgeLvl+1); AddText(f.pos,"FORGIA +1!",{245,158,11,255}); }
}

void ArenaMatch::OnWallBounce(int idx, const GameAudio* audio, bool soundOn) {
    if (!demo) Audio_Play(audio,SFX_BOUNCE,soundOn);
    fighters[idx].bounceCount++;
    TriggerVmEvent(idx, VmTrig::ON_WALL, -1, audio, soundOn);
    RunWallAbility(idx,fighters[idx].def,audio,soundOn);
    if (fighters[idx].fusedDef) RunWallAbility(idx,fighters[idx].fusedDef,audio,soundOn);
    if (fighters[idx].stolenDef) RunWallAbility(idx,fighters[idx].stolenDef,audio,soundOn);
}

void ArenaMatch::CheckOrbitHits(int idx, float dist, int dmg, int count, int slotBase, const GameAudio* audio, bool soundOn) {
    const auto& f=fighters[idx];
    for (int i=0;i<count;i++) {
        float a=f.angle+i*(2*PI/std::max(1,count)); Vector2 b={f.pos.x+cosf(a)*dist,f.pos.y+sinf(a)*dist};
        for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=f.team&&fighters[j].hp>0&&hypotf(fighters[j].pos.x-b.x,fighters[j].pos.y-b.y)<fighters[j].r+0.45f&&CooldownReady(idx,j,slotBase+i,0.34f)) {
            Damage(j,dmg,idx,nullptr,false,false,audio,soundOn); if (!demo) Audio_Play(audio,SFX_BLADE,soundOn);
        }
    }
}

void ArenaMatch::ApplyBallContactEffect(int atkIdx, int defIdx, const BallDef* bdef, const GameAudio* audio, bool soundOn) {
    if (!bdef) return; auto& atk=fighters[atkIdx]; auto& def=fighters[defIdx]; const auto& id=bdef->id;
    if (id=="electric") { def.stun=std::max(def.stun,0.65f); AddText(def.pos,"ZAP!",{250,204,21,255}); }
    else if (id=="virus") { def.poison=std::max(def.poison,4.0f); AddText(def.pos,"INFETTO!",{192,38,211,255}); }
    else if (id=="glass") for (int k=0;k<6;k++) { float a=(2*PI*k/6)+RandF(-0.2f,0.2f); projectiles.push_back({atk.team,atkIdx,atk.pos,{cosf(a)*12.5f,sinf(a)*12.5f},4,{186,230,253,255},0.22f,0,0.9f,false,false,false,false,false,{}}); }
}

void ArenaMatch::ContactHit(int atkIdx, int defIdx, const GameAudio* audio, bool soundOn) {
    auto& atk=fighters[atkIdx]; atk.hitCount++;
    int baseDmg=atk.fusedDef?(int)roundf((atk.def->dmg+atk.fusedDef->dmg)*0.65f):atk.def->dmg;
    int dmg=baseDmg+atk.bonusDmg; bool heavy=false;
    if (atk.def->id=="fibonacci"||(atk.fusedDef&&atk.fusedDef->id=="fibonacci")) { dmg=atk.fibA+atk.bonusDmg; heavy=dmg>=8; int nxt=atk.fibA+atk.fibB; atk.fibA=atk.fibB; atk.fibB=std::min(55,nxt); }
    if (atk.def->id=="charge"||(atk.fusedDef&&atk.fusedDef->id=="charge")) { dmg+=atk.chargeStack*3; if (atk.chargeStack>=3) heavy=true; atk.chargeStack=1; }
    if (atk.dashBoost>0) { dmg+=6; heavy=true; atk.dashBoost=0; }
    Damage(defIdx,dmg,atkIdx,nullptr,heavy,false,audio,soundOn);
    TriggerVmEvent(atkIdx,VmTrig::ON_HIT,defIdx,audio,soundOn);
    ApplyBallContactEffect(atkIdx,defIdx,atk.def,audio,soundOn);
    if (atk.fusedDef) ApplyBallContactEffect(atkIdx,defIdx,atk.fusedDef,audio,soundOn);
    if (atk.stolenDef) ApplyBallContactEffect(atkIdx,defIdx,atk.stolenDef,audio,soundOn);
}

void ArenaMatch::RunTickAbility(int idx, const BallDef* bdef, float& timer, float dt, int tIdx, const GameAudio* audio, bool soundOn) {
    if (!bdef) return; auto& f=fighters[idx]; const auto& bid=bdef->id; timer+=dt; bool lowHp=f.hp<f.maxHp*0.5f;
    if (f.blades>0 && bid!="blade") CheckOrbitHits(idx,f.r+0.65f,6,f.blades,700,audio,soundOn);
    if (bid=="blade") { if (timer>=4&&f.blades<8) { timer=0; f.blades++; AddText(f.pos,"+1 Lama!",{56,189,248,255}); } CheckOrbitHits(idx,f.r+0.65f,6,f.blades,100,audio,soundOn); }
    else if (bid=="orbit") CheckOrbitHits(idx,f.r+1.35f,6,3,200,audio,soundOn);
    else if (bid=="axe") CheckOrbitHits(idx,f.r+0.8f,7,1,300,audio,soundOn);
    else if (bid=="spear") CheckOrbitHits(idx,f.r+1.0f,8,1,400,audio,soundOn);
    else if (bid=="reforge"&&f.reforgeLvl>0) CheckOrbitHits(idx,f.r+0.6f,4+f.reforgeLvl*2,1,500,audio,soundOn);
    else if (bid=="zone"&&timer>=0.45f) { timer=0; for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=f.team&&fighters[j].hp>0&&hypotf(fighters[j].pos.x-f.pos.x,fighters[j].pos.y-f.pos.y)<f.r*2.6f) Damage(j,5,idx,nullptr,false,false,audio,soundOn); }
    else if (bid=="range"&&timer>=1.8f) { timer=0; shockwaves.push_back({f.pos,f.r,4.6f,{45,212,191,255},0.4f,0.4f}); for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=f.team&&fighters[j].hp>0&&hypotf(fighters[j].pos.x-f.pos.x,fighters[j].pos.y-f.pos.y)<4.6f) Damage(j,7,idx,nullptr,false,false,audio,soundOn); }
    else if (bid=="charge"&&timer>=1.2f) { timer=0; f.chargeStack=std::min(5,f.chargeStack+1); }
    else if (bid=="burst"&&timer>=2.8f) { timer=0; f.dashBoost=0.7f; float s=hypotf(f.vel.x,f.vel.y); if (s>0.1f) { f.vel.x=(f.vel.x/s)*14.5f; f.vel.y=(f.vel.y/s)*14.5f; } AddText(f.pos,"SCATTO!",{251,146,60,255}); }
    else if (bid=="chess"&&timer>=2.7f) { timer=0; float a=(rand()%2==0)?(PI*0.25f+(rand()%4)*PI*0.5f):((rand()%4)*PI*0.5f); f.vel={cosf(a)*14.0f,sinf(a)*14.0f}; f.dashBoost=0.5f; AddText(f.pos,"SCACCO!",WHITE); }
    else if (bid=="cell") { f.r=std::min(1.65f,f.r+dt*0.04f); if (!f.isMinion&&!f.splitDone&&f.hp>0&&f.hp<=f.maxHp*0.45f) { f.splitDone=true; for (int k=0;k<2;k++) fighters.push_back(MakeFighter(bdef,nullptr,f.team,f.pos.x+(k?0.8f:-0.8f),f.pos.y,RandF(0,2*PI),true)); AddText(f.pos,"MITOSI x2!",{74,222,128,255}); } }
    else if ((bid=="mg"||bid=="thief"||bid=="shuriken"||bid=="bow"||bid=="hive"||bid=="hook"||bid=="harpoon"||bid=="apple"||bid=="lightwing")&&tIdx>=0) {
        float cd=(bid=="mg")?0.46f:(bid=="thief"?(lowHp?0.42f:0.85f):((bid=="hook"||bid=="harpoon")?2.6f:(bid=="apple"?1.6f:1.25f)));
        if (timer>=cd) {
            timer=0; float a=atan2f(fighters[tIdx].pos.y-f.pos.y,fighters[tIdx].pos.x-f.pos.x);
            if (bid=="lightwing") for (int k=-1;k<=1;k++) { float fa=a+k*0.24f; projectiles.push_back({f.team,idx,{f.pos.x+cosf(fa)*(f.r+0.25f),f.pos.y+sinf(fa)*(f.r+0.25f)},{cosf(fa)*13.5f,sinf(fa)*13.5f},5,{253,224,71,255},0.24f,0,2.2f,false,false,false,false,false,{}}); }
            else projectiles.push_back({f.team,idx,{f.pos.x+cosf(a)*(f.r+0.25f),f.pos.y+sinf(a)*(f.r+0.25f)},{cosf(a)*(bid=="bow"?15.0f:(bid=="hive"?9.0f:13.5f)),sinf(a)*(bid=="bow"?15.0f:(bid=="hive"?9.0f:13.5f))},bid=="mg"?5:((bid=="bow"||bid=="harpoon")?8:7),bdef->color,(bid=="shuriken"||bid=="apple")?0.28f:0.22f,(bid=="shuriken"||bid=="bow")?2:0,3.2f,bid=="hive",false,bid=="hook"||bid=="harpoon",bid=="hive",bid=="apple",{}});
            if (!demo) Audio_Play(audio,SFX_SHOOT,soundOn);
        }
    } else if (bid=="beam"&&timer>=2.2f&&tIdx>=0) { timer=0; float a=atan2f(fighters[tIdx].pos.y-f.pos.y,fighters[tIdx].pos.x-f.pos.x); auto pts=ComputeBounceRay(f.pos,a,0); if (pts.size()>1) lasers.push_back({f.team,idx,pts[0],pts[1],0.35f,11,{34,211,238,255},{}}); if (!demo) Audio_Play(audio,SFX_LASER,soundOn); }
    else if (bid=="dice"&&timer>=2.1f) { timer=0; hazards.push_back({HazardType::DICE,f.team,idx,f.pos,{0,0},0,0,0.65f,1+(rand()%6),7.5f,0}); }
    else if ((bid=="bomb"||bid=="icecone")&&timer>=2.2f) { timer=0; hazards.push_back({HazardType::BOMB,f.team,idx,f.pos,{0,0},0,0,0.6f,bid=="icecone"?1:0,1.4f,0}); }
    else if (bid=="cactus"&&timer>=2.3f) { timer=0; hazards.push_back({HazardType::CACTUS,f.team,idx,f.pos,{0,0},0,0,0.65f,0,7.5f,0}); }
    else if (bid=="shackles"&&timer>=2.4f) { timer=0; hazards.push_back({HazardType::SHACKLE,f.team,idx,tIdx>=0?fighters[tIdx].pos:f.pos,{0,0},0,0,1.15f,0,5.5f,0}); }
    else if (bid=="frost"&&timer>=0.75f) { timer=0; hazards.push_back({HazardType::FROST,f.team,idx,f.pos,{0,0},0,0,1.05f,0,4,0}); }
    else if (bid=="acid"&&timer>=1.6f) { timer=0; hazards.push_back({HazardType::ACID,f.team,idx,{RandF(-5.5f,5.5f),RandF(-5.5f,5.5f)},{0,0},0,0,1.15f,0,6,0}); }
    else if (bid=="slime"&&timer>=1.9f) { timer=0; float a=RandF(0,2*PI); hazards.push_back({HazardType::SLIME_BLOB,f.team,idx,f.pos,{cosf(a)*5.5f,sinf(a)*5.5f},0,0,0.55f,0,5.5f,0}); }
    else if (bid=="ray"&&timer>=3.2f) { timer=0; hazards.push_back({HazardType::TURRET,f.team,idx,f.pos,{0,0},0,0,0.62f,0,7,0.2f}); }
    else if (bid=="wdc"&&timer>=2.2f) { timer=0; hazards.push_back({HazardType::WDC_CELL,f.team,idx,{RandF(-5.2f,5.2f),RandF(-5.2f,5.2f)},{0,0},0,0,0.9f,1+(rand()%9),7,0}); }
    else if (bid=="grave"&&timer>=2.8f) { timer=0; hazards.push_back({HazardType::GRAVE_STONE,f.team,idx,f.pos,{0,0},0,0,0.68f,0,8,0.5f}); }
    else if (bid=="potion"&&timer>=2.1f) { timer=0; int k=rand()%2; hazards.push_back({k==0?HazardType::POTION_HEAL:HazardType::POTION_POISON,f.team,idx,k==0?f.pos:(tIdx>=0?fighters[tIdx].pos:f.pos),{0,0},0,0,1.1f,0,4.5f,0}); }
    else if (bid=="snake") {
        f.tail.insert(f.tail.begin(),f.pos); size_t ml=(size_t)std::min(24,5+(int)(time*0.8f)); while (f.tail.size()>ml) f.tail.pop_back();
        if (timer>=0.38f) { timer=0; for (size_t k=4;k<f.tail.size();k+=4) for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=f.team&&fighters[j].hp>0&&hypotf(fighters[j].pos.x-f.tail[k].x,fighters[j].pos.y-f.tail[k].y)<fighters[j].r+0.4f) Damage(j,5,idx,nullptr,false,false,audio,soundOn); }
    }
}

void ArenaMatch::Update(float dt, const GameAudio* audio, bool soundOn) {
    if (ended) return; if (hitstop>0) { hitstop-=dt; return; }
    time+=dt; if (time>35&&!suddenDeath&&!demo) suddenDeath=true;
    shake=std::max(0.0f,shake-dt*1.2f); for (float& wf : wallFlash) wf=std::max(0.0f,wf-dt*3);
    for (int i=(int)timeFields.size()-1;i>=0;i--) { timeFields[i].life-=dt; if (timeFields[i].life<=0) timeFields.erase(timeFields.begin()+i); }
    for (int i=(int)gravFields.size()-1;i>=0;i--) { gravFields[i].life-=dt; if (gravFields[i].life<=0) gravFields.erase(gravFields.begin()+i); }
    for (int i=(int)tempWalls.size()-1;i>=0;i--) { tempWalls[i].life-=dt; if (tempWalls[i].life<=0) tempWalls.erase(tempWalls.begin()+i); }
    auto getTS=[&](int team)->float{ float ts=1.0f; for (const auto& tf:timeFields) if (tf.affectAll||tf.ownerTeam!=team) ts=std::min(ts,tf.scale); return ts; };

    size_t countNow=fighters.size();
    for (size_t idx=0;idx<countNow;idx++) {
        auto& f=fighters[idx]; if (f.hp<=0) continue;
        float fdt=dt*getTS(f.team);
        if (!f.startDone) { f.startDone=true; TriggerVmEvent((int)idx,VmTrig::ON_START,-1,audio,soundOn); }
        size_t rn=f.runtimeRules.size();
        for (size_t ri=0;ri<rn;ri++) {
            const ScriptRule r=f.runtimeRules[ri];
            if (r.trig==VmTrig::ON_FRAME) ExecuteVmRule((int)idx,r,-1,audio,soundOn);
            else if (r.trig==VmTrig::EVERY&&ri<f.ruleTimers.size()) {
                f.ruleTimers[ri]+=fdt; float iv=std::max(0.15f,EvalExpr(r.interval,MakeCtx((int)idx,-1)));
                if (f.ruleTimers[ri]>=iv) { f.ruleTimers[ri]=0; ExecuteVmRule((int)idx,r,-1,audio,soundOn); }
            } else if (r.trig==VmTrig::WHEN&&ri<f.rulePrevWhen.size()) {
                bool now=(EvalExpr(r.cond,MakeCtx((int)idx,-1))!=0.0f);
                if (now&&!f.rulePrevWhen[ri]&&!(r.once&&ri<f.ruleOnce.size()&&f.ruleOnce[ri])) { if (r.once&&ri<f.ruleOnce.size()) f.ruleOnce[ri]=true; ExecuteVmRule((int)idx,r,-1,audio,soundOn); }
                f.rulePrevWhen[ri]=now;
            }
        }
        f.hitFlash=std::max(0.0f,f.hitFlash-dt); f.sqX+=(1-f.sqX)*std::min(1.0f,dt*14); f.sqY+=(1-f.sqY)*std::min(1.0f,dt*14); f.timerAux+=fdt;
        if (f.timerAux>=1.0f) { f.timerAux=0; if (f.regen>0&&f.hp<f.maxHp) f.hp=std::min(f.maxHp,f.hp+(int)f.regen); if (f.poison>0) { f.poison-=1; Damage((int)idx,(int)f.poisonDps,-1,"-VELENO",false,false,audio,soundOn); if (f.hp<=0) continue; } }
        int tIdx=NearestEnemy((int)idx); if (tIdx>=0) f.look=atan2f(fighters[tIdx].pos.y-f.pos.y,fighters[tIdx].pos.x-f.pos.x);
        if (f.stun>0) { f.stun-=fdt; continue; } if (f.slow>0) f.slow-=fdt; if (f.dashBoost>0) f.dashBoost-=fdt;
        for (const auto& gf:gravFields) if (gf.affectAll||gf.ownerTeam!=f.team) { f.vel.x+=gf.gx*fdt; f.vel.y+=gf.gy*fdt; }
        for (const auto& bh:blackHoles) if (bh.team!=f.team) { Vector2 d={bh.pos.x-f.pos.x,bh.pos.y-f.pos.y}; float dist=hypotf(d.x,d.y); if (dist>0.1f&&dist<bh.rad*1.8f) { f.vel.x+=(d.x/dist)*bh.force*fdt; f.vel.y+=(d.y/dist)*bh.force*fdt; } }
        float sm=(f.slow>0)?0.55f:1.0f; f.pos.x+=f.vel.x*fdt*sm; f.pos.y+=f.vel.y*fdt*sm;
        f.trail.push_back(f.pos); if (f.trail.size()>8) f.trail.erase(f.trail.begin());
        bool bnc=false;
        if (f.pos.x-f.r<bounds.x) { f.pos.x=bounds.x+f.r; f.vel.x=fabsf(f.vel.x); bnc=true; wallFlash[0]=1; }
        else if (f.pos.x+f.r>bounds.x+bounds.width) { f.pos.x=bounds.x+bounds.width-f.r; f.vel.x=-fabsf(f.vel.x); bnc=true; wallFlash[1]=1; }
        if (f.pos.y-f.r<bounds.y) { f.pos.y=bounds.y+f.r; f.vel.y=fabsf(f.vel.y); bnc=true; wallFlash[2]=1; }
        else if (f.pos.y+f.r>bounds.y+bounds.height) { f.pos.y=bounds.y+bounds.height-f.r; f.vel.y=-fabsf(f.vel.y); bnc=true; wallFlash[3]=1; }
        for (const auto& tw:tempWalls) {
            float cx=std::max(tw.rect.x,std::min(f.pos.x,tw.rect.x+tw.rect.width)), cy=std::max(tw.rect.y,std::min(f.pos.y,tw.rect.y+tw.rect.height));
            float dx=f.pos.x-cx, dy=f.pos.y-cy, d=hypotf(dx,dy);
            if (d<f.r) { float nx=(d>0.01f)?dx/d:0, ny=(d>0.01f)?dy/d:-1; f.pos.x+=nx*(f.r-d); f.pos.y+=ny*(f.r-d); float dot=f.vel.x*nx+f.vel.y*ny; if (dot<0) { f.vel.x-=2*dot*nx; f.vel.y-=2*dot*ny; bnc=true; } }
        }
        if (bnc) { f.sqX=0.84f; f.sqY=1.18f; float ang=atan2f(f.vel.y,f.vel.x)+RandF(-0.14f,0.14f); float spd=std::max(8.5f*f.speedMult,std::min(14.0f*f.speedMult,hypotf(f.vel.x,f.vel.y))); f.vel={cosf(ang)*spd,sinf(ang)*spd}; OnWallBounce((int)idx,audio,soundOn); }
        f.angle+=fdt*((f.def->id=="axe"&&f.hp<f.maxHp*0.5f)?9.5f:4.2f);
        RunTickAbility((int)idx,f.def,f.timerA,fdt,tIdx,audio,soundOn);
        if (f.fusedDef) RunTickAbility((int)idx,f.fusedDef,f.timerB,fdt,tIdx,audio,soundOn);
    }
    for (int i=(int)blackHoles.size()-1;i>=0;i--) {
        auto& bh=blackHoles[i]; bh.life-=dt; bh.tick-=dt; if (bh.life<=0) { blackHoles.erase(blackHoles.begin()+i); continue; }
        if (bh.tick<=0) { bh.tick=0.28f; for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=bh.team&&fighters[j].hp>0&&hypotf(fighters[j].pos.x-bh.pos.x,fighters[j].pos.y-bh.pos.y)<bh.rad*0.65f) Damage(j,bh.dmg,bh.ownerIdx,nullptr,false,false,audio,soundOn); }
    }
    for (int i=(int)contBeams.size()-1;i>=0;i--) {
        auto& cb=contBeams[i]; cb.life-=dt; cb.tick-=dt; cb.spinAngle+=cb.spinRate*dt;
        if (cb.ownerIdx<0||cb.ownerIdx>=(int)fighters.size()||fighters[cb.ownerIdx].hp<=0||cb.life<=0) { contBeams.erase(contBeams.begin()+i); continue; }
        if (cb.tick<=0.0f) {
            cb.tick=0.22f; const auto& ow=fighters[cb.ownerIdx]; auto pts=ComputeBounceRay(ow.pos,cb.track?ow.look:cb.spinAngle,cb.bounces);
            for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=cb.team&&fighters[j].hp>0)
                for (size_t s=0;s+1<pts.size();s++) if (PtSegDist(fighters[j].pos,pts[s],pts[s+1])<fighters[j].r+0.35f) { Damage(j,cb.dmg,cb.ownerIdx,nullptr,false,false,audio,soundOn); break; }
        }
    }
    for (int i=0;i<(int)fighters.size();i++) for (int j=i+1;j<(int)fighters.size();j++) {
        auto &a=fighters[i], &b=fighters[j]; if (a.hp<=0||b.hp<=0) continue;
        float dx=b.pos.x-a.pos.x, dy=b.pos.y-a.pos.y, dist=hypotf(dx,dy), minD=a.r+b.r;
        if (dist<minD) {
            float nx=(dist>0.001f)?dx/dist:1, ny=(dist>0.001f)?dy/dist:0, ov=minD-dist;
            a.pos.x-=nx*ov*0.5f; a.pos.y-=ny*ov*0.5f; b.pos.x+=nx*ov*0.5f; b.pos.y+=ny*ov*0.5f;
            float dot=(a.vel.x-b.vel.x)*nx+(a.vel.y-b.vel.y)*ny;
            if (dot>0) { a.vel.x-=dot*nx; a.vel.y-=dot*ny; b.vel.x+=dot*nx; b.vel.y+=dot*ny; }
            if (a.team!=b.team&&CooldownReady(i,j,0,0.16f)) { shockwaves.push_back({{(a.pos.x+b.pos.x)*0.5f,(a.pos.y+b.pos.y)*0.5f},0.4f,2.2f,WHITE,0.25f,0.25f}); ContactHit(i,j,audio,soundOn); ContactHit(j,i,audio,soundOn); }
        }
    }
    for (int i=(int)projectiles.size()-1;i>=0;i--) {
        auto& p=projectiles[i]; float pdt=dt*getTS(p.team);
        for (const auto& bh:blackHoles) if (bh.team!=p.team) { Vector2 d={bh.pos.x-p.pos.x,bh.pos.y-p.pos.y}; float dist=hypotf(d.x,d.y); if (dist>0.2f&&dist<bh.rad*1.6f) { p.vel.x+=(d.x/dist)*bh.force*1.4f*pdt; p.vel.y+=(d.y/dist)*bh.force*1.4f*pdt; } }
        if (p.homing) {
            int best=-1; float bd=1e9f; for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=p.team&&fighters[j].hp>0) { float d=hypotf(fighters[j].pos.x-p.pos.x,fighters[j].pos.y-p.pos.y); if (d<bd) { bd=d; best=j; } }
            if (best>=0) { float a=atan2f(fighters[best].pos.y-p.pos.y,fighters[best].pos.x-p.pos.x); p.vel.x+=(cosf(a)*10.5f-p.vel.x)*std::min(1.0f,pdt*4.5f); p.vel.y+=(sinf(a)*10.5f-p.vel.y)*std::min(1.0f,pdt*4.5f); }
        }
        p.pos.x+=p.vel.x*pdt; p.pos.y+=p.vel.y*pdt; p.life-=pdt;
        if (p.bounces>0) { if (p.pos.x<bounds.x+p.r||p.pos.x>bounds.x+bounds.width-p.r) { p.vel.x*=-1; p.bounces--; } if (p.pos.y<bounds.y+p.r||p.pos.y>bounds.y+bounds.height-p.r) { p.vel.y*=-1; p.bounces--; } }
        for (const auto& tw:tempWalls) if (CheckCollisionCircleRec(p.pos,p.r,tw.rect)) { if (tw.reflectProj) { p.vel.x*=-1; p.vel.y*=-1; } else p.life=0; }
        bool hit=false;
        for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=p.team&&fighters[j].hp>0&&hypotf(fighters[j].pos.x-p.pos.x,fighters[j].pos.y-p.pos.y)<fighters[j].r+p.r) {
            if (p.pierce&&std::find(p.hitIds.begin(),p.hitIds.end(),j)!=p.hitIds.end()) continue;
            p.hitIds.push_back(j); Damage(j,p.dmg,p.ownerIdx,nullptr,p.hookPull,false,audio,soundOn);
            if (p.poisonHit) fighters[j].poison=std::max(fighters[j].poison,3.0f);
            if (p.hookPull&&p.ownerIdx>=0&&p.ownerIdx<(int)fighters.size()) { Vector2 d={fighters[p.ownerIdx].pos.x-fighters[j].pos.x,fighters[p.ownerIdx].pos.y-fighters[j].pos.y}; float l=hypotf(d.x,d.y); if (l>0.1f) fighters[j].vel={(d.x/l)*12.0f,(d.y/l)*12.0f}; }
            if (p.healOwner&&p.ownerIdx>=0&&p.ownerIdx<(int)fighters.size()&&fighters[p.ownerIdx].hp>0) fighters[p.ownerIdx].hp=std::min(fighters[p.ownerIdx].maxHp,fighters[p.ownerIdx].hp+6);
            if (!p.pierce) { hit=true; break; }
        }
        if (hit||p.life<=0||p.pos.x<bounds.x-0.5f||p.pos.x>bounds.x+bounds.width+0.5f||p.pos.y<bounds.y-0.5f||p.pos.y>bounds.y+bounds.height+0.5f) projectiles.erase(projectiles.begin()+i);
    }
    for (int i=(int)hazards.size()-1;i>=0;i--) {
        auto& h=hazards[i]; h.life-=dt; h.tick-=dt;
        if (h.type==HazardType::SLIME_BLOB) { h.pos.x+=h.vel.x*dt; h.pos.y+=h.vel.y*dt; if (h.pos.x<bounds.x+h.r||h.pos.x>bounds.x+bounds.width-h.r) h.vel.x*=-1; if (h.pos.y<bounds.y+h.r||h.pos.y>bounds.y+bounds.height-h.r) h.vel.y*=-1; }
        if ((h.type==HazardType::TURRET||h.type==HazardType::CANNON||h.type==HazardType::GRAVE_STONE)&&h.tick<=0) {
            h.tick=(h.type==HazardType::TURRET)?0.75f:((h.type==HazardType::CANNON)?1.1f:1.4f);
            int best=-1; float bd=1e9f; for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=h.team&&fighters[j].hp>0) { float d=hypotf(fighters[j].pos.x-h.pos.x,fighters[j].pos.y-h.pos.y); if (d<bd) { bd=d; best=j; } }
            if (best>=0) { float a=atan2f(fighters[best].pos.y-h.pos.y,fighters[best].pos.x-h.pos.x), sp=(h.type==HazardType::TURRET)?14.0f:((h.type==HazardType::CANNON)?11.0f:8.0f); projectiles.push_back({h.team,h.ownerIdx,h.pos,{cosf(a)*sp,sinf(a)*sp},h.val>0?h.val:(h.type==HazardType::CANNON?8:5),h.type==HazardType::TURRET?Color{248,113,113,255}:Color{200,220,255,255},h.type==HazardType::CANNON?0.32f:0.20f,0,2.5f,h.type==HazardType::GRAVE_STONE,false,false,false,false,{}}); }
        }
        if (h.type==HazardType::BOMB&&h.life<=0) {
            Color bc=(h.val==1)?Color{96,165,250,255}:Color{255,145,45,255}; SpawnParticles(h.pos,bc,24,7.0f); shockwaves.push_back({h.pos,0.5f,3.4f,bc,0.35f,0.35f});
            if (!demo) { Audio_Play(audio,SFX_BOOM,soundOn); shake=0.25f; }
            for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=h.team&&fighters[j].hp>0&&hypotf(fighters[j].pos.x-h.pos.x,fighters[j].pos.y-h.pos.y)<3.4f) { Damage(j,17,h.ownerIdx,nullptr,true,false,audio,soundOn); if (h.val==1) fighters[j].slow=1.2f; }
            if (h.val==1) for (int k=0;k<6;k++) { float a=k*PI/3; projectiles.push_back({h.team,h.ownerIdx,h.pos,{cosf(a)*11.5f,sinf(a)*11.5f},5,{147,197,253,255},0.20f,0,0.9f,false,false,false,false,false,{}}); }
            hazards.erase(hazards.begin()+i); continue;
        }
        for (int j=0;j<(int)fighters.size();j++) {
            auto& o=fighters[j]; if (o.hp<=0) continue;
            if (h.type==HazardType::POTION_HEAL&&o.team==h.team&&hypotf(o.pos.x-h.pos.x,o.pos.y-h.pos.y)<h.r+o.r) { if (h.tick<=0) { h.tick=0.5f; o.hp=std::min(o.maxHp,o.hp+5); } continue; }
            if (o.team==h.team) continue;
            if (h.type==HazardType::RAIL) { if (fabsf(o.pos.x-h.pos.x)<h.w*0.5f+o.r&&fabsf(o.pos.y-h.pos.y)<h.h*0.5f+o.r&&h.tick<=0) { h.tick=0.4f; Damage(j,h.val?10:9,h.ownerIdx,nullptr,false,false,audio,soundOn); } }
            else if (hypotf(o.pos.x-h.pos.x,o.pos.y-h.pos.y)<h.r+o.r) {
                if (h.type==HazardType::WEB||h.type==HazardType::DARK_WEB) { o.slow=0.8f; if (h.tick<=0) { h.tick=0.45f; Damage(j,h.type==HazardType::DARK_WEB?6:4,h.ownerIdx,nullptr,false,false,audio,soundOn); if (h.type==HazardType::DARK_WEB&&h.ownerIdx>=0&&fighters[h.ownerIdx].hp>0) fighters[h.ownerIdx].hp=std::min(fighters[h.ownerIdx].maxHp,fighters[h.ownerIdx].hp+3); } }
                else if (h.type==HazardType::FROST||h.type==HazardType::SLIME_BLOB) { o.slow=0.9f; if (h.tick<=0) { h.tick=0.5f; Damage(j,4,h.ownerIdx,nullptr,false,false,audio,soundOn); } }
                else if (h.type==HazardType::ACID||h.type==HazardType::POTION_POISON) { if (h.tick<=0) { h.tick=0.45f; Damage(j,5,h.ownerIdx,nullptr,false,false,audio,soundOn); } }
                else if (h.type==HazardType::DICE||h.type==HazardType::WDC_CELL) { char b[24]; int d=(int)roundf(h.val*2.2f); std::snprintf(b,24,"-%d #%d",d,h.val); Damage(j,d,h.ownerIdx,b,h.val>=6,false,audio,soundOn); h.life=0; break; }
                else if (h.type==HazardType::CACTUS||h.type==HazardType::SPIKE) { Damage(j,h.val>0?h.val:9,h.ownerIdx,nullptr,false,false,audio,soundOn); h.life=0; break; }
                else if (h.type==HazardType::POISON_SPIKE) { o.poison=std::max(o.poison,4.0f); Damage(j,6,h.ownerIdx,"VELENO!",false,false,audio,soundOn); h.life=0; break; }
                else if (h.type==HazardType::SHACKLE) { o.stun=1.25f; Damage(j,11,h.ownerIdx,"STORDITO!",true,false,audio,soundOn); h.life=0; break; }
            }
        }
        if (h.life<=0) hazards.erase(hazards.begin()+i);
    }
    for (int i=(int)lasers.size()-1;i>=0;i--) {
        auto& l=lasers[i]; l.life-=dt;
        for (int j=0;j<(int)fighters.size();j++) if (fighters[j].team!=l.team&&fighters[j].hp>0&&std::find(l.hitIds.begin(),l.hitIds.end(),j)==l.hitIds.end())
            if (PtSegDist(fighters[j].pos,l.p1,l.p2)<fighters[j].r+0.25f) { l.hitIds.push_back(j); if (l.dmg>0) Damage(j,l.dmg,l.ownerIdx,nullptr,true,false,audio,soundOn); }
        if (l.life<=0) lasers.erase(lasers.begin()+i);
    }
    for (int i=(int)particles.size()-1;i>=0;i--) { particles[i].pos.x+=particles[i].vel.x*dt; particles[i].pos.y+=particles[i].vel.y*dt; particles[i].y+=particles[i].vy*dt; particles[i].vy-=14.0f*dt; if (particles[i].y<0.1f) particles[i].y=0.1f; particles[i].life-=dt; if (particles[i].life<=0) particles.erase(particles.begin()+i); }
    for (int i=(int)shockwaves.size()-1;i>=0;i--) { shockwaves[i].life-=dt; shockwaves[i].r+=(shockwaves[i].maxR-shockwaves[i].r)*std::min(1.0f,dt*11); if (shockwaves[i].life<=0) shockwaves.erase(shockwaves.begin()+i); }
    for (int i=(int)texts.size()-1;i>=0;i--) { texts[i].y+=1.8f*dt; texts[i].life-=dt; if (texts[i].life<=0) texts.erase(texts.begin()+i); }
    bool aa=false, ea=false; for (const auto& f:fighters) if (f.hp>0) { if (f.team==0) aa=true; else ea=true; }
    if (!aa||!ea) {
        if (demo) { const auto& db=GetBallCatalog(); int a=rand()%db.size(), b=(a+1+rand()%(db.size()-1))%db.size(); Init({{&db[a],nullptr}},{{&db[b],nullptr}},true,false,0.3f,0,0,1,0); }
        else { ended=true; winnerTeam=aa?0:1; }
    }
}

