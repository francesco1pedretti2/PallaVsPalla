#include "ai_client.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#elif defined(__ANDROID__)
#include "raymob.h"
#include <jni.h>
#endif

// ============================================================================
//  JSON: scrittura (solo ASCII, cosi' JNI/WinHTTP non vedono mai UTF-8 "strano")
// ============================================================================
static void AppendU16(std::string& o, unsigned v) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", v & 0xFFFF); o += b; }

static std::string JsonEscapeAscii(const std::string& s) {
    std::string out; out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            if (c == '"') out += "\\\""; else if (c == '\\') out += "\\\\"; else if (c == '\n') out += "\\n";
            else if (c == '\r') {} else if (c == '\t') out += "\\t"; else if (c < 32) {} else out.push_back((char)c);
            i++; continue;
        }
        int n = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 0;
        if (n == 0 || i + n > s.size()) { out += "?"; i++; continue; }
        unsigned cp = (n == 2) ? (c & 0x1F) : (n == 3) ? (c & 0x0F) : (c & 0x07);
        bool bad = false;
        for (int k = 1; k < n; k++) { unsigned char cc = (unsigned char)s[i + k]; if ((cc & 0xC0) != 0x80) { bad = true; break; } cp = (cp << 6) | (cc & 0x3F); }
        if (bad) { out += "?"; i++; continue; }
        i += n;
        if (cp >= 0x10000) { cp -= 0x10000; AppendU16(out, 0xD800 + (cp >> 10)); AppendU16(out, 0xDC00 + (cp & 0x3FF)); }
        else AppendU16(out, cp);
    }
    return out;
}

// ============================================================================
//  JSON: lettura (parser completo, con \uXXXX e coppie surrogate)
// ============================================================================
struct JV {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false; double n = 0; std::string s;
    std::vector<JV> a; std::vector<std::pair<std::string, JV>> o;
    const JV* Get(const char* k) const { if (t != Obj) return nullptr; for (const auto& kv : o) if (kv.first == k) return &kv.second; return nullptr; }
};
static void AppendUtf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o.push_back((char)cp);
    else if (cp < 0x800) { o.push_back((char)(0xC0 | (cp >> 6))); o.push_back((char)(0x80 | (cp & 0x3F))); }
    else if (cp < 0x10000) { o.push_back((char)(0xE0 | (cp >> 12))); o.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); o.push_back((char)(0x80 | (cp & 0x3F))); }
    else { o.push_back((char)(0xF0 | (cp >> 18))); o.push_back((char)(0x80 | ((cp >> 12) & 0x3F))); o.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); o.push_back((char)(0x80 | (cp & 0x3F))); }
}
struct JParser {
    const char* p; const char* e; bool fail = false;
    void ws() { while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool lit(const char* w) { size_t n = std::strlen(w); if ((size_t)(e - p) >= n && std::strncmp(p, w, n) == 0) { p += n; return true; } return false; }
    unsigned hex4() { unsigned v = 0; for (int i = 0; i < 4; i++) { if (p >= e) { fail = true; return 0; } char c = *p++; v <<= 4; if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10; else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else { fail = true; return 0; } } return v; }
    bool str(std::string& out) {
        if (p >= e || *p != '"') { fail = true; return false; } p++;
        while (p < e && *p != '"') {
            if (*p == '\\') {
                p++; if (p >= e) { fail = true; return false; } char c = *p++;
                switch (c) {
                    case 'n': out.push_back('\n'); break; case 't': out.push_back('\t'); break; case 'r': break;
                    case 'b': case 'f': break; case '/': out.push_back('/'); break; case '"': out.push_back('"'); break; case '\\': out.push_back('\\'); break;
                    case 'u': { unsigned cp = hex4(); if (cp >= 0xD800 && cp < 0xDC00 && (e - p) >= 6 && p[0] == '\\' && p[1] == 'u') { p += 2; unsigned lo = hex4(); if (lo >= 0xDC00 && lo < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); } AppendUtf8(out, cp); break; }
                    default: out.push_back(c);
                }
            } else out.push_back(*p++);
        }
        if (p >= e) { fail = true; return false; } p++; return true;
    }
    bool val(JV& v, int depth) {
        if (depth > 40) { fail = true; return false; }
        ws(); if (p >= e) { fail = true; return false; }
        if (*p == '{') {
            p++; v.t = JV::Obj; ws(); if (p < e && *p == '}') { p++; return true; }
            while (!fail) {
                ws(); std::string k; if (!str(k)) return false; ws(); if (p >= e || *p != ':') { fail = true; return false; } p++;
                JV c; if (!val(c, depth + 1)) return false; v.o.push_back({ k, std::move(c) }); ws();
                if (p < e && *p == ',') { p++; continue; } if (p < e && *p == '}') { p++; return true; } fail = true; return false;
            }
            return false;
        }
        if (*p == '[') {
            p++; v.t = JV::Arr; ws(); if (p < e && *p == ']') { p++; return true; }
            while (!fail) {
                JV c; if (!val(c, depth + 1)) return false; v.a.push_back(std::move(c)); ws();
                if (p < e && *p == ',') { p++; continue; } if (p < e && *p == ']') { p++; return true; } fail = true; return false;
            }
            return false;
        }
        if (*p == '"') { v.t = JV::Str; return str(v.s); }
        if (lit("true")) { v.t = JV::Bool; v.b = true; return true; }
        if (lit("false")) { v.t = JV::Bool; v.b = false; return true; }
        if (lit("null")) { v.t = JV::Null; return true; }
        char* endp = nullptr; double d = std::strtod(p, &endp);
        if (endp == p) { fail = true; return false; } p = endp; v.t = JV::Num; v.n = d; return true;
    }
};
static bool ParseJson(const std::string& s, JV& out) { JParser jp{ s.data(), s.data() + s.size() }; bool ok = jp.val(out, 0); return ok && !jp.fail; }

// ============================================================================
//  Chiave API
// ============================================================================
static bool KeyCh(char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.'; }
std::string SanitizeApiKey(const std::string& raw) {
    std::vector<std::string> toks; std::string cur;
    for (char c : raw) { if (KeyCh(c)) cur.push_back(c); else { if (!cur.empty()) toks.push_back(cur); cur.clear(); } }
    if (!cur.empty()) toks.push_back(cur);
    std::string best; for (const auto& t : toks) if (t.size() > best.size()) best = t;
    return best;
}

// ============================================================================
//  HTTP (WinHTTP su Windows, HttpURLConnection via JNI su Android)
// ============================================================================
struct HttpResp { int status = 0; std::string body, err; };
static const char* kHost = "generativelanguage.googleapis.com";

#ifdef _WIN32
static HttpResp HttpCall(const char* method, const std::string& path, const std::string& key, const std::string& body) {
    HttpResp r;
    HINTERNET hS = WinHttpOpen(L"PallaVsPalla/3.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) { r.err = "WinHTTP non disponibile"; return r; }
    DWORD protos = 0x00000800 | 0x00002000; // TLS 1.2 | TLS 1.3 (valori numerici: compatibili con SDK vecchi)
    WinHttpSetOption(hS, WINHTTP_OPTION_SECURE_PROTOCOLS, &protos, sizeof(protos));
    WinHttpSetTimeouts(hS, 15000, 15000, 30000, 120000);
    std::wstring wHost(kHost, kHost + std::strlen(kHost)), wPath(path.begin(), path.end()), wMethod(method, method + std::strlen(method));
    HINTERNET hC = WinHttpConnect(hS, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hC) { r.err = "connessione fallita"; WinHttpCloseHandle(hS); return r; }
    HINTERNET hR = WinHttpOpenRequest(hC, wMethod.c_str(), wPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hR) { r.err = "richiesta non creata"; WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return r; }
    std::wstring hdr = L"Content-Type: application/json\r\nx-goog-api-key: " + std::wstring(key.begin(), key.end()) + L"\r\n";
    BOOL ok = WinHttpSendRequest(hR, hdr.c_str(), (DWORD)-1L, body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)
              && WinHttpReceiveResponse(hR, nullptr);
    if (!ok) {
        DWORD ec = GetLastError(); char b[96];
        std::snprintf(b, sizeof(b), "errore di rete WinHTTP %lu%s", (unsigned long)ec, ec == 12002 ? " (timeout)" : (ec == 12007 || ec == 12029) ? " (nessuna connessione)" : "");
        r.err = b;
    } else {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(hR, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
        r.status = (int)code;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hR, &avail) && avail > 0) {
            std::string chunk(avail, '\0'); DWORD rd = 0;
            if (!WinHttpReadData(hR, &chunk[0], avail, &rd) || rd == 0) break;
            r.body.append(chunk.data(), rd);
        }
    }
    WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
    return r;
}
#elif defined(__ANDROID__)
static HttpResp HttpCall(const char* method, const std::string& path, const std::string& key, const std::string& body) {
    HttpResp r;
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) { r.err = "JVM non attiva"; return r; }
    JNIEnv* env = nullptr; bool att = false;
    if (app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) { r.err = "JNI non disponibile"; return r; } att = true;
    }
    jclass cls = env->GetObjectClass(app->activity->clazz);
    jmethodID mid = env->GetStaticMethodID(cls, "callHttp", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!mid) { r.err = "callHttp non trovato (patch Android mancante)"; }
    else {
        std::string url = std::string("https://") + kHost + path;
        jstring jm = env->NewStringUTF(method), ju = env->NewStringUTF(url.c_str()), jk = env->NewStringUTF(key.c_str()), jb = env->NewStringUTF(body.c_str());
        jstring jr = (jstring)env->CallStaticObjectMethod(cls, mid, jm, ju, jk, jb);
        if (env->ExceptionCheck()) { env->ExceptionClear(); r.err = "eccezione Java"; }
        else if (jr) {
            const char* u = env->GetStringUTFChars(jr, nullptr);
            if (u) {
                std::string raw = u; env->ReleaseStringUTFChars(jr, u);
                size_t nl = raw.find('\n');
                if (nl != std::string::npos) { r.status = std::atoi(raw.substr(0, nl).c_str()); r.body = raw.substr(nl + 1); }
                else r.err = "risposta non valida";
                if (r.status == 0 && r.err.empty()) r.err = r.body.empty() ? "nessuna risposta" : r.body.substr(0, 120);
            }
            env->DeleteLocalRef(jr);
        }
        env->DeleteLocalRef(jm); env->DeleteLocalRef(ju); env->DeleteLocalRef(jk); env->DeleteLocalRef(jb);
    }
    env->DeleteLocalRef(cls); if (att) app->activity->vm->DetachCurrentThread();
    return r;
}
#else
static HttpResp HttpCall(const char*, const std::string&, const std::string&, const std::string&) { HttpResp r; r.err = "piattaforma non supportata"; return r; }
#endif

// ============================================================================
//  Scelta del modello: scoperta dinamica (i modelli Gemini vengono spenti spesso!)
// ============================================================================
static std::string g_model; // modello che ha funzionato l'ultima volta

static std::string Lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
static bool Has(const std::string& s, const char* w) { return s.find(w) != std::string::npos; }

static std::string ErrMsg(const std::string& body) {
    JV root; if (ParseJson(body, root)) { const JV* e = root.Get("error"); if (e) { const JV* m = e->Get("message"); if (m && m->t == JV::Str) return m->s.substr(0, 260); } }
    std::string t = body.substr(0, 160); for (char& c : t) if (c == '\n' || c == '\r') c = ' '; return t;
}

static std::vector<std::string> RankModels(const JV& root) {
    struct C { std::string name; double score; };
    std::vector<C> list;
    const JV* models = root.Get("models");
    if (models && models->t == JV::Arr) for (const JV& m : models->a) {
        const JV* nm = m.Get("name"); if (!nm || nm->t != JV::Str) continue;
        std::string name = nm->s; if (name.rfind("models/", 0) == 0) name = name.substr(7);
        std::string low = Lower(name);
        if (low.rfind("gemini-", 0) != 0 || !Has(low, "flash")) continue;
        static const char* bad[] = { "image", "tts", "live", "audio", "embedding", "robotics", "native", "computer", "imagen", "veo", "vision", "learnlm", "thinking" };
        bool skip = false; for (const char* b : bad) if (Has(low, b)) skip = true; if (skip) continue;
        const JV* meth = m.Get("supportedGenerationMethods"); bool gen = false;
        if (meth && meth->t == JV::Arr) for (const JV& x : meth->a) if (x.t == JV::Str && x.s == "generateContent") gen = true;
        if (!gen) continue;
        bool lite = Has(low, "lite"), preview = Has(low, "preview") || Has(low, "exp"), alias = Has(low, "latest");
        double ver = 0; if (low.size() > 7 && std::isdigit((unsigned char)low[7])) ver = std::atof(low.c_str() + 7);
        double base = alias ? (lite ? 35.0 : 1000.0) : (lite ? (preview ? 20.0 : 40.0) : (preview ? 60.0 : 100.0));
        double score = base + ver * 10.0;
        if (!alias && low.size() > 4 && std::isdigit((unsigned char)low[low.size() - 1]) && std::isdigit((unsigned char)low[low.size() - 2])) score -= 3.0; // snapshot datati (-001)
        list.push_back({ name, score });
    }
    std::sort(list.begin(), list.end(), [](const C& a, const C& b) { return a.score > b.score; });
    std::vector<std::string> out; for (const auto& c : list) { if (out.size() >= 5) break; out.push_back(c.name); }
    return out;
}

static std::string AuthMessage(int status, const std::string& msg) {
    if (status == 400 && (Has(Lower(msg), "api key") || Has(Lower(msg), "api_key"))) return "La API Key non è valida. Controlla di averla copiata per intero da aistudio.google.com/apikey.";
    if (status == 401) return "La API Key è stata rifiutata (401). Creane una nuova su aistudio.google.com/apikey.";
    return "La API Key non ha i permessi necessari (403): " + msg;
}

// Elenca i modelli disponibili con questa chiave. Ritorna false e riempie R se la chiave e' il problema.
static bool DiscoverModels(const std::string& key, AiResult& R, std::vector<std::string>& out) {
    HttpResp h = HttpCall("GET", "/v1beta/models?pageSize=200", key, "");
    R.status = h.status;
    if (h.status == 0) { R.error = "Nessuna connessione a Gemini (" + h.err + "). Controlla internet o il firewall."; return false; }
    if (h.status == 200) {
        JV root; if (ParseJson(h.body, root)) out = RankModels(root);
        return true;
    }
    std::string msg = ErrMsg(h.body);
    if (h.status == 400 || h.status == 401 || h.status == 403) { R.keyProblem = true; R.error = AuthMessage(h.status, msg); return false; }
    R.error = "Errore Gemini (HTTP " + std::to_string(h.status) + "): " + msg; return false;
}

// ============================================================================
//  Prompt di sistema
// ============================================================================
static const char* kPromptA = R"PROMPT(Sei l'AI Creator di "Palla VS Palla 3D": battaglie tra palline che rimbalzano in un'arena quadrata vista dall'alto. L'utente descrive una palla, tu la progetti con statistiche e regole in un formato testuale RIGIDO.

FORMATO (obbligatorio): rispondi SOLO con queste righe, una per riga, senza markdown, senza ```, senza altro testo. Ad ogni richiesta (anche modifiche) riscrivi la palla COMPLETA aggiornata.
CHAT: <1-2 frasi in italiano che spiegano cosa fa la palla>
NAME: <nome breve, max 22 caratteri>
RARITY: <RARO|SUPER RARO|EPICO|MITICO|LEGGENDARIO|ULTRA>
HP: <60..400>
DMG: <3..40>   (danno da contatto)
SPEED: <0.6..2.0>
SIZE: <0.8..1.6>
COLOR: <R,G,B>
STYLE: <none|blade|axe|spear|mg|orbit|cactus|lightwing|shackles|zone>
RULE: trig=<TRIG> act=<ACT> [parametri]   (da 1 a 10 righe RULE)

I parametri di RULE sono chiave=valore. Valori SEMPRE tra virgolette doppie e possono essere espressioni matematiche con le variabili. Chiavi: cond int amt dur cnt x y ang spd rad w h target str var bnc rep home pierce once track ign.
Predefiniti: cond="1" int="1.5" amt="10" dur="3" cnt="1" x="x" y="y" ang="angle" spd="12" rad="2.5" w="3" h="0.6" target=nearest str=dmg var=0 bnc=0 rep=1 home=0 pierce=0 once=0 track=1 ign=0.

ARENA: coordinate da -7 a +7 su x e y (14x14). Le palle partono a x=-3.8 (alleato) e x=+3.8 (nemico) e rimbalzano a 9-14 unita/s. Angoli in radianti (0 = destra). "angle" e' l'angolo verso il nemico piu' vicino.

TRIGGER: ON_START (una volta a inizio round) | EVERY (ogni int secondi, minimo 0.15) | ON_FRAME (ogni frame: solo cose leggere, MAI danni) | ON_WALL (rimbalzo sul muro) | ON_HIT (colpisce un nemico a contatto) | ON_HURT (subisce danno) | ON_KILL (uccide un nemico) | WHEN (quando cond diventa vera; usa once=1 per effetti singoli).
)PROMPT";

static const char* kPromptB = R"PROMPT(AZIONI:
PROJ: spara cnt proiettili verso ang con velocita spd, danno amt, raggio rad (0.1-0.6), bnc rimbalzi sui muri, home=1 inseguono, pierce=1 trapassano.
LASER: cnt raggi verso ang, danno amt, bnc rimbalzi (0-6).
ULTRA_BEAM: raggio continuo per dur secondi, danno amt a tick, bnc rimbalzi, track=1 segue il nemico (0 = ruota).
BLACK_HOLE: in (x,y), dur secondi, raggio rad, danno amt a tick, spd = forza di attrazione.
WALL_BARRIER: muro temporaneo centrato in (x,y), dimensioni w h, dur, bnc=1 riflette i proiettili.
GRAVITY_FLIP: per dur secondi accelera i nemici di (x,y) (es. x="0" y="9"); target=all colpisce anche te.
TIME_SCALE: per dur secondi i nemici vanno a velocita amt (0 = tempo fermo, 1 = normale); target=all colpisce anche te.
TELEPORT: va in (x,y), oppure target=behind per apparire dietro al nemico.
DASH: scatto verso ang con velocita spd. INVERT_VEL: target=self|enemy. STUN / SLOW: dur. POISON: dur e amt (danno/s).
DAMAGE: amt al nemico (target=all per tutti, ign=1 ignora lo scudo). HEAL: amt. STEAL_HP: danneggia e ti cura di amt. STEAL_ABILITY: copia le regole del nemico (usa once=1).
EXPLODE e SHOCKWAVE: danno amt in area rad attorno a te. FREEZE_PROJ: distrugge i proiettili nemici.
SPAWN_MINION: cnt mini-cloni (max 3), amt = loro HP. SPAWN_TURRET: torretta con danno amt per dur secondi.
HAZARD: str=web|darkweb|frost|acid|shackle|dice|spike|bomb in (x,y), raggio rad, durata dur, valore amt.
PULL / PUSH: tira o spinge il nemico con velocita spd. SET_POS: x y. SET_VEL: x y = velocita. DRAW_FX: str = testo sopra la palla.
SET_VAR: var=0..5 amt=valore. ADD_STAT: str=hp|maxhp|dmg|speed|r|blades|shield|regen|reflect e amt = variazione.

VARIABILI: hp maxhp hppct dmg speed r x y vx vy blades shield ehp emaxhp ehppct ex ey evx evy er dist angle time bounces hits hurts ax ay aw ah allies enemies i v0..v5 pi rand sin cos abs sqrt min max clamp floor ceil round sign pow atan2 lerp rand(a,b). Operatori: + - * / % ^ < > <= >= == != && || !

BILANCIAMENTO: HP 90-160 e DMG 4-12 per palle normali; proiettili 4-14 di danno; effetti enormi (buco nero, tempo fermo, raggio ultra) solo con int>=6 oppure WHEN con once=1. La rarita riflette la potenza: RARO debole ... ULTRA fortissima. Colori vivaci e diversi tra loro.

ESEMPIO 1 (utente: "spara 3 proiettili a ventaglio ogni 2 secondi e diventa veloce con poca vita"):
CHAT: Spara 3 proiettili a ventaglio ogni 2 secondi e, sotto il 30% di vita, scatta in una corsa furiosa.
NAME: Palla Ventaglio
RARITY: EPICO
HP: 120
DMG: 7
SPEED: 1.0
SIZE: 1.1
COLOR: 255,140,60
STYLE: none
RULE: trig=EVERY act=PROJ int="2" amt="7" cnt="3" ang="angle" spd="13" rad="0.24"
RULE: trig=WHEN act=ADD_STAT cond="hppct<30" str="speed" amt="0.4" once="1"

ESEMPIO 2 (utente: "buco nero sul nemico e ferma il tempo quando sta per morire"):
CHAT: Ogni 8 secondi apre un buco nero sul nemico e, sotto il 35% di vita, ferma il tempo per 2 secondi.
NAME: Palla Singolarita
RARITY: ULTRA
HP: 130
DMG: 8
SPEED: 1.0
SIZE: 1.2
COLOR: 168,85,247
STYLE: orbit
RULE: trig=EVERY act=BLACK_HOLE int="8" amt="4" x="ex" y="ey" dur="3" rad="3" spd="12"
RULE: trig=WHEN act=TIME_SCALE cond="hppct<35" amt="0" dur="2" once="1"
)PROMPT";

static std::string BuildBody(const std::vector<std::pair<bool, std::string>>& turns, const std::string& curScript, const std::string& model) {
    std::string sys = std::string(kPromptA) + kPromptB;
    if (!curScript.empty()) sys += "\nPALLA ATTUALE (se l'utente chiede modifiche, parti da questa):\n" + curScript + "\n";
    std::string b = "{\"systemInstruction\":{\"parts\":[{\"text\":\"" + JsonEscapeAscii(sys) + "\"}]},\"contents\":[";
    for (size_t i = 0; i < turns.size(); i++) {
        if (i) b += ",";
        b += std::string("{\"role\":\"") + (turns[i].first ? "user" : "model") + "\",\"parts\":[{\"text\":\"" + JsonEscapeAscii(turns[i].second) + "\"}]}";
    }
    b += "],\"generationConfig\":{\"maxOutputTokens\":8192";
    if (model.rfind("gemini-2.5", 0) == 0) b += ",\"thinkingConfig\":{\"thinkingBudget\":0}"; // risposta rapida: niente "ragionamento" lungo
    b += "}}";
    return b;
}

// Estrae il testo (tutte le parti non-"thought") dalla prima candidata.
static bool ExtractReply(const std::string& body, std::string& text, std::string& whyEmpty) {
    JV root; if (!ParseJson(body, root)) { whyEmpty = "risposta non leggibile"; return false; }
    const JV* pf = root.Get("promptFeedback");
    if (pf) { const JV* br = pf->Get("blockReason"); if (br && br->t == JV::Str) { whyEmpty = "richiesta bloccata da Gemini (" + br->s + "): riformula la descrizione"; return false; } }
    const JV* cands = root.Get("candidates");
    if (!cands || cands->t != JV::Arr || cands->a.empty()) { whyEmpty = "nessuna risposta dal modello"; return false; }
    const JV& c0 = cands->a[0];
    const JV* content = c0.Get("content"); const JV* parts = content ? content->Get("parts") : nullptr;
    if (parts && parts->t == JV::Arr) for (const JV& p : parts->a) {
        const JV* th = p.Get("thought"); if (th && th->t == JV::Bool && th->b) continue;
        const JV* t = p.Get("text"); if (t && t->t == JV::Str) text += t->s;
    }
    if (text.empty()) {
        const JV* fr = c0.Get("finishReason"); whyEmpty = std::string("risposta vuota") + ((fr && fr->t == JV::Str) ? " (" + fr->s + ")" : "");
        return false;
    }
    return true;
}

AiResult GeminiGenerate(const std::string& apiKeyRaw, const std::vector<ChatTurn>& history, const std::string& curScript) {
    AiResult R; std::string key = SanitizeApiKey(apiKeyRaw);
    if (key.size() < 20) { R.keyProblem = true; R.error = "Manca la API Key (o è troppo corta). Toccala in alto e incollala per intero."; return R; }

    // Storia valida per Gemini: parte da "user", ruoli alternati, niente messaggi di errore.
    std::vector<std::pair<bool, std::string>> turns;
    for (const auto& t : history) {
        if (t.isError || t.text.empty()) continue;
        if (turns.empty() && !t.isUser) continue;
        if (!turns.empty() && turns.back().first == t.isUser) turns.back().second += "\n" + t.text; else turns.push_back({ t.isUser, t.text });
    }
    while (turns.size() > 14) turns.erase(turns.begin());
    while (!turns.empty() && !turns.front().first) turns.erase(turns.begin());
    if (turns.empty() || !turns.back().first) { R.error = "Scrivi prima cosa deve fare la palla."; return R; }

    std::vector<std::string> cands; bool discovered = false;
    if (!g_model.empty()) cands.push_back(g_model); else cands.push_back("gemini-flash-latest");
    std::string lastErr = "nessun modello disponibile";
    int attempts = 0;
    for (size_t i = 0; i < cands.size() && attempts < 10; i++) {
        const std::string model = cands[i];
        for (int retry = 0; retry < 2; retry++) {
            attempts++;
            HttpResp h = HttpCall("POST", "/v1beta/models/" + model + ":generateContent", key, BuildBody(turns, curScript, model));
            R.status = h.status;
            if (h.status == 0) { R.error = "Nessuna connessione a Gemini (" + h.err + "). Controlla internet o il firewall."; return R; }
            if (h.status == 200) {
                std::string text, why;
                if (ExtractReply(h.body, text, why)) { R.ok = true; R.text = text; R.model = model; g_model = model; return R; }
                lastErr = why; if (Has(why, "bloccata")) { R.error = "Gemini: " + why; return R; }
                break;
            }
            std::string msg = ErrMsg(h.body);
            if (h.status == 401 || h.status == 403 || (h.status == 400 && (Has(Lower(msg), "api key") || Has(Lower(msg), "api_key")))) { R.keyProblem = true; R.error = AuthMessage(h.status, msg); return R; }
            lastErr = "HTTP " + std::to_string(h.status) + ": " + msg;
            if (h.status == 404) { if (model == g_model) g_model.clear(); break; }
            if ((h.status == 429 || h.status >= 500) && retry == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1800)); continue; }
            break;
        }
        if (i + 1 >= cands.size() && !discovered) {
            discovered = true; std::vector<std::string> found;
            if (!DiscoverModels(key, R, found)) return R;
            for (const auto& f : found) if (std::find(cands.begin(), cands.end(), f) == cands.end()) cands.push_back(f);
        }
    }
    R.error = "Gemini non ha risposto (" + lastErr + ")";
    if (Has(lastErr, "HTTP 429")) R.error = "Limite di richieste di Gemini raggiunto (429). Aspetta un minuto e riprova.";
    return R;
}

AiResult GeminiCheckKey(const std::string& apiKeyRaw) {
    AiResult R; std::string key = SanitizeApiKey(apiKeyRaw);
    if (key.size() < 20) { R.keyProblem = true; R.error = "La chiave è troppo corta: incollala per intero."; return R; }
    std::vector<std::string> found;
    if (!DiscoverModels(key, R, found)) return R;
    if (found.empty()) { R.error = "La chiave funziona ma non vedo modelli Gemini Flash disponibili."; return R; }
    R.ok = true; R.model = found[0]; if (g_model.empty()) g_model = found[0];
    return R;
}

// ============================================================================
//  Tastiera nativa Android
// ============================================================================
void OpenNativeInputDialog(const std::string& title, const std::string& initialText) {
#ifdef __ANDROID__
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) return;
    JNIEnv* env = nullptr; bool att = false;
    if (app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) { if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return; att = true; }
    jclass cls = env->GetObjectClass(app->activity->clazz);
    jmethodID mid = env->GetMethodID(cls, "openInputDialog", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (mid) {
        // Il testo iniziale puo' contenere caratteri fuori dal BMP: lo filtriamo in ASCII+Latin1 per non rompere NewStringUTF.
        std::string safe; for (size_t i = 0; i < initialText.size(); i++) { unsigned char c = (unsigned char)initialText[i]; if (c < 0x80 || c == 0xC2 || c == 0xC3 || (c >= 0x80 && c < 0xC0 && i > 0 && ((unsigned char)initialText[i - 1] == 0xC2 || (unsigned char)initialText[i - 1] == 0xC3))) safe.push_back((char)c); }
        jstring jt = env->NewStringUTF(title.c_str()), ji = env->NewStringUTF(safe.c_str());
        env->CallVoidMethod(app->activity->clazz, mid, jt, ji);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(jt); env->DeleteLocalRef(ji);
    }
    env->DeleteLocalRef(cls); if (att) app->activity->vm->DetachCurrentThread();
#else
    (void)title; (void)initialText;
#endif
}

bool PollNativeInputDialog(std::string& outText) {
#ifdef __ANDROID__
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) return false;
    JNIEnv* env = nullptr; bool att = false;
    if (app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) { if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false; att = true; }
    bool got = false;
    jclass cls = env->GetObjectClass(app->activity->clazz);
    static jmethodID mid = nullptr;
    if (!mid) { mid = env->GetMethodID(cls, "pollInputDialog", "()Ljava/lang/String;"); if (env->ExceptionCheck()) env->ExceptionClear(); }
    if (mid) {
        jstring jr = (jstring)env->CallObjectMethod(app->activity->clazz, mid);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (jr) { const char* u = env->GetStringUTFChars(jr, nullptr); if (u) { outText = u; got = true; env->ReleaseStringUTFChars(jr, u); } env->DeleteLocalRef(jr); }
    }
    env->DeleteLocalRef(cls); if (att) app->activity->vm->DetachCurrentThread();
    return got;
#else
    (void)outText; return false;
#endif
}
