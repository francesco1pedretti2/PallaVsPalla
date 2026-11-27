#include "ai_client.hpp"
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#elif defined(__ANDROID__)
#include "raymob.h"
#include <jni.h>
#endif

static std::string JsonEscape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c=='"') out+="\\\""; else if (c=='\\') out+="\\\\"; else if (c=='\n') out+="\\n";
        else if (c=='\r') {} else if (c=='\t') out+=" "; else if (c>=32) out.push_back((char)c);
    }
    return out;
}
static std::string ExtractField(const std::string& json, const std::string& key) {
    std::string n="\""+key+"\""; size_t p=json.find(n); if(p==std::string::npos)return "";
    p=json.find(':',p+n.size()); if(p==std::string::npos)return "";
    p=json.find('"',p+1); if(p==std::string::npos)return ""; p++;
    std::string res;
    for (;p<json.size();p++) {
        char c=json[p];
        if (c=='\\'&&p+1<json.size()) { char nx=json[++p]; if(nx=='n')res+='\n'; else if(nx=='t')res+=' '; else res+=nx; }
        else if (c=='"') break; else res+=c;
    }
    return res;
}

void OpenNativeInputDialog(const std::string& title, const std::string& initialText) {
#ifdef __ANDROID__
    struct android_app* app = GetAndroidApp();
    if (!app||!app->activity||!app->activity->vm) return;
    JNIEnv* env=nullptr; bool att=false;
    if (app->activity->vm->GetEnv((void**)&env,JNI_VERSION_1_6)!=JNI_OK) { if(app->activity->vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)return; att=true; }
    jclass cls=env->GetObjectClass(app->activity->clazz);
    jmethodID mid=env->GetMethodID(cls,"openInputDialog","(Ljava/lang/String;Ljava/lang/String;)V");
    if (mid) {
        jstring jt=env->NewStringUTF(title.c_str()), ji=env->NewStringUTF(initialText.c_str());
        env->CallVoidMethod(app->activity->clazz,mid,jt,ji);
        env->DeleteLocalRef(jt); env->DeleteLocalRef(ji);
    }
    env->DeleteLocalRef(cls); if(att) app->activity->vm->DetachCurrentThread();
#else
    (void)title; (void)initialText;
#endif
}

bool PollNativeInputDialog(std::string& outText) {
#ifdef __ANDROID__
    struct android_app* app = GetAndroidApp();
    if (!app||!app->activity||!app->activity->vm) return false;
    JNIEnv* env=nullptr; bool att=false;
    if (app->activity->vm->GetEnv((void**)&env,JNI_VERSION_1_6)!=JNI_OK) { if(app->activity->vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)return false; att=true; }
    bool got=false;
    jclass cls=env->GetObjectClass(app->activity->clazz);
    jmethodID mid=env->GetMethodID(cls,"pollInputDialog","()Ljava/lang/String;");
    if (mid) {
        jstring jr=(jstring)env->CallObjectMethod(app->activity->clazz,mid);
        if (jr) { const char* u=env->GetStringUTFChars(jr,nullptr); if(u){outText=u; got=true; env->ReleaseStringUTFChars(jr,u);} env->DeleteLocalRef(jr); }
    }
    env->DeleteLocalRef(cls); if(att) app->activity->vm->DetachCurrentThread();
    return got;
#else
    (void)outText; return false;
#endif
}

std::string CallGeminiChatAPI(const std::string& apiKey, const std::vector<ChatTurn>& history, const std::string& curScript) {
    if (apiKey.empty()) return "ERR: Inserisci la tua API Key di Google AI Studio!";
    std::ostringstream b;
    std::string sys = "Sei l'AI Creator di 'Palla VS Palla 3D' (arena 3D 60x60 con x,y da -7 a +7). Rispondi SOLO in questo formato puro (niente ```):\n"
                      "CHAT: <spiegazione in italiano>\nNAME: <nome>\nRARITY: <RARO|SUPER RARO|EPICO|MITICO|LEGGENDARIO|ULTRA>\n"
                      "HP: <50..999>\nDMG: <2..150>\nSPEED: <0.6..2.5>\nSIZE: <0.8..1.7>\nCOLOR: <R,G,B>\n"
                      "STYLE: <none|blade|axe|spear|mg|orbit|cactus|lightwing|shackles|zone>\n"
                      "RULE: trig=<TRIG> act=<ACT> cond=\"<expr>\" int=\"<expr>\" amt=\"<expr>\" dur=\"<expr>\" cnt=\"<expr>\" x=\"<expr>\" y=\"<expr>\" ang=\"<expr>\" spd=\"<expr>\" rad=\"<expr>\" w=\"<expr>\" h=\"<expr>\" target=<nearest|all|self|enemy|behind> str=<arg> var=<0..5> bnc=<0..6> rep=<1..12> home=<0|1> pierce=<0|1> once=<0|1> track=<0|1> ign=<0|1>\n"
                      "TRIG: ON_START, EVERY, ON_FRAME, ON_WALL, ON_HIT, ON_HURT, ON_KILL, WHEN\n"
                      "ACT: SET_VAR, ADD_STAT, DAMAGE, HEAL, STEAL_HP, STEAL_ABILITY, PROJ, LASER, ULTRA_BEAM, BLACK_HOLE, WALL_BARRIER, GRAVITY_FLIP, TIME_SCALE, TELEPORT, DASH, INVERT_VEL, STUN, SLOW, POISON, FREEZE_PROJ, EXPLODE, SHOCKWAVE, SPAWN_MINION, SPAWN_TURRET, HAZARD, PULL, PUSH, SET_POS, SET_VEL, DRAW_FX\n"
                      "Variabili: hp maxhp hppct dmg speed r x y vx vy blades shield ehp emaxhp ehppct ex ey evx evy er dist angle time bounces hits hurts ax ay aw ah allies enemies i v0..v5 pi rand sin cos abs sqrt min max clamp floor\n";
    if (!curScript.empty()) sys += "Stato attuale:\n" + curScript;
    b << "{\"systemInstruction\":{\"parts\":[{\"text\":\"" << JsonEscape(sys) << "\"}]},\"contents\":[";
    for (size_t i=0;i<history.size();i++) {
        if (i>0) b<<",";
        b<<"{\"role\":\""<<(history[i].isUser?"user":"model")<<"\",\"parts\":[{\"text\":\""<<JsonEscape(history[i].text)<<"\"}]}";
    }
    b << "]}";
    std::string body = b.str(), raw;

#ifdef _WIN32
    HINTERNET hS = WinHttpOpen(L"PallaVsPalla/3D", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) return "ERR: WinHTTP non disponibile";
    HINTERNET hC = WinHttpConnect(hS, L"generativelanguage.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    std::wstring wHdr = L"Content-Type: application/json\r\nx-goog-api-key: " + std::wstring(apiKey.begin(), apiKey.end()) + L"\r\n";
    const wchar_t* models[] = { L"/v1beta/models/gemini-2.5-flash:generateContent", L"/v1beta/models/gemini-2.5-flash-lite:generateContent", L"/v1beta/models/gemini-2.0-flash:generateContent" };
    for (int m=0; m<3; m++) {
        HINTERNET hR = WinHttpOpenRequest(hC, L"POST", models[m], nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (WinHttpSendRequest(hR, wHdr.c_str(), (DWORD)-1L, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) && WinHttpReceiveResponse(hR, nullptr)) {
            DWORD sz=0; raw.clear();
            while (WinHttpQueryDataAvailable(hR, &sz) && sz>0) {
                std::string c(sz, '\0'); DWORD rd=0;
                if (WinHttpReadData(hR, &c[0], sz, &rd) && rd>0) { c.resize(rd); raw += c; }
            }
            WinHttpCloseHandle(hR);
            if (raw.find("\"candidates\"") != std::string::npos) break;
        } else WinHttpCloseHandle(hR);
    }
    WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
#elif defined(__ANDROID__)
    struct android_app* app = GetAndroidApp();
    if (!app||!app->activity||!app->activity->vm) return "ERR: JVM non attiva";
    JNIEnv* env=nullptr; bool att=false;
    if (app->activity->vm->GetEnv((void**)&env,JNI_VERSION_1_6)!=JNI_OK) { if(app->activity->vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)return "ERR: JNI"; att=true; }
    jclass cls=env->GetObjectClass(app->activity->clazz);
    jmethodID mid=env->GetStaticMethodID(cls,"callGeminiHttp","(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    if (mid) {
        std::string url="https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
        jstring ju=env->NewStringUTF(url.c_str()), jk=env->NewStringUTF(apiKey.c_str()), jb=env->NewStringUTF(body.c_str());
        jstring jr=(jstring)env->CallStaticObjectMethod(cls,mid,ju,jk,jb);
        if (jr) { const char* u=env->GetStringUTFChars(jr,nullptr); if(u){raw=u; env->ReleaseStringUTFChars(jr,u);} env->DeleteLocalRef(jr); }
        env->DeleteLocalRef(ju); env->DeleteLocalRef(jk); env->DeleteLocalRef(jb);
    }
    env->DeleteLocalRef(cls); if(att) app->activity->vm->DetachCurrentThread();
#endif

    size_t cd = raw.find("\"candidates\"");
    if (cd != std::string::npos) {
        std::string tx = ExtractField(raw.substr(cd), "text");
        if (!tx.empty()) return tx;
    }
    std::string err = ExtractField(raw, "message");
    return err.empty() ? (raw.empty() ? "ERR: Nessuna risposta da Gemini" : raw) : ("ERR: " + err);
}