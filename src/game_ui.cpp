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

// Testo "formato AI" di una palla: serve a Gemini (palla attuale), al salvataggio e alla descrizione mostrata in elenco.
static std::string ScriptText(const BallDef& d) {
    char b[160]; std::string t;
    t += "CHAT: " + (d.desc.empty() ? std::string("Palla personalizzata") : d.desc) + "\n";
    t += "NAME: " + d.name + "\n";
    std::string rn = RarityName(d.rarity); for (char& c : rn) c = (char)std::toupper((unsigned char)c);
    t += "RARITY: " + rn + "\n";
    std::snprintf(b, sizeof(b), "HP: %d\nDMG: %d\nSPEED: %.2f\nSIZE: %.2f\nCOLOR: %d,%d,%d\n", d.hp, d.dmg, d.speedMult, d.baseRadius, (int)d.color.r, (int)d.color.g, (int)d.color.b);
    t += b; t += "STYLE: " + d.visualStyle + "\n"; t += RulesToText(d.scriptRules);
    return t;
}

static void WriteBallBlock(std::ostream& f, const BallDef& b) {
    f << b.id << " " << (int)b.rarity << " " << (int)b.color.r << " " << (int)b.color.g << " " << (int)b.color.b
      << " " << b.hp << " " << b.dmg << " " << b.speedMult << " " << b.baseRadius << " " << (b.visualStyle.empty() ? "none" : b.visualStyle) << " " << b.scriptRules.size() << "\n";
    std::string nm = b.name; for (char& c : nm) if (c == '\n' || c == '\r') c = ' ';
    std::string enc; { std::istringstream is(b.customPrompt); std::string l; while (std::getline(is, l)) { while (!l.empty() && l.back() == '\r') l.pop_back(); enc += l + "\x01"; } }
    f << nm << "\n" << enc << "\n" << SerializeScriptRules(b.scriptRules);
}
static bool ReadBallBlock(std::istream& f, BallDef& b) {
    int rar = 5, r = 34, g = 211, bl = 238, rc = 0; std::string hdr; if (!std::getline(f, hdr)) return false;
    std::istringstream iss(hdr); iss >> b.id >> rar >> r >> g >> bl >> b.hp >> b.dmg >> b.speedMult >> b.baseRadius >> b.visualStyle >> rc;
    if (b.id.empty()) return false;
    b.rarity = (Rarity)std::max(0, std::min(5, rar)); b.color = { (unsigned char)r, (unsigned char)g, (unsigned char)bl, 255 };
    std::getline(f, b.name); std::getline(f, b.customPrompt); for (char& c : b.customPrompt) if (c == '\x01') c = '\n';
    std::string block; for (int k = 0; k < rc; k++) { std::string rl; if (!std::getline(f, rl)) break; block += rl + "\n"; }
    b.scriptRules = DeserializeScriptRules(block);
    ParsedBallScript ps = ParseAIScript(b.customPrompt); if (!ps.chatMsg.empty()) b.desc = ps.chatMsg;
    return true;
}

void PallaGame::LoadCustomBalls() {
    std::ifstream f(customFile); if (!f.is_open()) return; int count = 0; if (!(f >> count)) return;
    std::string dummy; std::getline(f, dummy);
    for (int i = 0; i < count; i++) {
        BallDef b; b.isCustom = true; if (!ReadBallBlock(f, b)) break;
        if (b.desc.empty()) b.desc = "Palla creata con l'AI (" + std::to_string(b.scriptRules.size()) + " regole)";
        GetBallCatalog().push_back(b);
    }
}
void PallaGame::WriteCustomBalls() const {
    std::ofstream f(customFile); if (!f.is_open()) return;
    std::vector<const BallDef*> cs; for (const auto& b : GetBallCatalog()) if (b.isCustom) cs.push_back(&b);
    f << cs.size() << "\n"; for (const auto* b : cs) WriteBallBlock(f, *b);
}
// Modifiche alle palle del gioco (nome, aspetto, statistiche, regole extra): file a parte, l'abilita' integrata resta quella originale.
void PallaGame::LoadEdits() {
    std::ifstream f(editsFile); if (!f.is_open()) return; int count = 0; if (!(f >> count)) return;
    std::string dummy; std::getline(f, dummy);
    for (int i = 0; i < count; i++) {
        BallDef e; if (!ReadBallBlock(f, e)) break;
        for (auto& b : GetBallCatalog()) if (!b.isCustom && b.id == e.id) {
            b.name = e.name; b.rarity = e.rarity; b.color = e.color; b.hp = e.hp; b.dmg = e.dmg; b.speedMult = e.speedMult; b.baseRadius = e.baseRadius;
            b.visualStyle = e.visualStyle; b.scriptRules = e.scriptRules; if (!e.desc.empty()) b.desc = e.desc; editedIds.insert(b.id);
        }
    }
}
void PallaGame::WriteEdits() const {
    std::ofstream f(editsFile); if (!f.is_open()) return; f << editedIds.size() << "\n";
    for (const auto& id : editedIds) for (const auto& b : GetBallCatalog()) if (b.id == id) {
        BallDef c = b; c.customPrompt = ScriptText(b); WriteBallBlock(f, c);
    }
}

// ============================================================================
//  Init / sprite
// ============================================================================
void PallaGame::Init(const std::string& dataDir) {
    saveFile = dataDir + "palla_vs_palla_save.txt"; customFile = dataDir + "palla_vs_palla_custom.txt"; keyFile = dataDir + "palla_gemini_key.txt"; editsFile = dataDir + "palla_vs_palla_edits.txt";
    audio = Audio_Init(); ui.Init(); GfxInit();
    previewTex = LoadRenderTexture(640, 480); SetTextureFilter(previewTex.texture, TEXTURE_FILTER_BILINEAR);
    trainDummy.id = "__dummy"; trainDummy.name = "Sacco 1000 HP"; trainDummy.isDummy = true; trainDummy.hp = 1000; trainDummy.dmg = 0; trainDummy.speedMult = 0.0f;
    trainDummy.baseRadius = 1.4f; trainDummy.color = { 148, 163, 184, 255 }; trainDummy.rarity = Rarity::Raro; trainDummy.visualStyle = "none"; trainDummy.desc = "Bersaglio fermo per provare le palle.";
    for (const auto& b : GetBallCatalog()) originals[b.id] = b;     // copia di fabbrica (per "Ripristina originale")
    LoadKey(); LoadCustomBalls(); LoadEdits(); LoadSave(); modalKey = geminiApiKey;
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
void PallaGame::BuildSprite(const BallDef* def, bool force) {
    if (!def) return;
    auto it = sprites.find(def->id);
    if (it != sprites.end()) { if (!force) return; UnloadRenderTexture(it->second); sprites.erase(it); }
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
    if (training) { OnTrainRoundEnded(); return; }
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
    chatHistory.push_back(g); chatInput.clear(); chatStick = true; nativeTarget = 0; aiEditMode = false;
    state = GameState::AI_CHAT; SND(SFX_CLICK);
    if (geminiApiKey.empty()) { keyModal = true; modalKey.clear(); modalError.clear(); modalOffer = false; }
}

void PallaGame::StartAiJob(AiJob kind) {
    if (aiBusy.load()) return;
    if (aiThread.joinable()) aiThread.join();
    aiBusy.store(true); aiBusyTime = 0.0f;
    std::string key = (kind == AiJob::VERIFY) ? modalKey : geminiApiKey, cur = draftCustom.customPrompt;
    if (aiEditMode && editBuiltin && kind == AiJob::CHAT) cur += "\nNOTA: questa e' una palla del gioco con un'abilita integrata che NON puoi cambiare ne rimuovere. Puoi modificare statistiche e aspetto e AGGIUNGERE regole extra; se l'utente non chiede abilita nuove, non scrivere righe RULE.\n"; std::vector<ChatTurn> hist = chatHistory;
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
    draftCustom.desc = say;
    if (!ps.warnings.empty()) { say += "  [Attenzione: " + std::to_string(ps.warnings.size()) + " regola/e ignorata/e: " + ps.warnings[0] + ". Chiedimi di correggere.]"; }
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
    while (!t.empty()) { size_t k = t.size() - 1; while (k > 0 && ((unsigned char)t[k] & 0xC0) == 0x80) k--; unsigned char c = (unsigned char)t[k]; size_t need = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1; if (t.size() - k < need) t.erase(k); else break; }
}

// ============================================================================
//  Logica per frame
// ============================================================================
void PallaGame::HandleBack() {
    if (keyModal) { keyModal = false; return; }
    if (training && (state == GameState::ARENA || state == GameState::ROUND_END || state == GameState::RESULT)) { ExitTraining(); return; }
    switch (state) {
        case GameState::AI_CHAT: if (aiEditMode) { state = GameState::EDIT; } else Go(GameState::COLLECTION); break;
        case GameState::EDIT: Go(GameState::COLLECTION); break;
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
        else if (nativeTarget == 3) { editDraft.name = in.nativeText.substr(0, 40); }
        else if (nativeTarget == 4) { editDraft.desc = in.nativeText.substr(0, 200); }
        else if (nativeTarget == 5) {
            ParsedBallScript ps = ParseAIScript(in.nativeText);
            if (!ps.rules.empty() || in.nativeText.find_first_not_of(" \n\r\t") == std::string::npos) { editDraft.scriptRules = ps.rules; SetToast(ps.warnings.empty() ? "Regole aggiornate" : "Regole aggiornate (" + std::to_string(ps.warnings.size()) + " ignorate)"); }
            else SetToast("Nessuna regola valida nel testo");
        }
        nativeTarget = 0;
    }
    if (keyModal) { AppendTyped(modalKey, 220, true); if (in.enter && !aiBusy.load() && !modalKey.empty()) { modalError.clear(); StartAiJob(AiJob::VERIFY); } }
    else if (state == GameState::AI_CHAT) { AppendTyped(chatInput, 400, true); if (in.enter) SendAiChatMessage(); }
    else if (state == GameState::EDIT && editFocus != 0) {
        if (editFocus == 1) AppendTyped(editDraft.name, 22, true); else AppendTyped(editDraft.desc, 160, true);
        if (in.enter) editFocus = 0;
    }
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
            if (training) { StartTrainRound(); return; }
            if (mode == GameMode::MODE_TOURNAMENT) { upgOptions[0] = rand() % 4; upgOptions[1] = (upgOptions[0] + 1) % 4; upgOptions[2] = (upgOptions[0] + 2) % 4; state = GameState::UPGRADE; }
            else PrepareNextRound();
        }
        return;
    }
    if (state == GameState::RPS && rpsPlayer >= 0) { rpsShowTimer -= dt; if (rpsShowTimer <= 0.0f) state = GameState::DRAFT; return; }
    if (state == GameState::LOBBY) demoMatch.Update(dt, &audio, false);
    if (state == GameState::ARENA) {
        for (int s = 0; s < simSpeed; s++) if (!activeMatch.ended) activeMatch.Update(dt, &audio, soundOn);
        if (training && !activeMatch.ended && activeMatch.time >= 60.0f) { activeMatch.ended = true; activeMatch.winnerTeam = -1; }   // limite di 60 s per round
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
    while (!spriteRefresh.empty()) { BuildSprite(spriteRefresh.back(), true); spriteRefresh.pop_back(); }
    if (state == GameState::EDIT) RenderBallSprite(&editDraft, previewTex, globalTime * 0.8f, true);
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
        case GameState::EDIT: ScreenEdit(); break;
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
    bool inLobby = state == GameState::LOBBY, inPick = state == GameState::COLLECTION && collPick, inList = state == GameState::COLLECTION && !collPick;
    if (ui.Button({ 24, 86, 184, 50 }, "BATTAGLIA", inLobby ? Pal::Blue : Pal::PanelHi, Pal::Text, 17)) { if (!inLobby) { collPick = false; Go(GameState::LOBBY); } }
    char nb[40]; std::snprintf(nb, sizeof(nb), "PALLE (%d)", (int)GetBallCatalog().size());
    if (ui.Button({ 24, 144, 184, 50 }, nb, inList ? Pal::Blue : Pal::PanelHi, Pal::Text, 17)) { if (!inList) { collPick = false; Go(GameState::COLLECTION); collScroll = 0; } }
    if (ui.Button({ 24, 202, 184, 50 }, "CAMPO DI PROVA", inPick ? Pal::Blue : Pal::PanelHi, inPick ? Pal::Text : Pal::Green, 15.5f)) { if (!inPick) { collPick = true; Go(GameState::COLLECTION); collScroll = 0; } }
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

int PallaGame::BallCard(const BallDef* def, Rectangle r, bool hl, bool showDesc, int buttons) {
    Color rc = RarityColor(def->rarity); bool held = ui.Held(r) && buttons == 0;
    Rectangle q = r; if (held) { q.y += 2; }
    ui.Panel(q, 16, held ? Color{ 28, 38, 66, 255 } : Color{ 20, 28, 50, 250 }, hl ? rc : A(rc, 0.55f), hl ? 3.0f : 1.6f);
    DrawRectangleRounded({ q.x + 3, q.y + 3, q.width - 6, std::min(96.0f, q.height * 0.42f) }, 0.25f, 10, A(rc, 0.16f));
    float s = std::min(q.width - 24.0f, std::min(120.0f, q.height * 0.46f)); Rectangle sp = { q.x + q.width * 0.5f - s * 0.5f, q.y + 6, s, s };
    const RenderTexture2D* rt = GetSprite(def);
    if (rt) DrawSprite(*rt, sp); else ui.Orb(sp.x + s * 0.5f, sp.y + s * 0.5f, s * 0.3f, def->color);
    const char* rn = RarityName(def->rarity); float pw = ui.Measure(rn, 11.5f, true).x + 18;
    ui.Pill({ q.x + 10, q.y + 10, pw, 20 }, rn, rc, (def->rarity == Rarity::Ultra || def->rarity == Rarity::Leggendario || def->rarity == Rarity::Raro) ? Color{ 12, 16, 28, 255 } : WHITE, 11.5f);
    if (def->isCustom) ui.Pill({ q.x + q.width - 40, q.y + 10, 30, 20 }, "AI", Pal::Cyan, { 8, 22, 34, 255 }, 11.5f);
    float ny = q.y + 6 + s + 6;
    ui.TextC(ui.Fit(def->name, q.width - 20, 18, true).c_str(), q.x + q.width * 0.5f, ny + 9, 18, Pal::Text, true);
    char st[40]; std::snprintf(st, sizeof(st), "HP %d    DMG %d", def->hp, def->dmg);
    ui.TextC(st, q.x + q.width * 0.5f, ny + 32, 14, { 150, 190, 240, 255 }, true);
    float bottom = q.y + q.height - (buttons ? 46.0f : 10.0f);
    if (showDesc && bottom - (ny + 48) > 16) ui.Wrap(def->desc, q.x + 14, ny + 50, q.width - 28, 12.5f, Pal::Muted, true, false, std::max(1, (int)((bottom - ny - 50) / 16.0f)));
    int res = 0;
    if (buttons == 1) {
        float bw = (q.width - 24 - 8) / 2.0f;
        if (ui.Button({ q.x + 12, q.y + q.height - 40, bw, 30 }, "PROVA", { 26, 110, 78, 255 }, WHITE, 13.5f)) res = 2;
        if (ui.Button({ q.x + 12 + bw + 8, q.y + q.height - 40, bw, 30 }, "MODIFICA", Pal::PanelHi, Pal::Cyan, 13.5f)) res = 1;
    } else if (buttons == 2) {
        if (ui.Button({ q.x + 12, q.y + q.height - 40, q.width - 24, 30 }, "PROVA CON QUESTA", { 26, 110, 78, 255 }, WHITE, 13.5f)) res = 2;
    }
    return res;
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
    if (collPick) { ui.Text("CAMPO DI PROVA", cx, 74, 24, Pal::Green, true); ui.Text("Scegli la tua palla: affronta un sacco fermo da 1000 HP. 3 round, puoi uscire quando vuoi.", cx + 250, 82, 14, Pal::Muted); }
    else { ui.Text("ELENCO PALLE", cx, 74, 24, Pal::Text, true); char cnt[40]; std::snprintf(cnt, sizeof(cnt), "%d palle sbloccate", (int)GetBallCatalog().size()); ui.Text(cnt, cx + 190, 82, 14, Pal::Muted); }
    // filtri rarità
    const char* names[7] = { "TUTTE", RarityName(Rarity::Raro), RarityName(Rarity::SuperRaro), RarityName(Rarity::Epico), RarityName(Rarity::Mitico), RarityName(Rarity::Leggendario), RarityName(Rarity::Ultra) };
    float fx = cx, fy = 108; 
    for (int i = 0; i < 7; i++) {
        float w = ui.Measure(names[i], 13, true).x + 26; Color c = (i == 0) ? Pal::Cyan : RarityColor((Rarity)(i - 1));
        if (ui.Chip({ fx, fy, w, 30 }, names[i], c, rarFilter == i - 1, 13)) { rarFilter = i - 1; collScroll = 0; SND(SFX_CLICK); }
        fx += w + 8;
    }
    Rectangle area = { cx, 148, cw, H - 148 - 10 };
    int cols = std::max(2, (int)((cw + 14) / 214.0f)); float gap = 14, cwid = (cw - gap * (cols - 1)) / cols, ch = 268;
    std::vector<const BallDef*> items; for (const auto& b : GetBallCatalog()) if (rarFilter < 0 || (int)b.rarity == rarFilter) items.push_back(&b);
    int off = collPick ? 0 : 1; int total = (int)items.size() + off; int rows = (total + cols - 1) / cols; float contentH = rows * (ch + gap) + 8, maxScroll = std::max(0.0f, contentH - area.height);
    const InputState& in = ui.in;
    if (ui.enabled && ui.Inside(area, in.pos)) collScroll -= in.wheel * 60.0f;
    if (ui.enabled && in.down && ui.Inside(area, in.pressPos)) collScroll -= in.dragDy;
    collScroll = std::max(0.0f, std::min(maxScroll, collScroll));
    ui.ScissorBegin(area);
    for (int i = 0; i < total; i++) {
        Rectangle r = { area.x + (i % cols) * (cwid + gap), area.y + (i / cols) * (ch + gap) - collScroll + 4, cwid, ch };
        if (r.y > area.y + area.height || r.y + r.height < area.y) continue;
        bool visible = ui.Inside(area, in.pos);
        if (i == 0 && !collPick) {
            bool held = ui.Held(r) && visible; Rectangle q = r; if (held) q.y += 2;
            ui.Panel(q, 16, { 14, 26, 46, 250 }, A(Pal::Cyan, 0.8f), 2.2f);
            DrawRectangleRounded({ q.x + 3, q.y + 3, q.width - 6, 100 }, 0.25f, 10, A(Pal::Cyan, 0.12f));
            float px = q.x + q.width * 0.5f, py = q.y + 74; DrawCircleV({ px, py }, 38, A(Pal::Cyan, 0.18f));
            DrawRectangleRounded({ px - 3.5f, py - 20, 7, 40 }, 1.0f, 6, Pal::Cyan); DrawRectangleRounded({ px - 20, py - 3.5f, 40, 7 }, 1.0f, 6, Pal::Cyan);
            ui.TextC("CREA CON AI", px, q.y + 148, 19, Pal::Text, true);
            ui.Wrap("Descrivi una palla a Gemini e la vedi subito in 3D.", q.x + 16, q.y + 172, q.width - 32, 13, Pal::Muted, true, false, 3);
            if (ui.Click(r) && visible) { OpenAiChatStudio(); break; }
        } else {
            const BallDef* d = items[i - off];
            int act = BallCard(d, r, false, true, collPick ? 2 : 1);
            if (act && visible) { if (act == 1) OpenEditor(const_cast<BallDef*>(d)); else StartTraining(*d, GameState::COLLECTION); break; }
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
    if (ui.Button({ 14, 9, 118, 40 }, "Indietro", Pal::PanelHi, Pal::Text, 16)) { if (aiEditMode) state = GameState::EDIT; else Go(GameState::COLLECTION); return; }
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
    int nb = aiEditMode ? 3 : 2; float bh = 46.0f, bg = 8.0f, btop = side.y + side.height - 12 - nb * bh - (nb - 1) * bg;
    ui.Wrap(has ? draftCustom.desc : "Scrivi a Gemini cosa deve fare la tua palla: apparirà qui in 3D.", side.x + 18, dy + (has ? 20 : 0), side.width - 36, 13, Pal::Muted, true, false, std::max(2, (int)((btop - 8 - dy - 22) / 17.0f)));
    auto bt = [&](int i) { return Rectangle{ side.x + 14, btop + i * (bh + bg), side.width - 28, bh }; };
    if (ui.Button(bt(0), "PROVA (campo di prova)", { 26, 110, 78, 255 }, WHITE, 16, !has)) { StartTraining(draftCustom, GameState::AI_CHAT); return; }
    if (aiEditMode) {
        if (ui.Button(bt(1), "APPLICA ALL'EDITOR", Pal::Blue, WHITE, 16, !has)) { ApplyAiDraftToEditor(); return; }
        if (ui.Button(bt(2), "APPLICA E SALVA", Pal::Green, { 6, 30, 18, 255 }, 16, !has)) { ApplyAiDraftToEditor(); SaveEdit(); return; }
    } else {
        if (ui.Button(bt(1), "SALVA NELL'ELENCO", Pal::Green, { 6, 30, 18, 255 }, 16, !has)) { SaveCustomBall(false); return; }
    }
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
    if (training) {
        TrainHud();
        if (state == GameState::ARENA) {
            if (ui.Button({ W - 204.0f, H - 186.0f, 184, 48 }, simSpeed == 1 ? "Velocità x1" : "Velocità x2", simSpeed == 2 ? Pal::Orange : Pal::PanelHi, simSpeed == 2 ? Color{ 40, 20, 4, 255 } : Pal::Text, 16)) { simSpeed = (simSpeed == 1) ? 2 : 1; SND(SFX_CLICK); }
            if (ui.Button({ W - 204.0f, H - 128.0f, 184, 48 }, "Fine round", Pal::PanelHi, Pal::Cyan, 16)) { activeMatch.ended = true; activeMatch.winnerTeam = -1; OnRoundEnded(); return; }
            if (ui.Button({ W - 204.0f, H - 70.0f, 184, 48 }, "Esci dalla prova", { 120, 32, 48, 255 }, Pal::Text, 16)) { ExitTraining(); return; }
        } else {
            float k = Clamp01(roundBannerTimer * 3.0f), k2 = Clamp01((1.6f - roundBannerTimer) * 5.0f), a = std::min(k, k2);
            const TrainStat& st = trainStats[std::max(0, trainRound - 2)];
            DrawRectangle(0, (int)(H * 0.5f - 62), (int)std::ceil(W), 124, { 4, 6, 14, (unsigned char)(190 * a) });
            char t1[48]; std::snprintf(t1, sizeof(t1), "ROUND %d COMPLETATO", std::max(1, trainRound - 1)); ui.TextOut(t1, W * 0.5f, H * 0.5f - 10, 46, A(st.killed ? Pal::Green : Pal::Cyan, a), 2.2f);
            char t2[96]; if (st.killed) std::snprintf(t2, sizeof(t2), "Sacco distrutto in %.1fs  -  %.0f danni al secondo", st.time, (float)st.dmg / std::max(0.1f, st.time));
            else std::snprintf(t2, sizeof(t2), "Danno inflitto: %d su 1000 in %.0fs  -  %.0f danni al secondo", st.dmg, st.time, (float)st.dmg / std::max(0.1f, st.time));
            ui.TextC(t2, W * 0.5f, H * 0.5f + 34, 17, A(Pal::Text, a), false);
        }
        return;
    }
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
    if (training) {
        DrawRectangle(0, 0, (int)std::ceil(W), (int)std::ceil(H), { 3, 5, 12, 170 });
        Rectangle r = { W * 0.5f - 310, H * 0.5f - 205, 620, 410 }; ui.Panel(r, 26, { 16, 23, 42, 252 }, Pal::Green, 2.5f);
        DrawRectangleRounded({ r.x + 4, r.y + 4, r.width - 8, 96 }, 0.2f, 10, A(Pal::Green, 0.12f));
        ui.TextC("PROVA COMPLETATA", r.x + r.width * 0.5f, r.y + 44, 40, Pal::Green, true);
        ui.TextC(ui.Fit(trainBall.name, r.width - 60, 18, true).c_str(), r.x + r.width * 0.5f, r.y + 82, 18, Pal::Text, true);
        float totDps = 0.0f; int n = 0;
        for (int i = 0; i < 3; i++) {
            const TrainStat& st = trainStats[i]; float y = r.y + 122 + i * 52.0f, dps = (float)st.dmg / std::max(0.1f, st.time); totDps += dps; n++;
            ui.Panel({ r.x + 26, y, r.width - 52, 44 }, 12, { 22, 30, 54, 255 }, { 60, 78, 120, 140 }, 1.0f, false);
            char a[24]; std::snprintf(a, sizeof(a), "Round %d", i + 1); ui.Text(a, r.x + 42, y + 12, 17, Pal::Muted, true);
            char b[80]; if (st.killed) std::snprintf(b, sizeof(b), "Sacco distrutto in %.1fs", st.time); else std::snprintf(b, sizeof(b), "Danno %d su 1000 in %.0fs", st.dmg, st.time);
            ui.Text(b, r.x + 140, y + 12, 17, st.killed ? Pal::Green : Pal::Orange, true);
            char c[32]; std::snprintf(c, sizeof(c), "%.0f / s", dps); ui.TextR(c, r.x + r.width - 42, y + 12, 17, Pal::Text, true);
        }
        char m[64]; std::snprintf(m, sizeof(m), "Media: %.0f danni al secondo", totDps / std::max(1, n)); ui.TextC(m, r.x + r.width * 0.5f, r.y + 292, 18, Pal::Gold, true);
        const char* back = trainReturn == GameState::AI_CHAT ? "TORNA ALL'AI STUDIO" : (trainReturn == GameState::EDIT ? "TORNA ALL'EDITOR" : (trainReturn == GameState::COLLECTION ? "TORNA ALL'ELENCO" : "TORNA ALLA LOBBY"));
        if (ui.Button({ r.x + 26, r.y + r.height - 78, 270, 58 }, "RIFAI 3 ROUND", Pal::Green, { 6, 30, 18, 255 }, 20)) { BallDef keep = trainBall; GameState ret = trainReturn; StartTraining(keep, ret); return; }
        if (ui.Button({ r.x + r.width - 296, r.y + r.height - 78, 270, 58 }, back, Pal::PanelHi, Pal::Text, 16)) { ExitTraining(); return; }
        return;
    }
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


// ============================================================================
//  EDITOR PALLE (a mano o con l'AI) - vale per le palle del gioco e per quelle create con l'AI
// ============================================================================
static int ClampI(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static float ClampF(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static void ApplyDraftTo(BallDef& dst, const BallDef& src) {
    if (!src.name.empty()) dst.name = src.name;
    dst.rarity = src.rarity; dst.color = src.color; dst.hp = ClampI(src.hp, 20, 600); dst.dmg = ClampI(src.dmg, 1, 80);
    dst.speedMult = ClampF(src.speedMult, 0.4f, 2.6f); dst.baseRadius = ClampF(src.baseRadius, 0.7f, 1.8f);
    dst.visualStyle = src.visualStyle.empty() ? "none" : src.visualStyle; dst.desc = src.desc; dst.scriptRules = src.scriptRules;
    if (dst.isCustom) dst.customPrompt = ScriptText(dst);   // il testo salvato resta coerente con quello che vedi
}

void PallaGame::OpenEditor(BallDef* b) {
    editTarget = b; editDraft = *b; editBuiltin = !b->isCustom; editFocus = 0; aiEditMode = false; editScroll = 0; state = GameState::EDIT; SND(SFX_CLICK);
}

void PallaGame::SaveEdit() {
    if (!editTarget) return;
    ApplyDraftTo(*editTarget, editDraft);
    if (editBuiltin) { editedIds.insert(editTarget->id); WriteEdits(); } else WriteCustomBalls();
    spriteRefresh.push_back(editTarget); SetToast("Palla \"" + editTarget->name + "\" aggiornata!");
    aiEditMode = false; state = GameState::COLLECTION; SND(SFX_WIN);
}

void PallaGame::ResetEdit() {
    if (!editTarget || !editBuiltin) return;
    auto it = originals.find(editTarget->id); if (it == originals.end()) return;
    *editTarget = it->second; editedIds.erase(editTarget->id); WriteEdits();
    spriteRefresh.push_back(editTarget); SetToast("Palla ripristinata all'originale"); state = GameState::COLLECTION; SND(SFX_CLICK);
}

void PallaGame::OpenAiForEdit() {
    draftCustom = editDraft; draftCustom.isCustom = true; draftCustom.customPrompt = ScriptText(editDraft);
    chatHistory.clear(); ChatTurn g; g.isUser = false;
    g.text = "Stai modificando \"" + editDraft.name + "\". Dimmi cosa vuoi cambiare: statistiche, colore, aspetto o abilità. Esempi: \"più veloce e più grossa\", \"fai scudo blu\", \"aggiungi un laser ogni 5 secondi\".";
    chatHistory.push_back(g); chatInput.clear(); chatStick = true; nativeTarget = 0; aiEditMode = true; state = GameState::AI_CHAT; SND(SFX_CLICK);
    if (geminiApiKey.empty()) { keyModal = true; modalKey.clear(); modalError.clear(); modalOffer = false; }
}

void PallaGame::ApplyAiDraftToEditor() {
    editDraft.name = draftCustom.name; editDraft.rarity = draftCustom.rarity; editDraft.color = draftCustom.color; editDraft.hp = draftCustom.hp; editDraft.dmg = draftCustom.dmg;
    editDraft.speedMult = draftCustom.speedMult; editDraft.baseRadius = draftCustom.baseRadius; editDraft.visualStyle = draftCustom.visualStyle;
    editDraft.desc = draftCustom.desc; editDraft.scriptRules = draftCustom.scriptRules;
    aiEditMode = false; state = GameState::EDIT; SND(SFX_CLICK);
}

void PallaGame::ScreenEdit() {
    const InputState& in = ui.in; bool fieldClicked = false;
    DrawRectangle(0, 0, (int)std::ceil(W), 58, { 8, 12, 24, 235 }); DrawRectangle(0, 57, (int)std::ceil(W), 1, { 255, 255, 255, 26 });
    if (ui.Button({ 14, 9, 118, 40 }, "Annulla", Pal::PanelHi, Pal::Text, 16)) { Go(GameState::COLLECTION); return; }
    ui.Text("MODIFICA PALLA", 148, 9, 26, Pal::Text, true);
    ui.Text(editBuiltin ? "Palla del gioco: l'abilità integrata resta, puoi cambiare il resto e aggiungere regole" : "Palla creata con l'AI: puoi cambiare tutto", 150, 36, 12.5f, Pal::Muted);

    // ---------------- colonna sinistra: anteprima, nome, rarità ----------------
    float lx = 14, lw = 330; Rectangle pv = { lx, 68, lw, lw * 0.75f };
    DrawRectangleRounded(pv, 0.06f, 10, { 14, 20, 38, 255 }); DrawSprite(previewTex, pv); DrawRectangleRoundedLinesEx(pv, 0.06f, 10, 1.5f, A(RarityColor(editDraft.rarity), 0.85f));
    float y = pv.y + pv.height + 10; bool blink = ((int)(globalTime * 2.0f) & 1) == 0;
    ui.Text("NOME", lx + 2, y, 12, Pal::Muted, true);
    Rectangle nf = { lx, y + 16, lw, 40 };
    ui.Panel(nf, 12, { 22, 30, 54, 255 }, editFocus == 1 ? Pal::Cyan : Pal::Border, 1.5f, false);
    { std::string nm = ui.Fit(editDraft.name, lw - 28, 18, true); ui.Text(nm.c_str(), nf.x + 14, nf.y + 9, 18, Pal::Text, true);
      if (editFocus == 1 && blink) DrawRectangle((int)(nf.x + 15 + ui.Measure(nm.c_str(), 18, true).x), (int)(nf.y + 8), 2, 24, Pal::Cyan); }
    if (ui.Click(nf)) { fieldClicked = true;
#ifdef __ANDROID__
        nativeTarget = 3; OpenNativeInputDialog("Nome della palla", editDraft.name);
#else
        editFocus = 1;
#endif
    }
    y = nf.y + nf.height + 10; ui.Text("RARITÀ", lx + 2, y, 12, Pal::Muted, true);
    float cw3 = (lw - 16) / 3.0f;
    for (int i = 0; i < 6; i++) {
        Rectangle cr = { lx + (i % 3) * (cw3 + 8), y + 16 + (i / 3) * 34.0f, cw3, 28 };
        if (ui.Chip(cr, RarityName((Rarity)i), RarityColor((Rarity)i), (int)editDraft.rarity == i, 12.5f)) editDraft.rarity = (Rarity)i;
    }
    if (editBuiltin && editedIds.count(editTarget ? editTarget->id : "")) ui.Text("Modificata rispetto all'originale", lx + 2, y + 92, 12.5f, Pal::Gold, true);

    // ---------------- colonna destra ----------------
    float rx = 358, rw = W - rx - 14, colW = (rw - 18) / 2.0f;
    auto row = [&](const char* label, float& v, float mn, float mx, const char* fmt, Rectangle r, Color ac, bool integer) {
        char t[24]; std::snprintf(t, sizeof(t), fmt, v); ui.Text(label, r.x + 2, r.y, 12, Pal::Muted, true); ui.TextR(t, r.x + r.width - 2, r.y - 1, 14, Pal::Text, true);
        return ui.Slider({ r.x, r.y + 14, r.width, 26 }, v, mn, mx, ac, integer);
    };
    ui.Text("STATISTICHE", rx, 66, 12, Pal::Cyan, true);
    { float hp = (float)editDraft.hp, dm = (float)editDraft.dmg;
      if (row("VITA (HP)", hp, 40, 400, "%.0f", { rx, 84, colW, 40 }, Pal::Red, true)) editDraft.hp = (int)hp;
      if (row("DANNO", dm, 1, 60, "%.0f", { rx + colW + 18, 84, colW, 40 }, Pal::Orange, true)) editDraft.dmg = (int)dm;
      row("VELOCITÀ", editDraft.speedMult, 0.5f, 2.4f, "%.2f", { rx, 128, colW, 40 }, Pal::Cyan, false);
      row("TAGLIA", editDraft.baseRadius, 0.75f, 1.7f, "%.2f", { rx + colW + 18, 128, colW, 40 }, Pal::Green, false); }

    ui.Text("COLORE", rx, 176, 12, Pal::Cyan, true);
    static const Color sw[12] = { { 239, 68, 68, 255 }, { 249, 115, 22, 255 }, { 250, 204, 21, 255 }, { 132, 204, 22, 255 }, { 34, 197, 94, 255 }, { 20, 184, 166, 255 },
                                  { 34, 211, 238, 255 }, { 59, 130, 246, 255 }, { 99, 102, 241, 255 }, { 168, 85, 247, 255 }, { 236, 72, 153, 255 }, { 226, 232, 240, 255 } };
    for (int i = 0; i < 12; i++) {
        Rectangle sr = { rx + i * 32.0f, 192, 26, 26 }; bool cur = editDraft.color.r == sw[i].r && editDraft.color.g == sw[i].g && editDraft.color.b == sw[i].b;
        DrawRectangleRounded(sr, 0.35f, 8, sw[i]); if (cur) DrawRectangleRoundedLinesEx({ sr.x - 3, sr.y - 3, sr.width + 6, sr.height + 6 }, 0.35f, 8, 2.5f, WHITE);
        if (ui.Click(sr)) editDraft.color = sw[i];
    }
    { float c3 = (rw - 28) / 3.0f; float cr = editDraft.color.r, cg = editDraft.color.g, cb = editDraft.color.b;
      if (row("ROSSO", cr, 0, 255, "%.0f", { rx, 224, c3, 40 }, { 239, 68, 68, 255 }, true)) editDraft.color.r = (unsigned char)cr;
      if (row("VERDE", cg, 0, 255, "%.0f", { rx + c3 + 14, 224, c3, 40 }, { 34, 197, 94, 255 }, true)) editDraft.color.g = (unsigned char)cg;
      if (row("BLU", cb, 0, 255, "%.0f", { rx + (c3 + 14) * 2, 224, c3, 40 }, { 59, 130, 246, 255 }, true)) editDraft.color.b = (unsigned char)cb; }

    ui.Text("STILE ARMA", rx, 270, 12, Pal::Cyan, true);
    static const char* styId[10] = { "none", "blade", "axe", "spear", "mg", "orbit", "cactus", "lightwing", "shackles", "zone" };
    static const char* styNm[10] = { "Nessuna", "Lame", "Ascia", "Lancia", "Mitra", "Orbite", "Cactus", "Ali", "Catene", "Zona" };
    float sw5 = (rw - 4 * 6.0f) / 5.0f;
    for (int i = 0; i < 10; i++) {
        Rectangle cr = { rx + (i % 5) * (sw5 + 6), 286 + (i / 5) * 32.0f, sw5, 26 };
        if (ui.Chip(cr, styNm[i], Pal::Purple, editDraft.visualStyle == styId[i], 12.5f)) editDraft.visualStyle = styId[i];
    }

    ui.Text("DESCRIZIONE", rx, 350, 12, Pal::Cyan, true);
    Rectangle df = { rx, 366, rw, 36 };
    ui.Panel(df, 12, { 22, 30, 54, 255 }, editFocus == 2 ? Pal::Cyan : Pal::Border, 1.5f, false);
    { std::string ds = ui.Fit(editDraft.desc, rw - 28, 15); if (editDraft.desc.empty()) ui.Text("Cosa fa questa palla?", df.x + 14, df.y + 9, 15, Pal::Dim); else ui.Text(ds.c_str(), df.x + 14, df.y + 9, 15, Pal::Text);
      if (editFocus == 2 && blink) DrawRectangle((int)(df.x + 15 + (editDraft.desc.empty() ? 0.0f : ui.Measure(ds.c_str(), 15).x)), (int)(df.y + 7), 2, 22, Pal::Cyan); }
    if (ui.Click(df)) { fieldClicked = true;
#ifdef __ANDROID__
        nativeTarget = 4; OpenNativeInputDialog("Descrizione", editDraft.desc);
#else
        editFocus = 2;
#endif
    }

    // regole
    char rt[40]; std::snprintf(rt, sizeof(rt), "REGOLE ABILITÀ (%d)", (int)editDraft.scriptRules.size()); ui.Text(rt, rx, 412, 12, Pal::Cyan, true);
    float bx = rx + rw;
#ifdef __ANDROID__
    bx -= 92; if (ui.Button({ bx, 406, 92, 26 }, "Testo", Pal::PanelHi, Pal::Text, 12.5f)) { nativeTarget = 5; OpenNativeInputDialog("Regole (una per riga, formato RULE:)", RulesToText(editDraft.scriptRules)); } bx -= 8;
#endif
    bx -= 92; if (ui.Button({ bx, 406, 92, 26 }, "Incolla", Pal::PanelHi, Pal::Text, 12.5f)) {
        const char* cl = GetClipboardText(); ParsedBallScript ps = ParseAIScript(cl ? cl : "");
        if (!ps.rules.empty()) { editDraft.scriptRules = ps.rules; SetToast(ps.warnings.empty() ? "Regole incollate" : "Regole incollate (" + std::to_string(ps.warnings.size()) + " ignorate)"); }
        else SetToast("Negli appunti non ci sono regole valide");
    }
    bx -= 8; bx -= 92; if (ui.Button({ bx, 406, 92, 26 }, "Copia", Pal::PanelHi, Pal::Text, 12.5f)) { SetClipboardText(RulesToText(editDraft.scriptRules).c_str()); SetToast("Regole copiate negli appunti"); }

    float by = H - 56.0f; Rectangle list = { rx, 438, rw, std::max(52.0f, by - 10 - 438) };
    ui.Panel(list, 12, { 14, 20, 38, 255 }, { 50, 64, 100, 200 }, 1.2f, false);
    float rowH = 28, contentH = std::max(1, (int)editDraft.scriptRules.size()) * rowH + 8, maxScroll = std::max(0.0f, contentH - list.height);
    if (ui.enabled && ui.Inside(list, in.pos)) editScroll -= in.wheel * 40.0f;
    if (ui.enabled && in.down && ui.Inside(list, in.pressPos)) editScroll -= in.dragDy;
    editScroll = std::max(0.0f, std::min(maxScroll, editScroll));
    int del = -1; ui.ScissorBegin({ list.x + 2, list.y + 2, list.width - 4, list.height - 4 });
    if (editDraft.scriptRules.empty()) ui.Wrap(editBuiltin ? "Nessuna regola extra: la palla usa solo la sua abilità integrata. Usa «Modifica con AI» per aggiungerne." : "Nessuna regola. Usa «Modifica con AI» per crearne.", list.x + 14, list.y + 10, list.width - 28, 13.5f, Pal::Dim);
    for (size_t i = 0; i < editDraft.scriptRules.size(); i++) {
        float ry = list.y + 6 + i * rowH - editScroll; if (ry + rowH < list.y || ry > list.y + list.height) continue;
        std::string sm = RuleSummary(editDraft.scriptRules[i]);
        ui.Text(ui.Fit(sm, list.width - 70, 14).c_str(), list.x + 14, ry + 4, 14, Pal::Text);
        Rectangle xr = { list.x + list.width - 44, ry + 1, 32, 24 };
        if (ui.Button(xr, "X", { 120, 32, 48, 255 }, Pal::Text, 12.5f) && ui.Inside(list, in.pos)) del = (int)i;
    }
    ui.ScissorEnd();
    if (del >= 0) editDraft.scriptRules.erase(editDraft.scriptRules.begin() + del);

    // ---------------- barra azioni ----------------
    if (editBuiltin && editTarget && editedIds.count(editTarget->id) && ui.Button({ lx, by, lw, 46 }, "Ripristina originale", { 90, 40, 30, 255 }, Pal::Text, 15)) { ResetEdit(); return; }
    float w3 = (rw - 24) / 3.0f;
    if (ui.Button({ rx, by, w3, 46 }, "PROVA", { 26, 110, 78, 255 }, WHITE, 19)) { StartTraining(editDraft, GameState::EDIT); return; }
    if (ui.Button({ rx + w3 + 12, by, w3, 46 }, "MODIFICA CON AI", Pal::Purple, WHITE, 16.5f)) { OpenAiForEdit(); return; }
    if (ui.Button({ rx + (w3 + 12) * 2, by, w3, 46 }, "SALVA", Pal::Green, { 6, 30, 18, 255 }, 20)) { SaveEdit(); return; }
    if (in.released && !fieldClicked && in.dragDist < 14.0f && ui.enabled) editFocus = 0;
}


// ============================================================================
//  CAMPO DI PROVA: la tua palla (anche non salvata) contro un sacco fermo da 1000 HP, 3 round, uscita libera
// ============================================================================
void PallaGame::StartTraining(const BallDef& b, GameState returnTo) {
    trainBall = b; trainBall.isDummy = false; trainReturn = returnTo; trainRound = 1; for (auto& st : trainStats) st = TrainStat{};
    training = true; simSpeed = 1; StartTrainRound(); SND(SFX_CLICK);
}

void PallaGame::StartTrainRound() {
    float aim = ((float)(rand() % 1000) / 1000.0f - 0.5f) * 1.2f;   // mira un po' diversa a ogni round
    activeMatch.Init({ { &trainBall, nullptr } }, { { &trainDummy, nullptr } }, false, false, aim, 0, 0, 1.0f, 0.0f);
    activeMatch.training = true;   // niente morte improvvisa
    for (auto& f : activeMatch.fighters) if (f.def->isDummy) { f.pos = { 0.0f, 0.0f }; f.anchor = f.pos; }   // al centro: le palle a contatto lo colpiscono piu' spesso
    state = GameState::ARENA;
}

void PallaGame::OnTrainRoundEnded() {
    int dh = 1000; bool killed = false;
    for (const auto& f : activeMatch.fighters) if (f.def->isDummy) { dh = std::max(0, f.hp); killed = f.hp <= 0; }
    TrainStat st; st.time = activeMatch.time; st.dmg = 1000 - dh; st.killed = killed;
    trainStats[std::max(0, std::min(2, trainRound - 1))] = st; SND(killed ? SFX_WIN : SFX_CLICK);
    if (trainRound >= 3) state = GameState::RESULT;
    else { trainRound++; roundBannerTimer = 1.6f; state = GameState::ROUND_END; }
}

void PallaGame::ExitTraining() { training = false; simSpeed = 1; state = trainReturn; SND(SFX_CLICK); }

void PallaGame::TrainHud() {
    int dh = 1000; for (const auto& f : activeMatch.fighters) if (f.def->isDummy) dh = std::max(0, f.hp);
    int dealt = 1000 - dh; float t = std::max(0.1f, activeMatch.time);
    Rectangle l = { 14, 12, 300, 56 }; ui.Panel(l, 16, { 10, 16, 32, 218 }, A(Pal::Green, 0.6f), 1.5f);
    ui.Text("CAMPO DI PROVA", l.x + 16, l.y + 8, 13, Pal::Green, true);
    ui.Text(ui.Fit(trainBall.name, 268, 17, true).c_str(), l.x + 16, l.y + 27, 17, Pal::Text, true);
    char rb[32]; std::snprintf(rb, sizeof(rb), "ROUND %d/3", trainRound);
    ui.Panel({ W * 0.5f - 85, 12, 170, 40 }, 20, { 10, 16, 32, 218 }, Pal::Border, 1.2f); ui.TextC(rb, W * 0.5f, 32, 18, Pal::Text, true);
    char tb[40]; std::snprintf(tb, sizeof(tb), "Tempo %d s", (int)std::ceil(std::max(0.0f, 60.0f - activeMatch.time))); ui.TextC(tb, W * 0.5f, 72, 15, Pal::Muted, true);
    Rectangle r = { W - 314.0f, 12, 300, 66 }; ui.Panel(r, 16, { 10, 16, 32, 218 }, A(Pal::Red, 0.6f), 1.5f);
    ui.Text("SACCO FERMO", r.x + 16, r.y + 8, 13, Pal::Red, true);
    char hb[32]; std::snprintf(hb, sizeof(hb), "%d / 1000", dh); ui.TextR(hb, r.x + r.width - 16, r.y + 6, 16, Pal::Text, true);
    ui.Bar({ r.x + 16, r.y + 30, r.width - 32, 10 }, (float)dh / 1000.0f, Pal::Red, { 40, 48, 72, 255 });
    char db[64]; std::snprintf(db, sizeof(db), "Danno %d   -   %.0f al secondo", dealt, (float)dealt / t); ui.Text(db, r.x + 16, r.y + 45, 13, Pal::Muted);
}
