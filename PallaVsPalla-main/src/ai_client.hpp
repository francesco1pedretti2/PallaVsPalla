#ifndef AI_CLIENT_HPP
#define AI_CLIENT_HPP
#include <string>
#include <vector>
struct ChatTurn { bool isUser = true; std::string text; };
std::string CallGeminiChatAPI(const std::string& apiKey, const std::vector<ChatTurn>& history, const std::string& curScript);
void OpenNativeInputDialog(const std::string& title, const std::string& initialText);
bool PollNativeInputDialog(std::string& outText);
#endif