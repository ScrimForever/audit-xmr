#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

extern std::string RPC_URL;
extern std::string g_log_path; // Variável global para o caminho do log

// Funções RPC e auxiliares
void set_rpc_url(const std::string& url);
std::string rpc_call(const std::string& method, const std::string& params_json);
int get_blockchain_height();  // Retorna um int, conforme a implementação
nlohmann::json get_block_info(int height);
nlohmann::json get_transaction_details(const std::string& tx_hash);

// Declaração da função de log
void log_message(const std::string& log_path, const std::string& message);

// Estrutura para armazenar os resultados da auditoria de um bloco
struct AuditResult {
    int height = 0;
    std::string hash;
    uint64_t real_reward = 0;
    uint64_t coinbase_outputs = 0;
    uint64_t total_mined = 0;
    std::vector<std::string> issues;
    std::string status;

    std::string issues_string() const {
        if (issues.empty()) return "";
        std::string s = issues[0];
        for (size_t i = 1; i < issues.size(); ++i) {
            s += "|" + issues[i];
        }
        return s;
    }
};

// Função principal de auditoria de um bloco
std::optional<AuditResult> audit_block(int height);
