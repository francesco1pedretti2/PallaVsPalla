// Flusso di gioco, salvataggi, studio AI e tutte le schermate (UI immediate-mode: layout e click in un solo punto).
#include "game.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#define SND(id) Audio_Play(&audio, id, soundOn)

static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static Color A(Color c, float a) { return Fade2(c, a); }

// ============================================================================
//  Salvataggi
// ============================================================================
void PallaGame::LoadKey() {
    std::ifstream f(keyFile); if (f.is_open()) std::getline(f, geminiApiKey);
    geminiApiKey = SanitizeApiKey(geminiApiKey);
}
void PallaGame::WriteKey() const { std::ofstream f(keyFile); if (f.is_open()) f << geminiApiKey; }
void PallaGame::LoadSave() { std::ifstream f(saveFile); if (!f.is_open()) return; std::string l; if (std::getline(f, l)) { std::istringstream iss(l); iss >> save.wins >> save.streak >> save.rp >> save.level >> save.xp; } if (save.level < 1) save.level = 1; }
void PallaGame::WriteSave() const { std::ofstream f(saveFile); if (!f.is_open()) return; f << save.wins << " " << save.streak << " " << save.rp << " " << save.level << " " << save.xp << "\n"; }
void PallaGame::SetToast(const std::string& msg) { toastMsg = msg; toastTimer = 3.0f; }

void PallaGame::LoadCustomBalls() {
    std::ifstream f(customFile); if (!f.is_open()) return; int count = 0; if (!(f >> count)) return;
    std::string dummy; std::getline(f, dummy);
    for (int i = 0; i < count; i++) {
        BallDef b; b.isCustom = true; int rar = 5, r = 34, g = 211, bl = 238, rc = 0; std::string hdr; if (!std::getline(f, hdr)) break;
        std::istringstream iss(hdr); iss >> b.id >> rar >> r >> g >> bl >> b.hp >> b.dmg >> b.speedMult >> b.baseRadius >> b.visualStyle >> rc;
        b.rarity = (Rarity)std::max(0, std::min(5, rar)); b.color = { (unsigned char)r, (unsigned char)g, (unsigned char)bl, 255 };
        std::getline(f, b.name); std::getline(f, b.customPrompt); for (char& c : b.customPrompt) if (c == '\x01') c = '\n';
        std::string block; for (int k = 0; k < rc; k++) { std::string rl; if (!std::getline(f, rl)) break; block += rl + "\n"; }
        b.scriptRules = DeserializeScriptRules(block); b.desc = "Palla creata con l'AI (" + std::to_string(b.scriptRules.size()) + " regole)";
        ParsedBallScript ps = ParseAIScript(b.customPrompt); if (!ps.chatMsg.empty()) b.desc = ps.chatMsg;
        GetBallCatalog().push_back(b);
    }
}
void PallaGame::WriteCustomBalls() const {
    std::ofstream f(customFile); if (!f.is_open()) return;
    std::vector<const BallDef*> cs; for (const auto& b : GetBallCatalog()) if (b.isCustom) cs.push_back(&b);
    f << cs.size() << "\n";
    for (const auto* b : cs) {
        f << b->id << " " << (int)b->rarity << " " << (int)b->color.r << " " << (int)b->color.g << " " << (int)b->color.b
          << " " << b->hp << " " << b->dmg << " " << b->speedMult << " " << b->baseRadius << " " << b->visualStyle << " " << b->scriptRules.size() << "\n";
        std::string nm = b->name; for (char& c : nm) if (c == '\n' || c == '\r') c = ' ';
        // ParseAIScript ha bisogno delle righe: le salviamo separate da \x01 e le ripristiniamo al caricamento
        std::string enc; { std::istringstream is(b->customPrompt); std::string l; while (std::getline(is, l)) { while (!l.empty() && l.back() == '\r') l.pop_back(); enc += l + "\x01"; } }
        f << nm << "\n" << enc << "\n" << SerializeScriptRules(b->scriptRules);
    }
}

// ============================================================================
//  Init / sprite
// ============================================================================
void PallaGame::Init(const std::string& dataDir) {
    saveFile = dataDir + "palla_vs_palla_save.txt"; customFile = dataDir + "palla_vs_palla_custom.txt"; keyFile = dataDir + "palla_gemini_key.txt";
    audio = Audio_Init(); ui.Init(); GfxInit();
    previewTex = LoadRenderTexture(640, 480); SetTextureFilter(previewTex.texture, TEXTURE_FILTER_BILINEAR);
    LoadKey(); LoadCustomBalls(); LoadSave(); modalKey = geminiApiKey;
    const auto& db = GetBallCatalog();
    demoMatch.Init({ { &db[0], nullptr } }, { { &db[1], nullptr } }, true, false, 0.4f, 0, 0, 1.0f, 0.0f);
    for (const auto& b : db) BuildSprite(&b);
}
void PallaGame::Shutdown() {
    if (aiThread.joinable()) aiThread.join();
    for (auto& kv : sprites) UnloadRenderTexture(kv.second);
    sprites.clear(); if (previewTex.id > 0) UnloadRenderTexture(previewTex);
    ui.Unload(); Audio_Unload(&audio);
}
void PallaGame::BuildSprite(const BallDef* def) {
    if (!def || sprites.count(def->id)) return;
    RenderTexture2D rt = LoadRenderTexture(256, 256); SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
    RenderBallSprite(def, rt, 0.9f, false); sprites[def->id] = rt;
}
const RenderTexture2D* PallaGame::GetSprite(const BallDef* def) const { auto it = sprites.find(def->id); return it == sprites.end() ? nullptr : &it->second; }
Camera3D PallaGame::ArenaCam(float shake) const { return MakeArenaCamera(shake); }

// ============================================================================
//  Flusso partita
// ============================================================================
void PallaGame::Go(GameState s) { state = s; SND(SFX_CLICK); }
void PallaGame::RollDraftThree() {
    std::vector<const BallDef*> all; for (const auto& b : GetBallCatalog()) all.push_back(&b);
    for (size_t i = all.size(); i > 1; i--) std::swap(all[i - 1], all[rand() % i]);
    draftChoices.assign(all.begin(), all.begin() + std::min<size_t>(3, all.size()));
}
void PallaGame::StartNewMatch() { allyHearts = 3; enemyHearts = 3; roundNum = 1; upgHp = 0; upgDmg = 0; upgSpd = 1.0f; upgRegen = 0.0f; PrepareNextRound(); }
void PallaGame::PrepareNextRound() {
    selectedPrimary = nullptr; selectedSecondary = nullptr; pickingSecondFusion = false; rpsPlayer = -1; rpsEnemy = -1; rpsOutcome = 0; rpsShowTimer = 0;
    const auto& db = GetBallCatalog(); auto pickE = [&]() -> const BallDef* { return &db[rand() % db.size()]; };
    roundEnemies.clear();
    if (mode == GameMode::MODE_2V2) { roundEnemies.push_back({ pickE(), nullptr }); roundEnemies.push_back({ pickE(), nullptr }); roundAlly = &db[rand() % db.size()]; }
    else if (mode == GameMode::MODE_FUSION) { roundEnemies.push_back({ pickE(), pickE() }); roundAlly = nullptr; }
    else { roundEnemies.push_back({ pickE(), nullptr }); roundAlly = nullptr; }
    RollDraftThree(); state = (mode == GameMode::MODE_RPS) ? GameState::RPS : GameState::DRAFT; SND(SFX_CLICK);
}
void PallaGame::BeginAimPhase() {
    aimAngle = 0.0f; aimTimer = 8.0f;
    std::vector<std::pair<const BallDef*, const BallDef*>> allies = { { selectedPrimary, selectedSecondary } };
    if (roundAlly) allies.push_back({ roundAlly, nullptr });
    activeMatch.Init(allies, roundEnemies, false, mode == GameMode::MODE_RANKED, aimAngle, upgHp + (rpsOutcome == 1 ? 25 : 0), upgDmg + (rpsOutcome == 1 ? 2 : 0), upgSpd, upgRegen);
    state = GameState::AIM;
}
void PallaGame::LaunchRound() {
    std::vector<std::pair<const BallDef*, const BallDef*>> allies = { { selectedPrimary, selectedSecondary } };
    if (roundAlly) allies.push_back({ roundAlly, nullptr });
    activeMatch.Init(allies, roundEnemies, false, mode == GameMode::MODE_RANKED, aimAngle, upgHp + (rpsOutcome == 1 ? 25 : 0), upgDmg + (rpsOutcome == 1 ? 2 : 0), upgSpd, upgRegen);
    state = GameState::ARENA;
}
void PallaGame::OnRoundEnded() {
    lastRoundWinner = activeMatch.winnerTeam; if (lastRoundWinner == 0) enemyHearts--; else allyHearts--;
    if (allyHearts <= 0 || enemyHearts <= 0) FinishMatch(enemyHearts <= 0);
    else { roundNum++; roundBannerTimer = 1.7f; state = GameState::ROUND_END; SND(lastRoundWinner == 0 ? SFX_WIN : SFX_KO); }
}
void PallaGame::FinishMatch(bool won) {
    int xp = won ? 100 : 50, rpD = 0;
    if (won) { save.wins++; save.streak++; if (mode == GameMode::MODE_RANKED) rpD = 30; SND(SFX_WIN); }
    else { save.streak = 0; if (mode == GameMode::MODE_RANKED) rpD = -15; SND(SFX_KO); }
    save.rp = std::max(0, save.rp + rpD); save.xp += xp; bool lvlUp = false;
    while (save.xp >= save.level * 100) { save.xp -= save.level * 100; save.level++; lvlUp = true; }
    WriteSave(); resultInfo = { won, xp, rpD, lvlUp, activeMatch.time }; state = GameState::RESULT;
}

// ============================================================================
//  Studio AI: thread, risposte, salvataggio palla
// ============================================================================
void PallaGame::OpenAiChatStudio() {
    draftCustom = BallDef{}; draftCustom.name = "Nuova Palla"; draftCustom.rarity = Rarity::Epico; draftCustom.color = { 34, 211, 238, 255 };
    draftCustom.hp = 120; draftCustom.dmg = 8; draftCustom.speedMult = 1.0f; draftCustom.baseRadius = 1.15f; draftCustom.visualStyle = "none"; draftCustom.isCustom = true;
    chatHistory.clear();
    ChatTurn g; g.isUser = false; g.text = "Ciao! Descrivimi la palla che vuoi: buchi neri, stop del tempo, laser rimbalzanti, ruba-abilità... La creo e la vedi subito in 3D.";
    chatHistory.push_back(g); chatInput.clear(); chatStick = true; nativeTarget = 0;
    state = GameState::AI_CHAT; SND(SFX_CLICK);
    if (geminiApiKey.empty()) { keyModal = true; modalKey.clear(); modalError.clear(); modalOffer = false; }
}

void PallaGame::StartAiJob(AiJob kind) {
    if (aiBusy.load()) return;
    if (aiThread.joinable()) aiThread.join();
    aiBusy.store(true); aiBusyTime = 0.0f;
    std::string key = (kind == AiJob::VERIFY) ? modalKey : geminiApiKey, cur = draftCustom.customPrompt; std::vector<ChatTurn> hist = chatHistory;
    aiThread = std::thread([this, kind, key, hist, cur]() {
        AiResult r;
        try { r = (kind == AiJob::VERIFY) ? GeminiCheckKey(key) : GeminiGenerate(key, hist, cur); }
        catch (...) { r.ok = false; r.error = "Errore interno del client AI."; }
        { std::lock_guard<std::mutex> lk(aiMutex); aiResult = r; aiJobKind = kind; aiReplyReady = true; }
        aiBusy.store(false);
    });
}

void PallaGame::SendAiChatMessage() {
    std::string msg = chatInput; while (!msg.empty() && (msg.back() == ' ' || msg.back() == '\n')) msg.pop_back();
    if (msg.empty() || aiBusy.load()) return;
    if (geminiApiKey.empty()) { keyModal = true; modalKey.clear(); modalError.clear(); return; }
    ChatTurn u; u.isUser = true; u.text = msg; chatHistory.push_back(u); chatInput.clear(); chatStick = true;
    StartAiJob(AiJob::CHAT);
}

void PallaGame::HandleAiResult(const AiResult& r, AiJob kind) {
    if (kind == AiJob::VERIFY) {
        if (r.ok) { geminiApiKey = SanitizeApiKey(modalKey); WriteKey(); keyModal = false; modalError.clear(); aiModel = r.model; SetToast("API Key valida! Modello: " + r.model); SND(SFX_WIN); }
        else { modalError = r.error; modalOffer = !r.keyProblem; SND(SFX_KO); }
        return;
    }
    if (!r.ok) {
        ChatTurn e; e.isUser = false; e.isError = true; e.text = r.error; chatHistory.push_back(e); chatStick = true; SND(SFX_KO);
        if (r.keyProblem) { keyModal = true; modalKey = geminiApiKey; modalError = r.error; modalOffer = false; }
        return;
    }
    aiModel = r.model;
    ParsedBallScript ps = ParseAIScript(r.text);
    if (!ps.ok) {
        ChatTurn e; e.isUser = false; e.isError = true; e.text = "Gemini ha risposto in un formato che non riesco a leggere. Riprova descrivendo la palla con altre parole.";
        chatHistory.push_back(e); chatStick = true; SND(SFX_KO); return;
    }
    if (!ps.name.empty()) draftCustom.name = ps.name;
    draftCustom.rarity = ps.rarity; draftCustom.hp = ps.hp; draftCustom.dmg = ps.dmg; draftCustom.speedMult = ps.speed; draftCustom.baseRadius = ps.size;
    draftCustom.color = ps.color; draftCustom.visualStyle = ps.style; draftCustom.scriptRules = ps.rules; draftCustom.isCustom = true; draftCustom.customPrompt = r.text;
    std::string say = ps.chatMsg.empty() ? ("Creata '" + draftCustom.name + "' con " + std::to_string(ps.rules.size()) + " regole!") : ps.chatMsg;
    if (!ps.warnings.empty()) { say += "  [Attenzione: " + std::to_string(ps.warnings.size()) + " regola/e ignorata/e: " + ps.warnings[0] + ". Chiedimi di correggere.]"; }
    draftCustom.desc = say;
    ChatTurn m; m.isUser = false; m.text = r.text; m.shown = say; chatHistory.push_back(m); chatStick = true; SND(SFX_WIN);
}

void PallaGame::SaveCustomBall(bool testImmediately) {
    if (draftCustom.scriptRules.empty()) return;
    if (draftCustom.name.empty()) draftCustom.name = "Palla Custom";
    BallDef b = draftCustom; b.isCustom = true;
    int n = 0; for (const auto& x : GetBallCatalog()) if (x.isCustom) n++;
    b.id = "custom_" + std::to_string(n + 1) + "_" + std::to_string((int)GetBallCatalog().size());
    if (b.desc.empty()) b.desc = "Palla creata con l'AI (" + std::to_string(b.scriptRules.size()) + " regole)";
    GetBallCatalog().push_back(b); save.customIds.push_back(b.id); WriteCustomBalls(); WriteSave();
    spriteQueue.push_back(&GetBallCatalog().back());
    if (testImmediately) {
        allyHearts = 3; enemyHearts = 3; roundNum = 1; selectedPrimary = &GetBallCatalog().back(); selectedSecondary = nullptr; roundAlly = nullptr; rpsOutcome = 0;
        std::vector<const BallDef*> builtin; for (const auto& x : GetBallCatalog()) if (!x.isCustom) builtin.push_back(&x);
        roundEnemies = { { builtin[rand() % builtin.size()], nullptr } }; mode = GameMode::MODE_1V1; BeginAimPhase();
    } else { SetToast("Palla salvata nell'elenco!"); state = GameState::COLLECTION; collScroll = 1e6f; }
    SND(SFX_WIN);
}

// Testo digitato (UTF-8) + backspace + incolla. Su Android arriva dalla tastiera nativa.
void PallaGame::AppendTyped(std::string& t, size_t maxLen, bool singleLine) {
    const InputState& in = ui.in;
    for (int i = 0; i < in.backspaces && !t.empty(); i++) { size_t k = t.size() - 1; while (k > 0 && ((unsigned char)t[k] & 0xC0) == 0x80) k--; t.erase(k); }
    auto add = [&](const std::string& s) { for (char c : s) { if ((unsigned char)c < 32 && c != '\n') { if (c == '\t') t.push_back(' '); continue; } if (c == '\n' && singleLine) { t.push_back(' '); continue; } if (t.size() < maxLen) t.push_back(c); } };
    if (!in.typed.empty()) add(in.typed);
    if (in.ctrlV) { const char* cl = GetClipboardText(); if (cl) add(cl); }
}

// ============================================================================
//  Logica per frame
// ============================================================================
void PallaGame::HandleBack() {
    if (keyModal) { keyModal = false; return; }
    switch (state) {
        case GameState::AI_CHAT: Go(GameState::COLLECTION); break;
        case GameState::COLLECTION: case GameState::DRAFT: case GameState::RPS: case GameState::AIM: case GameState::ARENA: case GameState::RESULT: Go(GameState::LOBBY); break;
        default: break;
    }
}

void PallaGame::Logic(float dt) {
    const InputState& in = ui.in;
    if (toastTimer > 0.0f) toastTimer -= dt;
    if (aiBusy.load()) aiBusyTime += dt;
    AiResult r; AiJob k = AiJob::NONE; bool got = false;
    { std::lock_guard<std::mutex> lk(aiMutex); if (aiReplyReady) { aiReplyReady = false; r = aiResult; k = aiJobKind; got = true; } }
    if (got) HandleAiResult(r, k);

    if (in.nativeTextReady) {
        if (nativeTarget == 1) { chatInput = in.nativeText; SendAiChatMessage(); }
        else if (nativeTarget == 2) { modalKey = in.nativeText; modalError.clear(); }
        nativeTarget = 0;
    }
    if (keyModal) { AppendTyped(modalKey, 220, true); if (in.enter && !aiBusy.load() && !modalKey.empty()) { modalError.clear(); StartAiJob(AiJob::VERIFY); } }
    else if (state == GameState::AI_CHAT) { AppendTyped(chatInput, 400, true); if (in.enter) SendAiChatMessage(); }
    if (in.back) HandleBack();

    if (state == GameState::AIM) {
        aimTimer -= dt;
        Rectangle lr = { W - 250.0f, H - 92.0f, 230.0f, 70.0f };
        if (in.down && !ui.Inside(lr, in.pressPos) && in.pressPos.y > 60.0f && !activeMatch.fighters.empty()) {
            Vector3 bp = { activeMatch.fighters[0].pos.x, activeMatch.fighters[0].r, activeMatch.fighters[0].pos.y };
            Vector2 sp = GetWorldToScreen(bp, curCam); sp.x /= ui.scale; sp.y /= ui.scale;
            float dx = in.pos.x - sp.x, dy = in.pos.y - sp.y;
            if (dx * dx + dy * dy > 16.0f) { aimAngle = atan2f(dy, dx); activeMatch.fighters[0].look = aimAngle; }
        }
        if (aimTimer <= 0.0f) { LaunchRound(); return; }
    }
    if (state == GameState::ROUND_END) {
        roundBannerTimer -= dt;
        if (roundBannerTimer <= 0.0f) {
            if (mode == GameMode::MODE_TOURNAMENT) { upgOptions[0] = rand() % 4; upgOptions[1] = (upgOptions[0] + 1) % 4; upgOptions[2] = (upgOptions[0] + 2) % 4; state = GameState::UPGRADE; }
            else PrepareNextRound();
        }
        return;
    }
    if (state == GameState::RPS && rpsPlayer >= 0) { rpsShowTimer -= dt; if (rpsShowTimer <= 0.0f) state = GameState::DRAFT; return; }
    if (state == GameState::LOBBY) demoMatch.Update(dt, &audio, false);
    if (state == GameState::ARENA) {
        for (int s = 0; s < simSpeed; s++) if (!activeMatch.ended) activeMatch.Update(dt, &audio, soundOn);
        if (activeMatch.ended) OnRoundEnded();
    }
}

// ============================================================================
//  Frame: 2D (sfondo) -> 3D (arena) -> 2D (interfaccia)
// ============================================================================
void PallaGame::Frame(float dt, const InputState& in, float virtW, float uiScale) {
    W = virtW; H = (float)GetScreenHeight() / uiScale; ui.Begin(in, virtW, uiScale); ui.H = H; globalTime += dt;
    curCam = MakeArenaCamera((state == GameState::ARENA || state == GameState::ROUND_END) ? activeMatch.shake : 0.0f);
    Logic(dt);

    // render-to-texture: sempre fuori da qualsiasi Begin/EndMode2D
    while (!spriteQueue.empty()) { BuildSprite(spriteQueue.back()); spriteQueue.pop_back(); }
    if (state == GameState::AI_CHAT) RenderBallSprite(&draftCustom, previewTex, globalTime * 0.8f, true);

    ClearBackground(Pal::Bg0);
    Camera2D c2 = { { 0, 0 }, { 0, 0 }, 0.0f, uiScale };
    BeginMode2D(c2); DrawBackdrop(ui, globalTime); EndMode2D();

    if (state == GameState::LOBBY) RenderArena3D(demoMatch, curCam, globalTime);
    else if (state == GameState::AIM && !activeMatch.fighters.empty()) {
        AimInfo ai = { activeMatch.fighters[0].pos, aimAngle, globalTime };
        RenderArena3D(activeMatch, curCam, globalTime, &ai);
    } else if (state == GameState::ARENA || state == GameState::ROUND_END) RenderArena3D(activeMatch, curCam, globalTime);

    BeginMode2D(c2);
    if (keyModal) ui.enabled = false;
    switch (state) {
        case GameState::LOBBY: ScreenLobby(); break;
        case GameState::COLLECTION: ScreenCollection(); break;
        case GameState::AI_CHAT: ScreenAiChat(); break;
        case GameState::RPS: ScreenRps(); break;
        case GameState::DRAFT: case GameState::ALL_PICK: ScreenDraft(); break;
        case GameState::AIM: ScreenAim(); break;
        case GameState::ARENA: case GameState::ROUND_END: ScreenArena(); break;
        case GameState::UPGRADE: ScreenUpgrade(); break;
        case GameState::RESULT: ScreenResult(); break;
    }
    Toast();
    if (keyModal) KeyModal();
    EndMode2D();
}

// ============================================================================
//  Componenti comuni
// ============================================================================
static const char* kModeName[6] = { "1 vs 1 Classico", "2 vs 2 Squadre", "Fusion Mode", "1v1 Classificata", "Torneo Upgrade", "Sasso Carta Forbice" };
static const char* kModeDesc[6] = { "Duello 3D in arena quadrata. Vince chi vince 2 round su 3.", "Tu e un alleato contro due nemici. Lavoro di squadra!", "Fondi due palle in una sola e scatena entrambe le abilità.",
                                    "Scala la classifica: +30 punti per vittoria, -15 per sconfitta.", "Dopo ogni round scegli un potenziamento permanente.", "Vinci il mini-duello e parti con +25 HP e +2 danno." };
static const char* kModeTag[6] = { "1v1", "2v2", "FUSION", "RANK", "UPGRADE", "RPS" };
static Color ModeAccent(int i) { const Color a[6] = { Pal::Cyan, Pal::Blue, Pal::Purple, Pal::Gold, Pal::Orange, Pal::Pink }; return a[i]; }

void PallaGame::TopBar(const char*, bool) {
    DrawRectangle(0, 0, (int)std::ceil(W), 60, { 8, 12, 24, 232 }); DrawRectangle(0, 59, (int)std::ceil(W), 1, { 255, 255, 255, 26 });
    ui.Orb(34, 30, 15, Pal::Cyan);
    ui.Text("PALLA VS PALLA", 58, 12, 27, Pal::Text, true);
    float tw = ui.Measure("PALLA VS PALLA", 27, true).x; ui.Pill({ 58 + tw + 10, 19, 42, 23 }, "3D", Pal::Cyan, { 8, 22, 34, 255 }, 13);
    if (ui.Button({ W - 124.0f, 10, 110, 40 }, soundOn ? "Audio ON" : "Audio OFF", Pal::PanelHi, Pal::Text, 15)) { soundOn = !soundOn; SND(SFX_CLICK); }
    float lx = W - 124.0f - 14.0f - 170.0f; ui.Panel({ lx, 12, 170, 36 }, 12, { 16, 22, 40, 235 }, Pal::Border, 1.2f, false);
    char lv[24]; std::snprintf(lv, sizeof(lv), "LV %d", save.level); ui.Text(lv, lx + 12, 18, 16, Pal::Gold, true);
    ui.Bar({ lx + 62, 25, 96, 10 }, (float)save.xp / (float)std::max(1, save.level * 100), Pal::Gold, { 40, 48, 72, 255 });
}

void PallaGame::Rail() {
    Rectangle r = { 14, 74, 204, H - 88 }; ui.Panel(r, 18, { 12, 18, 34, 232 }, Pal::Border, 1.2f);
    bool inLobby = state == GameState::LOBBY;
    if (ui.Button({ 24, 86, 184, 50 }, "BATTAGLIA", inLobby ? Pal::Blue : Pal::PanelHi, Pal::Text, 17)) { if (!inLobby) Go(GameState::LOBBY); }
    char nb[40]; std::snprintf(nb, sizeof(nb), "PALLE (%d)", (int)GetBallCatalog().size());
    if (ui.Button({ 24, 144, 184, 50 }, nb, !inLobby ? Pal::Blue : Pal::PanelHi, Pal::Text, 17)) { if (inLobby) { Go(GameState::COLLECTION); collScroll = 0; } }
    Rectangle p = { 24, r.y + r.height - 150, 184, 138 }; ui.Panel(p, 14, { 20, 28, 50, 255 }, { 60, 78, 120, 120 }, 1.0f, false);
    ui.Text("PROFILO", p.x + 14, p.y + 10, 13, Pal::Muted, true);
    char b[48];
    std::snprintf(b, sizeof(b), "Vittorie  %d", save.wins); ui.Text(b, p.x + 14, p.y + 34, 16, Pal::Text);
    std::snprintf(b, sizeof(b), "Serie  %d", save.streak); ui.Text(b, p.x + 14, p.y + 62, 16, Pal::Text);
    std::snprintf(b, sizeof(b), "Punti RP  %d", save.rp); ui.Text(b, p.x + 14, p.y + 90, 16, Pal::Gold, true);
}

void PallaGame::Toast() {
    if (toastTimer <= 0.0f || toastMsg.empty()) return;
    float a = Clamp01(toastTimer * 2.0f); Vector2 m = ui.Measure(toastMsg.c_str(), 16, true); float w = std::min(W - 40.0f, m.x + 44.0f);
    Rectangle r = { W * 0.5f - w * 0.5f, H - 78.0f - (1.0f - a) * 10.0f, w, 44 };
    ui.Panel(r, 22, A({ 18, 26, 48, 255 }, a), A(Pal::Cyan, a * 0.8f), 1.5f, false);
    ui.TextC(ui.Fit(toastMsg, w - 30, 16, true).c_str(), r.x + w * 0.5f, r.y + 22, 16, A(Pal::Text, a), true);
}

void PallaGame::Hearts(float x, float y, int n, bool right) {
    for (int i = 0; i < 3; i++) { float cx = right ? x - 10 - i * 28.0f : x + 10 + i * 28.0f; ui.Heart(cx, y, 9.5f, i < n ? Pal::Red : Color{ 52, 60, 88, 255 }); }
}

void PallaGame::Hud() {
    Rectangle l = { 14, 12, 238, 56 }; ui.Panel(l, 16, { 10, 16, 32, 218 }, A(Pal::Green, 0.55f), 1.5f);
    ui.Text("TU", l.x + 16, l.y + 8, 13, Pal::Green, true);
    if (selectedPrimary) ui.TextR(ui.Fit(selectedPrimary->name, 150, 14).c_str(), l.x + l.width - 14, l.y + 8, 14, Pal::Muted);
    Hearts(l.x + 18, l.y + 38, allyHearts, false);
    Rectangle rr = { W - 252.0f, 12, 238, 56 }; ui.Panel(rr, 16, { 10, 16, 32, 218 }, A(Pal::Red, 0.55f), 1.5f);
    ui.TextR("NEMICO", rr.x + rr.width - 16, rr.y + 8, 13, Pal::Red, true);
    if (!roundEnemies.empty() && roundEnemies[0].first) ui.Text(ui.Fit(roundEnemies[0].first->name, 150, 14).c_str(), rr.x + 14, rr.y + 8, 14, Pal::Muted);
    Hearts(rr.x + rr.width - 18, rr.y + 38, enemyHearts, true);
    char rb[24]; std::snprintf(rb, sizeof(rb), "ROUND %d", roundNum);
    ui.Panel({ W * 0.5f - 85, 12, 170, 40 }, 20, { 10, 16, 32, 218 }, Pal::Border, 1.2f);
    ui.TextC(rb, W * 0.5f, 32, 18, Pal::Text, true);
}

void PallaGame::StatBars(const BallDef* d, Rectangle r) {
    struct Row { const char* n; float v, mx; Color c; char t[16]; } rows[3] = {
        { "HP", (float)d->hp, 400.0f, Pal::Red, "" }, { "DANNO", (float)d->dmg, 40.0f, Pal::Orange, "" }, { "VELOCITÀ", d->speedMult, 2.0f, Pal::Cyan, "" } };
    std::snprintf(rows[0].t, 16, "%d", d->hp); std::snprintf(rows[1].t, 16, "%d", d->dmg); std::snprintf(rows[2].t, 16, "%.2f", d->speedMult);
    for (int i = 0; i < 3; i++) {
        float y = r.y + i * (r.height / 3.0f);
        ui.Text(rows[i].n, r.x, y, 12.5f, Pal::Muted, true); ui.TextR(rows[i].t, r.x + r.width, y - 1, 14, Pal::Text, true);
        ui.Bar({ r.x, y + 18, r.width, 8 }, rows[i].v / rows[i].mx, rows[i].c, { 36, 44, 70, 255 });
    }
}

void PallaGame::BallCard(const BallDef* def, Rectangle r, bool hl, bool showDesc) {
    Color rc = RarityColor(def->rarity); bool held = ui.Held(r);
    Rectangle q = r; if (held) { q.y += 2; }
    ui.Panel(q, 16, held ? Color{ 28, 38, 66, 255 } : Color{ 20, 28, 50, 250 }, hl ? rc : A(rc, 0.55f), hl ? 3.0f : 1.6f);
    DrawRectangleRounded({ q.x + 3, q.y + 3, q.width - 6, std::min(96.0f, q.height * 0.42f) }, 0.25f, 10, A(rc, 0.16f));
    float s = std::min(q.width - 24.0f, std::min(120.0f, q.height * 0.5f)); Rectangle sp = { q.x + q.width * 0.5f - s * 0.5f, q.y + 6, s, s };
    const RenderTexture2D* rt = GetSprite(def);
    if (rt) DrawSprite(*rt, sp); else ui.Orb(sp.x + s * 0.5f, sp.y + s * 0.5f, s * 0.3f, def->color);
    const char* rn = RarityName(def->rarity); float pw = ui.Measure(rn, 11.5f, true).x + 18;
    ui.Pill({ q.x + 10, q.y + 10, pw, 20 }, rn, rc, (def->rarity == Rarity::Ultra || def->rarity == Rarity::Leggendario || def->rarity == Rarity::Raro) ? Color{ 12, 16, 28, 255 } : WHITE, 11.5f);
    if (def->isCustom) ui.Pill({ q.x + q.width - 40, q.y + 10, 30, 20 }, "AI", Pal::Cyan, { 8, 22, 34, 255 }, 11.5f);
    float ny = q.y + 6 + s + 6;
    ui.TextC(ui.Fit(def->name, q.width - 20, 18, true).c_str(), q.x + q.width * 0.5f, ny + 9, 18, Pal::Text, true);
    char st[40]; std::snprintf(st, sizeof(st), "HP %d    DMG %d", def->hp, def->dmg);
    ui.TextC(st, q.x + q.width * 0.5f, ny + 32, 14, { 150, 190, 240, 255 }, true);
    if (showDesc && q.y + q.height - (ny + 48) > 20) ui.Wrap(def->desc, q.x + 14, ny + 50, q.width - 28, 12.5f, Pal::Muted, true, false, std::max(1, (int)((q.y + q.height - ny - 56) / 16.0f)));
}

// ============================================================================
//  LOBBY
// ============================================================================
void PallaGame::ScreenLobby() {
    DrawRectangleGradientH(0, 0, (int)std::ceil(W), (int)std::ceil(H), { 5, 8, 18, 238 }, { 5, 8, 18, 120 });
    TopBar("", true); Rail();
    float cx = 236, cw = W - cx - 16;
    ui.Text("MODALITÀ DI GIOCO", cx, 76, 24, Pal::Text, true);
    ui.Text("3 Cuori, Best-of-3 e Fase di Mira prima di ogni round.", cx, 106, 14, Pal::Muted);
    float gap = 14, mw = (cw - gap * 2) / 3.0f, mh = 124, top = 134;
    for (int i = 0; i < 6; i++) {
        Rectangle r = { cx + (i % 3) * (mw + gap), top + (i / 3) * (mh + gap), mw, mh }; Color ac = ModeAccent(i); bool sel = ((int)mode == i), held = ui.Held(r);
        Rectangle q = r; if (held) q.y += 2;
        ui.Panel(q, 18, sel ? Mix({ 18, 26, 48, 250 }, ac, 0.16f) : Color{ 16, 23, 42, 238 }, sel ? ac : Pal::Border, sel ? 3.0f : 1.3f);
        DrawRectangleRounded({ q.x + 14, q.y + 14, 5, q.height - 28 }, 1.0f, 6, ac);
        Vector2 gm = ui.Measure(kModeTag[i], 40, true); ui.Text(kModeTag[i], q.x + q.width - gm.x - 16, q.y + 8, 40, A(ac, sel ? 0.34f : 0.16f), true);
        ui.Text(kModeName[i], q.x + 32, q.y + 16, 20, Pal::Text, true);
        ui.Wrap(kModeDesc[i], q.x + 32, q.y + 46, q.width - 48, 13.5f, Pal::Muted, true, false, 3);
        if (sel) ui.Pill({ q.x + 32, q.y + q.height - 30, 104, 20 }, "SELEZIONATA", ac, { 10, 14, 26, 255 }, 11.5f);
        if (ui.Click(r)) { mode = (GameMode)i; SND(SFX_CLICK); }
    }
    float py = top + (mh + gap) * 2 + 16;
    if (ui.Button({ cx + cw * 0.5f - 200, py, 400, 76 }, "GIOCA", Pal::Green, { 6, 30, 18, 255 }, 34)) StartNewMatch();
    ui.TextC((std::string("Modalità: ") + kModeName[(int)mode]).c_str(), cx + cw * 0.5f, py + 98, 15, Pal::Muted, false);
}

// ============================================================================
//  ELENCO PALLE
// ============================================================================
void PallaGame::ScreenCollection() {
    DrawRectangleGradientH(0, 0, (int)std::ceil(W), (int)std::ceil(H), { 5, 8, 18, 150 }, { 5, 8, 18, 60 });
    TopBar("", true); Rail();
    float cx = 236, cw = W - cx - 16;
    ui.Text("ELENCO PALLE", cx, 74, 24, Pal::Text, true);
    char cnt[40]; std::snprintf(cnt, sizeof(cnt), "%d palle sbloccate", (int)GetBallCatalog().size()); ui.Text(cnt, cx + 190, 82, 14, Pal::Muted);
    // filtri rarità
    const char* names[7] = { "TUTTE", RarityName(Rarity::Raro), RarityName(Rarity::SuperRaro), RarityName(Rarity::Epico), RarityName(Rarity::Mitico), RarityName(Rarity::Leggendario), RarityName(Rarity::Ultra) };
    float fx = cx, fy = 108; 
    for (int i = 0; i < 7; i++) {
        float w = ui.Measure(names[i], 13, true).x + 26; Color c = (i == 0) ? Pal::Cyan : RarityColor((Rarity)(i - 1));
        if (ui.Chip({ fx, fy, w, 30 }, names[i], c, rarFilter == i - 1, 13)) { rarFilter = i - 1; collScroll = 0; SND(SFX_CLICK); }
        fx += w + 8;
    }
    Rectangle area = { cx, 148, cw, H - 148 - 10 };
    int cols = std::max(2, (int)((cw + 14) / 214.0f)); float gap = 14, cwid = (cw - gap * (cols - 1)) / cols, ch = 240;
    std::vector<const BallDef*> items; for (const auto& b : GetBallCatalog()) if (rarFilter < 0 || (int)b.rarity == rarFilter) items.push_back(&b);
    int total = (int)items.size() + 1; int rows = (total + cols - 1) / cols; float contentH = rows * (ch + gap) + 8, maxScroll = std::max(0.0f, contentH - area.height);
    const InputState& in = ui.in;
    if (ui.enabled && ui.Inside(area, in.pos)) collScroll -= in.wheel * 60.0f;
    if (ui.enabled && in.down && ui.Inside(area, in.pressPos)) collScroll -= in.dragDy;
    collScroll = std::max(0.0f, std::min(maxScroll, collScroll));
    ui.ScissorBegin(area);
    for (int i = 0; i < total; i++) {
        Rectangle r = { area.x + (i % cols) * (cwid + gap), area.y + (i / cols) * (ch + gap) - collScroll + 4, cwid, ch };
        if (r.y > area.y + area.height || r.y + r.height < area.y) continue;
        bool visible = ui.Inside(area, in.pos);
        if (i == 0) {
            bool held = ui.Held(r) && visible; Rectangle q = r; if (held) q.y += 2;
            ui.Panel(q, 16, { 14, 26, 46, 250 }, A(Pal::Cyan, 0.8f), 2.2f);
            DrawRectangleRounded({ q.x + 3, q.y + 3, q.width - 6, 100 }, 0.25f, 10, A(Pal::Cyan, 0.12f));
            float px = q.x + q.width * 0.5f, py = q.y + 74; DrawCircleV({ px, py }, 38, A(Pal::Cyan, 0.18f));
            DrawRectangleRounded({ px - 3.5f, py - 20, 7, 40 }, 1.0f, 6, Pal::Cyan); DrawRectangleRounded({ px - 20, py - 3.5f, 40, 7 }, 1.0f, 6, Pal::Cyan);
            ui.TextC("CREA CON AI", px, q.y + 148, 19, Pal::Text, true);
            ui.Wrap("Descrivi una palla a Gemini e la vedi subito in 3D.", q.x + 16, q.y + 172, q.width - 32, 13, Pal::Muted, true, false, 3);
            if (ui.Click(r) && visible) { OpenAiChatStudio(); break; }
        } else {
            const BallDef* d = items[i - 1]; BallCard(d, r, false, true);
        }
    }
    ui.ScissorEnd();
    if (collScroll > 1e5f) collScroll = maxScroll;
}

// ============================================================================
//  STUDIO AI
// ============================================================================
void PallaGame::ScreenAiChat() {
    DrawRectangle(0, 0, (int)std::ceil(W), 58, { 8, 12, 24, 235 }); DrawRectangle(0, 57, (int)std::ceil(W), 1, { 255, 255, 255, 26 });
    if (ui.Button({ 14, 9, 118, 40 }, "Indietro", Pal::PanelHi, Pal::Text, 16)) { Go(GameState::COLLECTION); return; }
    ui.Text("AI STUDIO", 148, 9, 26, Pal::Text, true);
    ui.Text(aiModel.empty() ? "creatore di palle con Gemini" : ("Gemini  -  " + aiModel).c_str(), 150, 36, 12.5f, Pal::Muted);
    bool hasKey = !geminiApiKey.empty();
    if (ui.Button({ W - 354.0f, 9, 196, 40 }, hasKey ? "API Key: OK (cambia)" : "IMPOSTA API KEY", hasKey ? Color{ 28, 92, 62, 255 } : Pal::Orange, hasKey ? Pal::Text : Color{ 40, 20, 4, 255 }, 15)) { keyModal = true; modalKey = geminiApiKey; modalError.clear(); modalOffer = false; }
    if (ui.Button({ W - 150.0f, 9, 136, 40 }, "Ottieni key", Pal::Blue, WHITE, 15)) OpenURL("https://aistudio.google.com/apikey");

    float rw = 340, gap = 14; Rectangle chat = { 14, 68, W - 28 - rw - gap, H - 80 }, side = { W - 14 - rw, 68, rw, H - 80 };
    ui.Panel(chat, 18, { 12, 18, 34, 236 }, Pal::Border, 1.2f);
    // --- barra di input ---
    Rectangle inp = { chat.x + 12, chat.y + chat.height - 12 - 50, chat.width - 24 - 112, 50 }, sendR = { inp.x + inp.width + 10, inp.y, 102, 50 };
    bool busy = aiBusy.load();
    ui.Panel(inp, 14, { 22, 30, 54, 255 }, ui.Held(inp) ? Pal::Cyan : Pal::Border, 1.5f, false);
    std::string shownIn = ui.Fit(chatInput, inp.width - 28, 17);
    if (chatInput.empty()) ui.Text("Descrivi cosa deve fare la palla...", inp.x + 14, inp.y + 14, 17, Pal::Dim);
    else ui.Text(shownIn.c_str(), inp.x + 14, inp.y + 14, 17, Pal::Text);
#ifndef __ANDROID__
    if (!busy && !keyModal && ((int)(globalTime * 2.0f) & 1) == 0) { float tw = chatInput.empty() ? 0.0f : ui.Measure(shownIn.c_str(), 17).x; DrawRectangle((int)(inp.x + 15 + tw), (int)(inp.y + 12), 2, 26, Pal::Cyan); }
#endif
    if (ui.Click(inp) && !busy) { nativeTarget = 1; OpenNativeInputDialog("Cosa deve fare la palla?", chatInput); }
    if (ui.Button(sendR, busy ? "..." : "INVIA", Pal::Cyan, { 6, 24, 34, 255 }, 18, busy || chatInput.empty())) SendAiChatMessage();
    // --- suggerimenti ---
    float chipsY = inp.y - 10 - 30, chipX = chat.x + 12;
    static const char* chipLabel[4] = { "Buco nero", "Ferma il tempo", "Laser rimbalzanti", "Ruba abilità" };
    static const char* chipText[4] = { "Una palla che apre un buco nero sul nemico ogni 8 secondi", "Una palla che ferma il tempo per 2 secondi quando ha poca vita",
                                       "Una palla che spara 3 laser che rimbalzano sui muri", "Una palla che ruba l'abilità del nemico e si cura" };
    for (int i = 0; i < 4; i++) {
        float w = ui.Measure(chipLabel[i], 13, true).x + 26; if (chipX + w > chat.x + chat.width - 12) break;
        if (ui.Chip({ chipX, chipsY, w, 30 }, chipLabel[i], ModeAccent(i), false, 13) && !busy) { chatInput = chipText[i]; SendAiChatMessage(); }
        chipX += w + 8;
    }
    // --- messaggi ---
    Rectangle area = { chat.x + 12, chat.y + 12, chat.width - 24, chipsY - 10 - (chat.y + 12) };
    float maxBW = area.width * 0.82f, fs = 15.5f, pad = 12; float total = 0;
    struct Item { const ChatTurn* t; std::string txt; float h; }; std::vector<Item> items;
    for (const auto& t : chatHistory) { Item it; it.t = &t; it.txt = t.shown.empty() ? t.text : t.shown; it.h = ui.Wrap(it.txt, 0, 0, maxBW - pad * 2, fs, WHITE, false) + pad * 2; total += it.h + 10; items.push_back(it); }
    if (busy) total += 46;
    float maxScroll = std::max(0.0f, total - area.height); const InputState& in = ui.in;
    if (ui.enabled && ui.Inside(area, in.pos) && in.wheel != 0.0f) { chatScroll -= in.wheel * 50.0f; chatStick = false; }
    if (ui.enabled && in.down && ui.Inside(area, in.pressPos) && std::fabs(in.dragDy) > 0.0f) { chatScroll -= in.dragDy; chatStick = false; }
    if (chatStick) chatScroll = maxScroll;
    chatScroll = std::max(0.0f, std::min(maxScroll, chatScroll)); if (chatScroll >= maxScroll - 1.0f) chatStick = true;
    ui.ScissorBegin(area);
    float y = area.y - chatScroll + (total < area.height ? 0 : 0);
    for (const auto& it : items) {
        bool user = it.t->isUser, err = it.t->isError; float bw = std::min(maxBW, ui.Measure(it.txt.c_str(), fs).x + pad * 2 + 4);
        if (it.h > pad * 2 + fs * 1.3f) bw = maxBW;   // testo a capo: bolla piena
        Rectangle b = { user ? area.x + area.width - bw : area.x, y, bw, it.h };
        if (b.y + b.height >= area.y && b.y <= area.y + area.height) {
            Color fill = user ? Color{ 37, 99, 235, 255 } : (err ? Color{ 70, 24, 36, 255 } : Color{ 28, 38, 66, 255 });
            ui.Panel(b, 16, fill, user ? Color{ 90, 150, 255, 255 } : (err ? Pal::Red : Pal::Border), 1.2f, false);
            ui.Wrap(it.txt, b.x + pad, b.y + pad, bw - pad * 2, fs, err ? Color{ 255, 190, 200, 255 } : Pal::Text, true);
        }
        y += it.h + 10;
    }
    if (busy) {
        Rectangle b = { area.x, y, 250, 38 }; ui.Panel(b, 16, { 28, 38, 66, 255 }, Pal::Border, 1.2f, false);
        for (int i = 0; i < 3; i++) { float ph = 0.5f + 0.5f * sinf(globalTime * 6.0f - i * 0.9f); DrawCircleV({ b.x + 22 + i * 16.0f, b.y + 19 - ph * 3.0f }, 4.5f, A(Pal::Cyan, 0.4f + 0.6f * ph)); }
        char tb[40]; std::snprintf(tb, sizeof(tb), "Gemini sta creando... %ds", (int)aiBusyTime); ui.Text(tb, b.x + 66, b.y + 10, 14, Pal::Muted);
    }
    ui.ScissorEnd();

    // --- anteprima + stats ---
    ui.Panel(side, 18, { 12, 18, 34, 236 }, Pal::Border, 1.2f);
    Rectangle pv = { side.x + 10, side.y + 10, side.width - 20, (side.width - 20) * 0.75f };
    DrawRectangleRounded(pv, 0.07f, 10, { 14, 20, 38, 255 }); DrawSprite(previewTex, pv); DrawRectangleRoundedLinesEx(pv, 0.07f, 10, 1.5f, A(RarityColor(draftCustom.rarity), 0.8f));
    float y2 = pv.y + pv.height + 14; bool has = !draftCustom.scriptRules.empty();
    ui.TextC(ui.Fit(draftCustom.name, side.width - 30, 21, true).c_str(), side.x + side.width * 0.5f, y2 + 10, 21, Pal::Text, true);
    const char* rn = RarityName(draftCustom.rarity); float pw = ui.Measure(rn, 12, true).x + 22;
    ui.Pill({ side.x + side.width * 0.5f - pw * 0.5f, y2 + 28, pw, 21 }, rn, RarityColor(draftCustom.rarity), { 12, 16, 28, 255 }, 12);
    StatBars(&draftCustom, { side.x + 18, y2 + 62, side.width - 36, 96 });
    float dy = y2 + 168;
    if (has) { char rc[32]; std::snprintf(rc, sizeof(rc), "%d regole attive", (int)draftCustom.scriptRules.size()); ui.Text(rc, side.x + 18, dy, 12.5f, Pal::Cyan, true); }
    float by = side.y + side.height - 12 - 50;
    ui.Wrap(has ? draftCustom.desc : "Scrivi a Gemini cosa deve fare la tua palla: apparirà qui in 3D.", side.x + 18, dy + (has ? 20 : 0), side.width - 36, 13, Pal::Muted, true, false, std::max(2, (int)((by - 66 - dy - 22) / 17.0f)));
    if (ui.Button({ side.x + 14, by - 58, side.width - 28, 50 }, "SALVA NELL'ELENCO", Pal::Blue, WHITE, 16, !has)) { SaveCustomBall(false); return; }
    if (ui.Button({ side.x + 14, by, side.width - 28, 50 }, "TESTA SUBITO", Pal::Green, { 6, 30, 18, 255 }, 17, !has)) { SaveCustomBall(true); return; }
}

// ============================================================================
//  MODALE API KEY
// ============================================================================
void PallaGame::KeyModal() {
    ui.enabled = true; bool busy = aiBusy.load();
    DrawRectangle(0, 0, (int)std::ceil(W), (int)std::ceil(H), { 3, 5, 12, 200 });
    Rectangle r = { W * 0.5f - 285, std::max(20.0f, H * 0.5f - 205), 570, 410 }; ui.Panel(r, 22, { 16, 23, 42, 255 }, A(Pal::Cyan, 0.7f), 1.8f);
    ui.Text("Chiave API di Google AI Studio", r.x + 26, r.y + 20, 22, Pal::Text, true);
    ui.Wrap("1) Tocca \"Ottieni key\" e crea una chiave gratuita.   2) Incollala qui sotto.   3) Premi VERIFICA: controllo che funzioni e scelgo io il modello Gemini disponibile.", r.x + 26, r.y + 56, r.width - 52, 14, Pal::Muted);
    Rectangle f = { r.x + 26, r.y + 132, r.width - 52, 54 };
    ui.Panel(f, 14, { 10, 15, 30, 255 }, ui.Held(f) ? Pal::Cyan : Pal::Border, 1.5f, false);
    std::string k = modalKey, shown; if (k.empty()) shown = ""; else { shown = k.size() > 10 ? std::string(std::min<size_t>(22, k.size() - 6), '*') + k.substr(k.size() - 6) : k; }
    if (shown.empty()) ui.Text("Incolla qui la chiave (AQ.Ab... o AIza...)", f.x + 14, f.y + 17, 16, Pal::Dim); else ui.Text(ui.Fit(shown, f.width - 28, 17).c_str(), f.x + 14, f.y + 16, 17, Pal::Text);
    if (ui.Click(f)) {
#ifdef __ANDROID__
        nativeTarget = 2; OpenNativeInputDialog("Incolla la Gemini API Key", modalKey);
#endif
    }
    if (!k.empty()) { char lb[48]; std::snprintf(lb, sizeof(lb), "%d caratteri", (int)SanitizeApiKey(k).size()); ui.Text(lb, f.x + 4, f.y + 60, 12.5f, Pal::Dim); }
    float by = f.y + 84;
    if (ui.Button({ r.x + 26, by, 150, 44 }, "Incolla", Pal::PanelHi, Pal::Text, 15)) { const char* cl = GetClipboardText(); if (cl) { modalKey = SanitizeApiKey(cl); modalError.clear(); } }
    if (ui.Button({ r.x + 186, by, 150, 44 }, "Ottieni key", Pal::Blue, WHITE, 15)) OpenURL("https://aistudio.google.com/apikey");
    if (ui.Button({ r.x + 346, by, r.width - 372, 44 }, "Cancella", Pal::PanelHi, Pal::Text, 15)) { modalKey.clear(); modalError.clear(); }
    if (!modalError.empty()) ui.Wrap(modalError, r.x + 26, by + 58, r.width - 52, 14, { 255, 150, 165, 255 }, true, false, 4);
    else if (busy) { char tb[40]; std::snprintf(tb, sizeof(tb), "Verifico la chiave... %ds", (int)aiBusyTime); ui.Text(tb, r.x + 26, by + 58, 15, Pal::Cyan); }
    float fy = r.y + r.height - 24 - 52;
    if (modalOffer && !modalError.empty()) {
        if (ui.Button({ r.x + 26, fy - 56, r.width - 52, 44 }, "Salva comunque (senza verifica)", Pal::PanelHi, Pal::Orange, 15)) { geminiApiKey = SanitizeApiKey(modalKey); WriteKey(); keyModal = false; modalOffer = false; SetToast("Chiave salvata (non verificata)"); }
    }
    if (ui.Button({ r.x + 26, fy, 160, 52 }, "Chiudi", Pal::PanelHi, Pal::Text, 17)) keyModal = false;
    if (ui.Button({ r.x + 196, fy, r.width - 222, 52 }, busy ? "VERIFICA IN CORSO..." : "VERIFICA E SALVA", Pal::Green, { 6, 30, 18, 255 }, 18, busy || modalKey.empty())) { modalError.clear(); StartAiJob(AiJob::VERIFY); }
}

// ============================================================================
//  SASSO CARTA FORBICE
// ============================================================================
static void RpsIcon(int kind, float cx, float cy, float s, Color c) {
    if (kind == 0) {            // sasso
        DrawCircleV({ cx, cy + s * 0.06f }, s * 0.42f, Lighten(c, 0.55f)); DrawCircleV({ cx - s * 0.04f, cy }, s * 0.40f, c);
        DrawCircleV({ cx - s * 0.16f, cy - s * 0.14f }, s * 0.11f, { 255, 255, 255, 70 });
    } else if (kind == 1) {     // carta
        Rectangle p = { cx - s * 0.30f, cy - s * 0.40f, s * 0.60f, s * 0.80f };
        DrawRectangleRounded({ p.x, p.y + 3, p.width, p.height }, 0.14f, 8, Lighten(c, 0.55f)); DrawRectangleRounded(p, 0.14f, 8, c);
        for (int i = 0; i < 4; i++) DrawRectangle((int)(p.x + s * 0.09f), (int)(p.y + s * 0.15f + i * s * 0.14f), (int)(p.width - s * 0.18f), 3, { 40, 50, 80, 120 });
    } else {                    // forbice
        DrawLineEx({ cx - s * 0.30f, cy + s * 0.34f }, { cx + s * 0.22f, cy - s * 0.38f }, s * 0.10f, c);
        DrawLineEx({ cx + s * 0.30f, cy + s * 0.34f }, { cx - s * 0.22f, cy - s * 0.38f }, s * 0.10f, Lighten(c, 0.85f));
        DrawRing({ cx - s * 0.25f, cy + s * 0.34f }, s * 0.08f, s * 0.16f, 0, 360, 24, Lighten(c, 0.6f)); DrawRing({ cx + s * 0.25f, cy + s * 0.34f }, s * 0.08f, s * 0.16f, 0, 360, 24, Lighten(c, 0.6f));
    }
}

void PallaGame::ScreenRps() {
    static const char* names[3] = { "SASSO", "CARTA", "FORBICE" }; const Color cols[3] = { { 156, 163, 175, 255 }, { 240, 244, 250, 255 }, { 251, 113, 133, 255 } };
    ui.TextC("SASSO CARTA FORBICE", W * 0.5f, 86, 36, Pal::Text, true);
    ui.TextC("Chi vince parte con +25 HP e +2 danno nel primo round!", W * 0.5f, 126, 17, Pal::Muted, false);
    float rw = 230, gap = 22, sx = W * 0.5f - (rw * 3 + gap * 2) * 0.5f, y = 190;
    if (rpsPlayer < 0) {
        for (int i = 0; i < 3; i++) {
            Rectangle r = { sx + i * (rw + gap), y, rw, 190 }; bool held = ui.Held(r); Rectangle q = r; if (held) q.y += 2;
            ui.Panel(q, 20, { 18, 26, 48, 245 }, held ? Pal::Pink : Pal::Border, held ? 3.0f : 1.5f);
            RpsIcon(i, q.x + rw * 0.5f, q.y + 84, 110, cols[i]); ui.TextC(names[i], q.x + rw * 0.5f, q.y + 160, 20, Pal::Text, true);
            if (ui.Click(r)) {
                rpsPlayer = i; rpsEnemy = rand() % 3;
                rpsOutcome = (rpsPlayer == rpsEnemy) ? 0 : (((rpsPlayer == 0 && rpsEnemy == 2) || (rpsPlayer == 1 && rpsEnemy == 0) || (rpsPlayer == 2 && rpsEnemy == 1)) ? 1 : -1);
                rpsShowTimer = 1.5f; SND(rpsOutcome == 1 ? SFX_WIN : SFX_CLICK);
            }
        }
    } else {
        float k = Clamp01((1.5f - rpsShowTimer) * 3.0f);
        ui.Panel({ W * 0.5f - 300, 180, 600, 230 }, 24, { 16, 23, 42, 250 }, rpsOutcome == 1 ? Pal::Green : (rpsOutcome < 0 ? Pal::Red : Pal::Border), 2.5f);
        ui.TextC("TU", W * 0.5f - 160, 205, 16, Pal::Green, true); ui.TextC("NEMICO", W * 0.5f + 160, 205, 16, Pal::Red, true);
        RpsIcon(rpsPlayer, W * 0.5f - 160, 290, 120 * (0.6f + 0.4f * k), cols[rpsPlayer]); RpsIcon(rpsEnemy, W * 0.5f + 160, 290, 120 * (0.6f + 0.4f * k), cols[rpsEnemy]);
        ui.TextC("VS", W * 0.5f, 290, 30, Pal::Muted, true);
        ui.TextC(rpsOutcome == 1 ? "VINTO! +25 HP  +2 DANNO" : (rpsOutcome < 0 ? "PERSO" : "PAREGGIO"), W * 0.5f, 376, 26, rpsOutcome == 1 ? Pal::Green : (rpsOutcome < 0 ? Pal::Red : Pal::Muted), true);
    }
    if (rpsPlayer < 0 && ui.Button({ W * 0.5f - 90, y + 224, 180, 48 }, "Lobby", Pal::PanelHi, Pal::Text, 16)) Go(GameState::LOBBY);
}

// ============================================================================
//  SCELTA PALLA
// ============================================================================
void PallaGame::ScreenDraft() {
    char hdr[96];
    if (mode == GameMode::MODE_FUSION) std::snprintf(hdr, sizeof(hdr), "ROUND %d  -  SCEGLI %s (FUSIONE)", roundNum, pickingSecondFusion ? "LA 2ª PALLA" : "LA 1ª PALLA");
    else std::snprintf(hdr, sizeof(hdr), "ROUND %d  -  SCEGLI LA TUA PALLA", roundNum);
    Hud();
    ui.TextC(hdr, W * 0.5f, 98, 28, Pal::Text, true);
    if (!roundEnemies.empty() && roundEnemies[0].first) {
        std::string en = "Avversario: " + roundEnemies[0].first->name; if (roundEnemies[0].second) en += " + " + roundEnemies[0].second->name; if (roundEnemies.size() > 1) en += "  (+1 nemico)";
        ui.TextC(en.c_str(), W * 0.5f, 128, 16, Pal::Muted, false);
    }
    float cw = std::min(300.0f, (W - 96) / 3.0f), gap = 18, sx = W * 0.5f - (cw * 3 + gap * 2) * 0.5f, ch = std::min(318.0f, H - 270.0f), cy = 150;
    for (size_t i = 0; i < draftChoices.size(); i++) {
        Rectangle r = { sx + i * (cw + gap), cy, cw, ch }; BallCard(draftChoices[i], r, ui.Held(r), true);
        if (ui.Click(r)) {
            SND(SFX_CLICK);
            if (mode == GameMode::MODE_FUSION && !pickingSecondFusion) { selectedPrimary = draftChoices[i]; pickingSecondFusion = true; RollDraftThree(); }
            else if (mode == GameMode::MODE_FUSION) { selectedSecondary = draftChoices[i]; BeginAimPhase(); }
            else { selectedPrimary = draftChoices[i]; BeginAimPhase(); }
            return;
        }
    }
    float by = cy + ch + 22;
    if (ui.Button({ W * 0.5f - 250, by, 230, 54 }, "Cambia le 3 palle", Pal::PanelHi, Pal::Text, 17)) { RollDraftThree(); SND(SFX_CLICK); }
    if (ui.Button({ W * 0.5f + 20, by, 230, 54 }, "Torna alla lobby", Pal::PanelHi, Pal::Text, 17)) Go(GameState::LOBBY);
}

// ============================================================================
//  MIRA
// ============================================================================
void PallaGame::ScreenAim() {
    RenderArenaOverlay(activeMatch, curCam, ui); Hud();
    ui.Panel({ W * 0.5f - 235, 66, 470, 62 }, 18, { 10, 16, 32, 222 }, A(Pal::Cyan, 0.7f), 1.5f);
    ui.TextC("FASE DI MIRA", W * 0.5f, 86, 22, Pal::Cyan, true);
    ui.TextC("Trascina sullo schermo per puntare, poi premi LANCIA", W * 0.5f, 111, 14, Pal::Muted, false);
    float frac = Clamp01(aimTimer / 8.0f); Vector2 c = { 70, H - 64 };
    DrawCircleV(c, 40, { 10, 16, 32, 215 }); DrawRing(c, 30, 37, -90, -90 + 360 * frac, 48, aimTimer < 2.5f ? Pal::Red : Pal::Cyan);
    char tb[8]; std::snprintf(tb, sizeof(tb), "%d", (int)std::ceil(std::max(0.0f, aimTimer))); ui.TextC(tb, c.x, c.y, 28, Pal::Text, true);
    if (ui.Button({ W - 250.0f, H - 92.0f, 230, 70 }, "LANCIA!", Pal::Green, { 6, 30, 18, 255 }, 30)) { SND(SFX_CLICK); LaunchRound(); }
}

// ============================================================================
//  ARENA
// ============================================================================
void PallaGame::ScreenArena() {
    RenderArenaOverlay(activeMatch, curCam, ui); DrawVignette(ui, 90);
    Hud();
    char tb[24]; int sec = (int)activeMatch.time; std::snprintf(tb, sizeof(tb), "%d:%02d", sec / 60, sec % 60); ui.TextC(tb, W * 0.5f, 72, 18, Pal::Muted, true);
    if (activeMatch.suddenDeath && state == GameState::ARENA) {
        float p = 0.6f + 0.4f * sinf(globalTime * 8.0f); ui.TextOut("MORTE IMPROVVISA", W * 0.5f, 104, 24, A(Pal::Red, p), 1.8f);
    }
    if (state == GameState::ARENA) {
        if (ui.Button({ W - 204.0f, H - 128.0f, 184, 48 }, simSpeed == 1 ? "Velocità x1" : "Velocità x2", simSpeed == 2 ? Pal::Orange : Pal::PanelHi, simSpeed == 2 ? Color{ 40, 20, 4, 255 } : Pal::Text, 16)) { simSpeed = (simSpeed == 1) ? 2 : 1; SND(SFX_CLICK); }
        if (ui.Button({ W - 204.0f, H - 70.0f, 184, 48 }, "Abbandona", { 120, 32, 48, 255 }, Pal::Text, 16)) Go(GameState::LOBBY);
    } else {
        float k = Clamp01(roundBannerTimer * 3.0f), k2 = Clamp01((1.7f - roundBannerTimer) * 5.0f); bool won = lastRoundWinner == 0; Color c = won ? Pal::Green : Pal::Red;
        DrawRectangle(0, (int)(H * 0.5f - 62), (int)std::ceil(W), 124, { 4, 6, 14, (unsigned char)(190 * std::min(k, k2)) });
        ui.TextOut(won ? "ROUND VINTO!" : "ROUND PERSO", W * 0.5f, H * 0.5f - 8, 52, A(c, std::min(k, k2)), 2.4f);
        char sb[48]; std::snprintf(sb, sizeof(sb), "Cuori:  tu %d   -   nemico %d", allyHearts, enemyHearts); ui.TextC(sb, W * 0.5f, H * 0.5f + 34, 18, A(Pal::Text, std::min(k, k2)), false);
    }
}

// ============================================================================
//  POTENZIAMENTI (Torneo)
// ============================================================================
void PallaGame::ScreenUpgrade() {
    static const char* uN[4] = { "+25 HP MAX", "+2 DANNO", "+12% VELOCITÀ", "+3 HP/s REGEN" };
    static const char* uD[4] = { "La tua palla resiste molto di più.", "Ogni colpo fa più male.", "Rimbalzi più rapidi per tutto il torneo.", "Ti curi lentamente durante il round." };
    const Color uc[4] = { Pal::Red, Pal::Orange, Pal::Cyan, Pal::Green };
    Hud(); ui.TextC("SCEGLI UN POTENZIAMENTO", W * 0.5f, 100, 32, Pal::Text, true);
    ui.TextC("Resta con te fino alla fine del torneo.", W * 0.5f, 134, 16, Pal::Muted, false);
    float uw = 260, gap = 20, sx = W * 0.5f - (uw * 3 + gap * 2) * 0.5f;
    for (int i = 0; i < 3; i++) {
        int o = upgOptions[i]; Rectangle r = { sx + i * (uw + gap), 180, uw, 230 }; bool held = ui.Held(r); Rectangle q = r; if (held) q.y += 2;
        ui.Panel(q, 20, { 18, 26, 48, 248 }, held ? uc[o] : A(uc[o], 0.6f), held ? 3.0f : 2.0f);
        DrawRectangleRounded({ q.x + 4, q.y + 4, q.width - 8, 90 }, 0.25f, 10, A(uc[o], 0.16f));
        DrawCircleV({ q.x + uw * 0.5f, q.y + 62 }, 34, A(uc[o], 0.25f)); DrawRing({ q.x + uw * 0.5f, q.y + 62 }, 28, 34, 0, 360, 36, uc[o]);
        DrawRectangleRounded({ q.x + uw * 0.5f - 3.0f, q.y + 46, 6, 32 }, 1.0f, 6, uc[o]); DrawRectangleRounded({ q.x + uw * 0.5f - 16.0f, q.y + 59, 32, 6 }, 1.0f, 6, uc[o]);
        ui.TextC(uN[o], q.x + uw * 0.5f, q.y + 130, 22, Pal::Text, true);
        ui.Wrap(uD[o], q.x + 22, q.y + 160, uw - 44, 14.5f, Pal::Muted, true, false, 3);
        if (ui.Click(r)) {
            if (o == 0) upgHp += 25; else if (o == 1) upgDmg += 2; else if (o == 2) upgSpd += 0.12f; else upgRegen += 3.0f;
            SND(SFX_WIN); PrepareNextRound(); return;
        }
    }
}

// ============================================================================
//  RISULTATO
// ============================================================================
void PallaGame::ScreenResult() {
    DrawRectangle(0, 0, (int)std::ceil(W), (int)std::ceil(H), { 3, 5, 12, 150 });
    bool won = resultInfo.won; Color c = won ? Pal::Green : Pal::Red; Rectangle r = { W * 0.5f - 290, H * 0.5f - 190, 580, 380 };
    ui.Panel(r, 26, { 16, 23, 42, 252 }, c, 2.5f);
    DrawRectangleRounded({ r.x + 4, r.y + 4, r.width - 8, 120 }, 0.2f, 10, A(c, 0.14f));
    ui.TextC(won ? "VITTORIA!" : "SCONFITTA", r.x + r.width * 0.5f, r.y + 62, 54, c, true);
    char b[64]; std::snprintf(b, sizeof(b), "Cuori finali:  tu %d   -   nemico %d", allyHearts, enemyHearts); ui.TextC(b, r.x + r.width * 0.5f, r.y + 120, 16, Pal::Muted, false);
    std::snprintf(b, sizeof(b), "+%d XP", resultInfo.xp); ui.Pill({ r.x + 70, r.y + 156, 120, 34 }, b, Pal::Gold, { 30, 20, 2, 255 }, 18);
    if (mode == GameMode::MODE_RANKED) { std::snprintf(b, sizeof(b), "%+d RP", resultInfo.rpDelta); ui.Pill({ r.x + 200, r.y + 156, 120, 34 }, b, resultInfo.rpDelta >= 0 ? Pal::Cyan : Pal::Red, { 8, 14, 24, 255 }, 18); }
    if (resultInfo.levelUp) { std::snprintf(b, sizeof(b), "LIVELLO %d!", save.level); ui.Pill({ r.x + 330, r.y + 156, 180, 34 }, b, Pal::Purple, WHITE, 18); }
    int sec = (int)resultInfo.duration; std::snprintf(b, sizeof(b), "Ultimo round: %d:%02d   |   Vittorie totali: %d", sec / 60, sec % 60, save.wins); ui.TextC(b, r.x + r.width * 0.5f, r.y + 224, 15, Pal::Muted, false);
    if (ui.Button({ r.x + 36, r.y + r.height - 100, 244, 66 }, "RIGIOCA", Pal::Green, { 6, 30, 18, 255 }, 24)) StartNewMatch();
    if (ui.Button({ r.x + r.width - 280, r.y + r.height - 100, 244, 66 }, "LOBBY", Pal::PanelHi, Pal::Text, 22)) Go(GameState::LOBBY);
}
