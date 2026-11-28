#ifndef GAME_HPP
#define GAME_HPP
#include "raylib.h"
#include "audio_synth.h"
#include "ball_vm.hpp"
#include "ai_client.hpp"
#include "ui.hpp"
#include <deque>
#include <set>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

constexpr int VIRT_H = 600;
enum class GameState { LOBBY, COLLECTION, AI_CHAT, RPS, DRAFT, ALL_PICK, AIM, ARENA, ROUND_END, UPGRADE, RESULT, EDIT };
enum class GameMode { MODE_1V1=0, MODE_2V2, MODE_FUSION, MODE_RANKED, MODE_TOURNAMENT, MODE_RPS };

struct BallDef {
    std::string id, name; Rarity rarity=Rarity::Raro; Color color=WHITE; int hp=100, dmg=8;
    float speedMult=1.0f, baseRadius=1.15f; std::string visualStyle="none";
    bool isCustom=false; std::string customPrompt; std::vector<ScriptRule> scriptRules; std::string desc;
    bool isDummy=false;   // bersaglio fermo del campo di prova
};
struct Fighter {
    const BallDef* def=nullptr; const BallDef* fusedDef=nullptr; const BallDef* stolenDef=nullptr;
    int team=0; bool isMinion=false; Vector2 anchor{0,0}; Vector2 pos{0,0}, vel{0,0}; float r=1.15f;
    int hp=100, maxHp=100, bonusDmg=0, shield=0; float speedMult=1.0f;
    float stun=0, slow=0, poison=0, poisonDps=4.0f, regen=0, timerA=0, timerB=0, timerAux=0, angle=0, look=0;
    int blades=0, fibA=1, fibB=1, chargeStack=1, verityLvl=1, reforgeLvl=0, customReflect=0;
    int bounceCount=0, hitCount=0, hurtCount=0;
    bool splitDone=false, startDone=false; float dashBoost=0, hitFlash=0, sqX=1, sqY=1;
    float vars[6]={0,0,0,0,0,0};
    std::vector<ScriptRule> runtimeRules; std::vector<float> ruleTimers; std::vector<bool> ruleOnce, rulePrevWhen;
    std::vector<Vector2> tail, trail;
};
struct Projectile { int team=0, ownerIdx=-1; Vector2 pos{0,0}, vel{0,0}; int dmg=5; Color color=WHITE; float r=0.22f; int bounces=0; float life=3; bool homing=false, pierce=false, hookPull=false, poisonHit=false, healOwner=false; std::vector<int> hitIds; };
enum class HazardType { WEB, DARK_WEB, RAIL, DICE, BOMB, CACTUS, SHACKLE, SPIKE, POISON_SPIKE, FROST, ACID, TURRET, CANNON, WDC_CELL, GRAVE_STONE, POTION_HEAL, POTION_POISON, SLIME_BLOB };
struct Hazard { HazardType type=HazardType::WEB; int team=0, ownerIdx=-1; Vector2 pos{0,0}, vel{0,0}; float w=0, h=0, r=0.9f; int val=1; float life=5, tick=0; };
struct LaserBeam { int team=0, ownerIdx=-1; Vector2 p1{0,0}, p2{0,0}; float life=0.3f; int dmg=9; Color color=WHITE; std::vector<int> hitIds; };
struct ContinuousBeam { int team=0, ownerIdx=-1; float life=3.0f, tick=0, spinAngle=0, spinRate=0; int dmg=5, bounces=3; bool track=true; Color color=WHITE; };
struct BlackHole { int team=0, ownerIdx=-1; Vector2 pos{0,0}; float life=4.0f, tick=0, force=12, rad=3.5f; int dmg=6; Color color=PURPLE; };
struct TempWall { int team=0; Rectangle rect{}; float life=5.0f; bool reflectProj=true; Color color=WHITE; };
struct TimeField { int ownerTeam=0; float life=2.5f, scale=0.05f; bool affectAll=false; };
struct GravityField { int ownerTeam=0; float life=3.0f, gx=0, gy=9.0f; bool affectAll=false; };
struct Particle { Vector2 pos{0,0}, vel{0,0}; float y=0.6f, vy=2.0f, life=0.4f, maxLife=0.4f; Color color=WHITE; float r=0.14f; };
struct Shockwave { Vector2 pos{0,0}; float r=0.4f, maxR=2.6f; Color color=WHITE; float life=0.3f, maxLife=0.3f; };
struct FloatText { Vector2 pos{0,0}; float y=1.8f; std::string text; Color color=WHITE; float life=0.8f; };
struct PairCooldown { int a=0, b=0, slot=0; float lastTime=-10; };

class ArenaMatch {
public:
    Rectangle bounds{-7.0f, -7.0f, 14.0f, 14.0f};
    bool demo=false, ranked=false, suddenDeath=false, ended=false, training=false;
    float time=0, hitstop=0, shake=0, wallFlash[4]={0,0,0,0}; int winnerTeam=-1;
    std::vector<Fighter> fighters; std::deque<Projectile> projectiles; std::deque<Hazard> hazards;
    std::deque<LaserBeam> lasers; std::deque<ContinuousBeam> contBeams;
    std::deque<BlackHole> blackHoles; std::deque<TempWall> tempWalls;
    std::deque<TimeField> timeFields; std::deque<GravityField> gravFields;
    std::deque<Particle> particles; std::deque<Shockwave> shockwaves;
    std::deque<FloatText> texts; std::deque<PairCooldown> cooldowns;

    void Init(const std::vector<std::pair<const BallDef*, const BallDef*>>& allies, const std::vector<std::pair<const BallDef*, const BallDef*>>& enemies, bool isDemo, bool isRanked, float allyAimAng, int allyUpgHp, int allyUpgDmg, float allyUpgSpd, float allyRegen);
    void Update(float dt, const GameAudio* audio, bool soundOn);
    std::vector<Vector2> ComputeBounceRay(Vector2 start, float angle, int bounces) const;
private:
    Fighter MakeFighter(const BallDef* def, const BallDef* fused, int team, float x, float y, float ang, bool minion);
    int NearestEnemy(int idx) const;
    EvalContext MakeCtx(int idx, int enemyIdx, float loopI = 0.0f) const;
    void SpawnParticles(Vector2 p, Color c, int count, float speed);
    void AddText(Vector2 p, const std::string& txt, Color c);
    void Damage(int targetIdx, int amount, int attackerIdx, const char* label, bool heavy, bool ignoreShield, const GameAudio* audio, bool soundOn);
    void RunWallAbility(int idx, const BallDef* bdef, const GameAudio* audio, bool soundOn);
    void OnWallBounce(int idx, const GameAudio* audio, bool soundOn);
    void RunTickAbility(int idx, const BallDef* bdef, float& timer, float dt, int tIdx, const GameAudio* audio, bool soundOn);
    void TriggerVmEvent(int idx, VmTrig trig, int otherIdx, const GameAudio* audio, bool soundOn);
    void ExecuteVmRule(int idx, const ScriptRule& r, int otherIdx, const GameAudio* audio, bool soundOn);
    void CheckOrbitHits(int idx, float dist, int dmg, int count, int slotBase, const GameAudio* audio, bool soundOn);
    void ContactHit(int atkIdx, int defIdx, const GameAudio* audio, bool soundOn);
    void ApplyBallContactEffect(int atkIdx, int defIdx, const BallDef* bdef, const GameAudio* audio, bool soundOn);
    bool CooldownReady(int a, int b, int slot, float minGap);
};

std::deque<BallDef>& GetBallCatalog();
const BallDef* FindBallById(const std::string& id);

struct SaveData { int wins=0, streak=0, rp=0, level=1, xp=0; std::vector<std::string> customIds; };

class PallaGame {
public:
    void Init(const std::string& dataDir);
    // Un frame completo: logica + disegno (UI immediate-mode, le coordinate sono "virtuali": altezza 600).
    void Frame(float dt, const InputState& in, float virtW, float uiScale);
    void Shutdown();
private:
    UI ui; GameAudio audio{}; bool soundOn=true; SaveData save; std::string saveFile, customFile, keyFile;
    GameState state=GameState::LOBBY; GameMode mode=GameMode::MODE_1V1;
    int simSpeed=1; float globalTime=0, W=1067.0f, collScroll=0, chatScroll=0; bool chatStick=true; int rarFilter=-1;
    ArenaMatch demoMatch, activeMatch;
    RenderTexture2D previewTex{};
    std::unordered_map<std::string, RenderTexture2D> sprites;

    int allyHearts=3, enemyHearts=3, roundNum=1, lastRoundWinner=0; float roundBannerTimer=0;
    std::vector<const BallDef*> draftChoices; const BallDef* selectedPrimary=nullptr; const BallDef* selectedSecondary=nullptr;
    std::vector<std::pair<const BallDef*, const BallDef*>> roundEnemies; const BallDef* roundAlly=nullptr;
    bool pickingSecondFusion=false; float aimAngle=0, aimTimer=8.0f;
    int upgHp=0, upgDmg=0; float upgSpd=1.0f, upgRegen=0; int upgOptions[3]={0,1,2};
    int rpsPlayer=-1, rpsEnemy=-1, rpsOutcome=0; float rpsShowTimer=0;

    // --- AI Studio ---
    enum class AiJob { NONE, CHAT, VERIFY };
    BallDef draftCustom; std::vector<ChatTurn> chatHistory; std::string chatInput, geminiApiKey, modalKey, modalError, aiModel;
    bool keyModal=false, keyVerified=false, modalOffer=false;
    std::atomic<bool> aiBusy{false}; std::mutex aiMutex; AiResult aiResult; AiJob aiJobKind=AiJob::NONE; bool aiReplyReady=false; std::thread aiThread;
    float aiBusyTime=0; int nativeTarget=0; // 1 = chat, 2 = chiave

    // --- Campo di prova (sacco da 1000 HP fermo, 3 round, uscita libera) ---
    bool training=false, collPick=false; int trainRound=1; BallDef trainBall, trainDummy; GameState trainReturn=GameState::LOBBY;
    struct TrainStat { float time=0; int dmg=0; bool killed=false; } trainStats[3];
    void StartTraining(const BallDef& b, GameState returnTo); void StartTrainRound(); void OnTrainRoundEnded(); void ExitTraining(); void TrainHud();

    // --- Editor palle ---
    BallDef* editTarget=nullptr; BallDef editDraft; bool editBuiltin=false, aiEditMode=false; int editFocus=0; float editScroll=0;
    std::unordered_map<std::string, BallDef> originals; std::set<std::string> editedIds; std::string editsFile;
    std::vector<const BallDef*> spriteRefresh;

    std::string toastMsg; float toastTimer=0;
    struct { bool won=false; int xp=0, rpDelta=0; bool levelUp=false; float duration=0; } resultInfo;

    // logica
    void Logic(float dt);
    void StartNewMatch(); void PrepareNextRound(); void RollDraftThree(); void BeginAimPhase();
    void LaunchRound(); void OnRoundEnded(); void FinishMatch(bool won); void Go(GameState s);
    void SetToast(const std::string& msg);
    void LoadCustomBalls(); void WriteCustomBalls() const;
    void LoadSave(); void WriteSave() const; void LoadKey(); void WriteKey() const;
    // AI
    void OpenAiChatStudio(); void SendAiChatMessage(); void StartAiJob(AiJob kind);
    void HandleAiResult(const AiResult& r, AiJob kind); void SaveCustomBall(bool testImmediately);
    void AppendTyped(std::string& target, size_t maxLen, bool singleLine);
    // grafica
    const RenderTexture2D* GetSprite(const BallDef* def) const;
    void BuildSprite(const BallDef* def, bool force = false);
    std::vector<const BallDef*> spriteQueue; Camera3D curCam{}; float H=600.0f;
    Camera3D ArenaCam(float shake) const;
    // schermate (disegno + click)
    void Backdrop(bool arena3d); void TopBar(const char* title, bool showNav);
    void Rail(); void HandleBack(); void Hud();
    void ScreenLobby(); void ScreenCollection(); void ScreenAiChat(); void ScreenRps(); void ScreenDraft();
    void ScreenEdit(); void OpenEditor(BallDef* b); void SaveEdit(); void ResetEdit(); void OpenAiForEdit(); void ApplyAiDraftToEditor();
    void LoadEdits(); void WriteEdits() const;
    void ScreenAim(); void ScreenArena(); void ScreenUpgrade(); void ScreenResult(); void KeyModal(); void Toast();
    // buttons: 0 nessuno, 1 = PROVA + MODIFICA, 2 = solo PROVA. Ritorna 1 (modifica) o 2 (prova) se premuto.
    int BallCard(const BallDef* def, Rectangle r, bool highlight, bool showDesc, int buttons = 0);
    void Hearts(float x, float y, int n, bool alignRight);
    void StatBars(const BallDef* d, Rectangle r);
};
#endif
