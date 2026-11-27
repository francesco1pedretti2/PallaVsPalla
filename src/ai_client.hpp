#ifndef AI_CLIENT_HPP
#define AI_CLIENT_HPP
#include <string>
#include <vector>

struct ChatTurn {
    bool isUser = true;
    bool isError = false;   // messaggi di errore: mostrati in chat ma MAI inviati a Gemini
    std::string text;       // testo inviato a Gemini (per le risposte AI: la risposta grezza)
    std::string shown;      // testo mostrato in chat (se vuoto: text)
};

struct AiResult {
    bool ok = false;
    bool keyProblem = false;   // chiave non valida / senza permessi
    int status = 0;            // ultimo codice HTTP
    std::string text;          // risposta del modello
    std::string error;         // messaggio d'errore leggibile (italiano)
    std::string model;         // modello che ha risposto
};

// Pulisce una chiave incollata (spazi, a capo, virgolette, prefisso "key=").
std::string SanitizeApiKey(const std::string& raw);

// Chat di creazione palle. Bloccante: chiamare da un thread.
AiResult GeminiGenerate(const std::string& apiKey, const std::vector<ChatTurn>& history, const std::string& curScript);

// Verifica la chiave elencando i modelli disponibili. Bloccante: chiamare da un thread.
AiResult GeminiCheckKey(const std::string& apiKey);

// Tastiera nativa Android (su Windows non fanno nulla).
void OpenNativeInputDialog(const std::string& title, const std::string& initialText);
bool PollNativeInputDialog(std::string& outText);

#endif
