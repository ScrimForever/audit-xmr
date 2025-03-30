#include "rpc.hpp"
#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string RPC_URL = "http://127.0.0.1:18081/json_rpc";
extern std::string g_log_path; // Definido em audit-xmr.cpp

void set_rpc_url(const std::string& url) {
    RPC_URL = url;
    std::stringstream ss;
    ss << "[DEBUG] RPC_URL definida como " << url;
    log_message(g_log_path, ss.str());
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string rpc_call(const std::string& method, const std::string& params_json) {
    std::stringstream ss;
    ss << "[DEBUG] Chamando RPC: " << method << " com params " << params_json;
    log_message(g_log_path, ss.str());

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response_string;
    std::string post_fields = R"({"jsonrpc":"2.0","id":"0","method":")" 
                                + method + R"(","params":)" + params_json + "}";

    curl_easy_setopt(curl, CURLOPT_URL, RPC_URL.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        ss.str("");
        ss << "[ERRO] Falha na chamada CURL para " << method << ": " 
           << curl_easy_strerror(res);
        log_message(g_log_path, ss.str());
        return "";
    }

    ss.str("");
    ss << "[DEBUG] Resposta RPC " << method << ": " << response_string;
    log_message(g_log_path, ss.str());
    return response_string;
}

int get_blockchain_height() {
    std::string res = rpc_call("get_block_count", "{}");
    if (res.empty()) {
        log_message(g_log_path, "[ERRO] RPC 'get_block_count' retornou vazio.");
        return -1;
    }

    try {
        json parsed = json::parse(res);
        return parsed["result"]["count"];
    } catch (...) {
        log_message(g_log_path, "[ERRO] Falha ao parsear resposta de get_block_count");
        return -1;
    }
}

json get_block_info(int height) {
    json params = { {"height", height} };
    std::string res = rpc_call("get_block", params.dump());
    if (res.empty()) {
        std::stringstream ss;
        ss << "[ERRO] Falha ao obter bloco " << height;
        log_message(g_log_path, ss.str());
        return nullptr;
    }
    try {
        json parsed = json::parse(res);
        // Se a resposta contiver "error", logue o erro e retorne null
        if (parsed.find("error") != parsed.end()) {
            std::stringstream ss;
            ss << "[ERRO] RPC get_block retornou erro para o bloco " << height 
               << ": " << parsed["error"];
            log_message(g_log_path, ss.str());
            return nullptr;
        }
        return parsed["result"];
    } catch (...) {
        std::stringstream ss;
        ss << "[ERRO] Falha ao parsear bloco " << height;
        log_message(g_log_path, ss.str());
        return nullptr;
    }
}

json get_transaction_details(const std::string& tx_hash) {
    json params = {
        {"txs_hashes", {tx_hash}},
        {"decode_as_json", true}
    };
    std::string res = rpc_call("get_transactions", params.dump());
    if (res.empty()) return nullptr;
    
    try {
        json parsed = json::parse(res);
        if (parsed.contains("error")) {
            std::stringstream ss;
            ss << "[AVISO] RPC get_transactions não suportado para " << tx_hash;
            log_message(g_log_path, ss.str());
            return nullptr;
        }
        if (parsed["result"]["txs"].empty()) return nullptr;
        return json::parse(parsed["result"]["txs"][0]["as_json"].get<std::string>());
    } catch (...) {
        std::stringstream ss;
        ss << "[ERRO] Falha ao parsear transação " << tx_hash;
        log_message(g_log_path, ss.str());
        return nullptr;
    }
}
