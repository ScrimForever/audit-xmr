#include "audit.hpp"
#include "rpc.hpp"
#include <sstream>
#include <cmath> // Para std::abs

using json = nlohmann::json;

extern std::string g_log_path; // Definido em audit-xmr.cpp para acesso global

std::optional<AuditResult> audit_block(int height) {
    AuditResult result;
    result.height = height;

    std::stringstream ss;
    ss << "[DEBUG] Auditoria iniciada para bloco " << height;
    log_message(g_log_path, ss.str());

    auto block_info = get_block_info(height);
    if (block_info.is_null()) {
        ss.str("");
        ss << "[ERRO] Falha ao obter bloco " << height;
        log_message(g_log_path, ss.str());
        return std::nullopt;
    }

    result.hash = block_info["block_header"]["hash"];
    ss.str("");
    ss << "[DEBUG] Bloco " << height << " obtido com hash " << result.hash;
    log_message(g_log_path, ss.str());

    // Processa o JSON do bloco para extrair os dados da transação coinbase
    json block_json = json::parse(block_info["json"].get<std::string>());
    json miner_tx = block_json["miner_tx"];

    uint64_t coinbase_sum = 0;
    for (const auto& vout : miner_tx["vout"]) {
        coinbase_sum += vout["amount"].template get<uint64_t>(); // Uso de 'template' para evitar ambiguidades
    }
    ss.str("");
    ss << "[DEBUG] Saídas CoinBase bloco " << height << ": " << coinbase_sum;
    log_message(g_log_path, ss.str());

    // Como o cálculo do supply se baseia apenas na coinbase,
    // quaisquer transações adicionais (tx_hashes) são ignoradas.
    uint64_t tx_outputs = 0;
    ss.str("");
    ss << "[DEBUG] Total saídas TX bloco " << height << ": " << tx_outputs;
    log_message(g_log_path, ss.str());

    uint64_t reward = block_info["block_header"]["reward"].get<uint64_t>();
    result.real_reward = reward;
    result.coinbase_outputs = coinbase_sum;
    result.total_mined = coinbase_sum + tx_outputs;

    ss.str("");
    ss << "[DEBUG] Recompensa real bloco " << height << ": " << reward
       << ", Total minerado: " << result.total_mined;
    log_message(g_log_path, ss.str());

    // Verificações simples para garantir a consistência dos dados
    const uint64_t TOLERANCE = 1e9;
    if (std::abs((int64_t)(reward - coinbase_sum)) > TOLERANCE) {
        result.issues.push_back("Reward != CoinBase");
    }
    if (std::abs((int64_t)(reward - result.total_mined)) > TOLERANCE) {
        result.issues.push_back("Reward != TotalMined");
    }
    if (miner_tx["vin"].size() != 1 ||
        miner_tx["vin"][0]["gen"]["height"].get<int>() != height) {
        result.issues.push_back("CoinBase inválida");
    }

    result.status = result.issues.empty() ? "OK" : "Discrepância";

    ss.str("");
    ss << "[DEBUG] Resultado bloco " << height << ": status=" << result.status
       << ", issues=" << result.issues_string();
    log_message(g_log_path, ss.str());

    return result;
}
