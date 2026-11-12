#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <Windows.h>
#include <algorithm>
#include <vector>
#include <cstring>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sqlite3.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ---------------- DOSYA YÖNETİCİSİ ----------------
class FileManager {
public:
    static std::string WriteFile(const std::string& path, const std::string& content) {
        try {
            if (path.find("..") != std::string::npos) return "ERROR: Security violation.";
            std::ofstream file(path, std::ios::out | std::ios::trunc);
            if (!file.is_open()) return "ERROR: Could not open file.";
            file << content;
            return "SUCCESS: Saved to " + path;
        }
        catch (...) { return "ERROR: Writing failed."; }

    }

    static std::string AppendFile(const std::string& path, const std::string& content) {
        try {
            std::ofstream file(path, std::ios::out | std::ios::app);
            if (!file.is_open()) return "ERROR: Could not append.";
            file << content;
            return "SUCCESS: Appended to " + path;
        }
        catch (...) { return "ERROR: Appending failed."; }
    }

    static std::string DeleteFile(const std::string& path) {
        std::error_code ec;
        if (fs::remove(path, ec)) return "SUCCESS: Deleted.";
        return "ERROR: " + ec.message();
    }

    static std::string CreateDir(const std::string& path) {
        try {
            // filesystem kütüphanesini kullanarak klasör oluşturur
            if (fs::create_directories(path)) {
                return "SUCCESS: Directory created.";
            }
            return "SUCCESS: Directory already exists or created.";
        }
        catch (const std::exception& e) {
            return "ERROR: MKDIR failed: " + std::string(e.what());
        }
    }
};

// ---------------- AJAN ÇEKİRDEĞİ ----------------
class EklamulMuarifPro {
private:
    std::string api_key;

    std::string CleanRawString(std::string str) {
        str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char c) {
            return (c < 32 && c != '\n' && c != '\r' && c != '\t') || c > 126;
            }), str.end());
        return str;
    }

public:
    EklamulMuarifPro() {
        // API ANAHTARINI BURAYA EKLE
        api_key = "AIzaSyD_Uxlzqn9_BvetvfEYyt2mw6XtcT8jPNs";
    }

    std::string ExecuteSingleAction(json& cmd) {
        if (!cmd.contains("tool") || !cmd.contains("path")) return "ERROR: Missing fields.";

        std::string tool = cmd["tool"];
        std::string path = cmd["path"];

        if (tool == "WRITE")  return FileManager::WriteFile(path, cmd["content"]);
        if (tool == "DELETE") return FileManager::DeleteFile(path);
        if (tool == "MKDIR")  return FileManager::CreateDir(path);

        // Klasörü Windows'ta fiziksel olarak açma komutu
        if (tool == "OPEN") {
            // ShellExecuteA için Windows.h gereklidir
            ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return "SUCCESS: Opened " + path;
        }

        if (tool == "RUN") {
            std::string command = "";
            std::string extension = fs::path(path).extension().string();

            // 1. PYTHON ÇALIŞTIRMA
            if (extension == ".py") {
                command = "start cmd /k \"python \"" + path + "\" & pause\"";
            }
            // 2. C++ DERLEME VE ÇALIŞTIRMA (MinGW/g++ yüklü varsayılır)
            else if (extension == ".cpp") {
                std::string exeName = path.substr(0, path.find_last_of(".")) + ".exe";
                // Önce derler, hata yoksa çalıştırır
                command = "start cmd /k \"g++ \"" + path + "\" -o \"" + exeName + "\" && \"" + exeName + "\" & pause\"";
            }
            // 3. C# DERLEME VE ÇALIŞTIRMA (csc.exe yüklü varsayılır)
            else if (extension == ".cs") {
                std::string exeName = path.substr(0, path.find_last_of(".")) + ".exe";
                // Windows'un standart C# derleyicisini çağırmaya çalışır
                command = "start cmd /k \"csc /out:\"" + exeName + "\" \"" + path + "\" && \"" + exeName + "\" & pause\"";
            }
            // 4. DİREKT EXE ÇALIŞTIRMA
            else if (extension == ".exe") {
                command = "start \"\" \"" + path + "\"";
            }
            else {
                return "ERROR: No run logic for " + extension;
            }

            system(command.c_str());
            return "SUCCESS: Run command issued for " + path;
        }

        return "ERROR: Unknown tool: " + tool;
    }

    std::string ProcessRequest(const std::string& user_prompt) {
        if (api_key.empty() || api_key == "YOUR_API_KEY_HERE") return "ERROR: API Key is missing.";



        std::string system_instruction =
            "Respond in plain text. No markdown. "
            "For file ops use JSON: "
            "{\"tool\":\"WRITE\",\"path\":\"x.txt\",\"content\":\"y\"} or "
            "{\"tool\":\"MKDIR\",\"path\":\"folder_name\"}. " // Bunu ekledik!
            "Prompt: " + user_prompt;

        json body = {
            {"contents", {{
                {"parts", {{{"text", system_instruction}}}}
            }}}
        };

        std::string model_id = "gemini-3.5-flash";
        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model_id + ":generateContent?key=" + api_key;

        cpr::Response r;
        int retryCount = 0;
        const int maxRetries = 3; // En fazla 3 kez tekrar dene

        // 2. Komple bu blokla değiştir
        while (retryCount < maxRetries) {
            try {
                r = cpr::Post(
                    cpr::Url{ url },
                    cpr::Header{ {"Content-Type","application/json"} },
                    cpr::Body{ body.dump() },
                    cpr::VerifySsl{ false },
                    cpr::Timeout{ 60000 }
                );

                // Eğer 503 (Sunucu Meşgul) almadıysak döngüden çık
                if (r.status_code != 503) {
                    break;
                }

                // Eğer 503 aldıysak, her denemede biraz daha fazla bekle (2sn, 4sn...)
                retryCount++;
                if (retryCount < maxRetries) {
                    Sleep(2000 * retryCount);
                }
            }
            catch (const std::exception& e) {
                return "NETWORK_EXCEPTION: API call failed - " + std::string(e.what());
            }
        }

        // 3. Döngü bittikten sonraki kontroller
        if (r.status_code == 0) return "CONNECTION_ERROR: Check internet.";
        if (r.status_code == 429) return "RATE_LIMIT: API quota exceeded.";
        if (r.status_code == 503) return "SERVER_OVERLOAD: Gemini is still busy after retries.";

        if (r.status_code != 200) {
            try {
                auto err_json = json::parse(r.text, nullptr, false, true);
                if (!err_json.is_discarded() && err_json.contains("error")) {
                    return "API_ERROR (" + std::to_string(r.status_code) + "): " + err_json["error"]["message"].get<std::string>();
                }
            }
            catch (...) {}
            return "HTTP_ERROR: " + std::to_string(r.status_code) + " - Check Model Name or API Key.";
        }

        try {
            auto j = json::parse(r.text, nullptr, false, true);
            if (j.is_discarded()) return "JSON_PARSE_ERROR: API returned invalid data.";

            if (j.contains("candidates") && !j["candidates"].empty()) {
                auto& candidate = j["candidates"][0];
                if (candidate.contains("content") && candidate["content"].contains("parts")) {
                    std::string ai_text = candidate["content"]["parts"][0]["text"].get<std::string>();

                    size_t start = ai_text.find_first_of("{[");
                    size_t end = ai_text.find_last_of("}]");

                    if (start != std::string::npos && end != std::string::npos) {
                        std::string tool_json_str = ai_text.substr(start, end - start + 1);
                        auto tj = json::parse(tool_json_str, nullptr, false, true);

                        if (!tj.is_discarded()) {
                            if (tj.is_array()) {
                                std::string all_results = "";
                                for (auto& single_tool : tj) {
                                    all_results += ExecuteSingleAction(single_tool) + " | ";
                                }
                                return all_results;
                            }
                            else {
                                return ExecuteSingleAction(tj);
                            }
                        }
                    }
                    return CleanRawString(ai_text);
                }
            }
            return "ERROR: Unexpected API response format.";
        
        }
        catch (const std::exception& e) {
            return std::string("C++ EXCEPTION: ") + e.what();
        }
    }
};

// ---------------- GLOBAL STATE ----------------
EklamulMuarifPro* globalAgent = nullptr;
static std::string responseBuffer;

struct ErrorDetails {
    char message[256];
    char function[64];
    int line;
    char file[64];
};

static ErrorDetails lastError = { "None", "None", 0, "None" };

// ---------------- DLL EXPORTS ----------------
extern "C" {
    __declspec(dllexport) void __stdcall InitAgent() {
        if (!globalAgent) globalAgent = new EklamulMuarifPro();
    }

    __declspec(dllexport) const char* __stdcall AskToGeminiC(const char* prompt) {
        if (!globalAgent) return "ERROR: Agent not initialized.";
        try {
            if (!prompt) return "ERROR: Null prompt.";
            responseBuffer = globalAgent->ProcessRequest(prompt);
            return responseBuffer.c_str();
        }
        catch (const std::exception& e) {
            strncpy(lastError.message, e.what(), 255);
            return "DLL_CRITICAL_EXCEPTION";
        }
    }

    __declspec(dllexport) void __stdcall CloseAgent() {
        if (globalAgent) {
            delete globalAgent;
            globalAgent = nullptr;
        }
    }

    __declspec(dllexport) ErrorDetails __stdcall GetLastErrorDetails() {
        return lastError;
    }
}