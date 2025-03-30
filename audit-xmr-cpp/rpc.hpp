#pragma once
#include <string>
#include <nlohmann/json.hpp>

extern std::string RPC_URL;
extern std::string g_log_path; // Variável global para o caminho do log

// Funções RPC e auxiliares
void set_rpc_url(const std::string& url);
std::string rpc_call(const std::string& method, const std::string& params_json);
int get_blockchain_height();  // Retorna um int, conforme a implementação
nlohmann::json get_block_info(int height);
nlohmann::json get_transaction_details(const std::string& tx_hash);

// Funções de log
void log_message(const std::string& log_path, const std::string& message);
void log_message(const std::string& log_path, const std::string& message, bool is_block_end); // Sobrecarga com separador
