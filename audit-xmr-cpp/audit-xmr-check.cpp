#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include "audit.hpp"
#include "rpc.hpp"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// Nome do arquivo de log de debug
std::string g_log_path = "audit_check_debug.log";

// Função para carregar configurações do arquivo cfg
std::map<std::string, std::string> load_config(const std::string &config_file) {
    std::map<std::string, std::string> config;
    std::ifstream file(config_file);
    if (!file.is_open())
        return config;
    std::string line;
    while (std::getline(file, line)) {
        if(line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key, value;
        if(std::getline(iss, key, '=') && std::getline(iss, value)) {
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            config[key] = value;
        }
    }
    file.close();
    return config;
}

// Estrutura para armazenar um registro do CSV
struct CSVRecord {
    int height;
    string hash;
    unsigned long long real_reward;
    unsigned long long coinbase_outputs;
    unsigned long long total_mined;
    string issues;
    string status;
};

bool parseCSVLine(const string& line, CSVRecord& record) {
    istringstream ss(line);
    string token;
    vector<string> tokens;
    while(getline(ss, token, ',')) {
        tokens.push_back(token);
    }
    if(tokens.size() < 7)
        return false;
    try {
        record.height = stoi(tokens[0]);
        record.hash = tokens[1];
        record.real_reward = stoull(tokens[2]);
        record.coinbase_outputs = stoull(tokens[3]);
        record.total_mined = stoull(tokens[4]);
        record.issues = tokens[5];
        record.status = tokens[6];
    } catch(...) {
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    log_message(g_log_path, "Início da validação via RPC.");

    // Determina o servidor RPC: tenta --server na linha de comando, senão usa o arquivo de configuração.
    std::string server;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--server" && i+1 < argc) {
            server = argv[++i];
        }
    }

    auto config = load_config("audit-xmr.cfg");
    if(server.empty() && config.find("rpc_url") != config.end()) {
        server = config["rpc_url"];
    }

    // Exibir configurações usadas com formatação consistente
    cout << "------------------------\n";
    cout << "Configurações do audit-xmr-check\n";
    cout << "------------------------\n";
    if (!server.empty()) {
        if(server.find("http://") == std::string::npos && server.find("https://") == std::string::npos)
            server = "http://" + server;
        if(server.find("/json_rpc") == std::string::npos)
            server = server + "/json_rpc";
        cout << "RPC URL: " << server << "\n";
        cout << "  (Origem: " << (config.find("rpc_url") != config.end() ? "audit-xmr.cfg" : "--server") << ")\n";
        set_rpc_url(server);
        log_message(g_log_path, "Server RPC configurado: " + server);
    } else {
        server = "http://127.0.0.1:18081/json_rpc"; // Padrão
        cout << "RPC URL: " << server << "\n";
        cout << "  (Origem: padrão)\n";
        set_rpc_url(server);
        log_message(g_log_path, "Nenhum servidor RPC definido; usando o padrão.");
    }
    cout << "Log Path: " << g_log_path << "\n";
    cout << "  (Origem: padrão)\n";
    cout << "------------------------\n";
    cout << "Nota: Configs do audit-xmr.cfg não usadas aqui:\n";
    if (config.find("threads") != config.end()) {
        cout << "- Threads: " << config["threads"] << "\n";
    }
    if (config.find("output_dir") != config.end()) {
        cout << "- Output Dir: " << config["output_dir"] << "\n";
    }
    if (config.find("max_retries") != config.end()) {
        cout << "- Max Retries: " << config["max_retries"] << "\n";
    }
    if (config.find("timeout") != config.end()) {
        cout << "- Timeout: " << config["timeout"] << "\n";
    }
    cout << "------------------------\n\n";

    // Carrega os registros do arquivo CSV
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <arquivo.csv>" << endl;
        return 1;
    }
    string csvFilename = argv[1];
    ifstream infile(csvFilename);
    if(!infile.is_open()){
        cerr << "Erro: não foi possível abrir o arquivo " << csvFilename << endl;
        log_message(g_log_path, "Erro: não foi possível abrir o arquivo CSV: " + csvFilename);
        return 1;
    }

    vector<CSVRecord> csvRecords;
    string line;
    bool header = true;
    while(getline(infile, line)){
         if(header) {
             if(line.find("Altura") != string::npos) {
                  header = false;
                  continue;
             }
         }
         CSVRecord rec;
         if(parseCSVLine(line, rec)) {
              csvRecords.push_back(rec);
         }
    }
    infile.close();
    log_message(g_log_path, "Arquivo CSV lido com " + std::to_string(csvRecords.size()) + " registros.");

    int okCount = 0, errorCount = 0;
    cout << "------------------------\n";
    cout << "Resultados da Validação\n";
    cout << "------------------------\n";
    for(const auto& rec : csvRecords) {
         log_message(g_log_path, "Auditoria do bloco " + std::to_string(rec.height) + " iniciada.");
         auto auditResultOpt = audit_block(rec.height);
         if(!auditResultOpt.has_value()){
             cout << "Bloco " << setw(6) << rec.height << ": ERRO (falha ao auditar via RPC)\n";
             log_message(g_log_path, "Erro: audit_block(" + std::to_string(rec.height) + ") retornou null.");
             json blockInfo = get_block_info(rec.height);
             if(blockInfo.is_null()){
                 log_message(g_log_path, "Debug: get_block(" + std::to_string(rec.height) + ") retornou null.");
             } else {
                 log_message(g_log_path, "Debug: get_block(" + std::to_string(rec.height) + ") retornou:\n" + blockInfo.dump(2));
             }
             errorCount++;
             continue;
         }
         auto auditResult = auditResultOpt.value();
         bool match = true;
         ostringstream details;
         if(auditResult.real_reward != rec.real_reward) {
             match = false;
             details << "RecompensaReal (CSV: " << rec.real_reward
                     << ", RPC: " << auditResult.real_reward << ") ";
         }
         if(auditResult.coinbase_outputs != rec.coinbase_outputs) {
             match = false;
             details << "CoinbaseOutputs (CSV: " << rec.coinbase_outputs
                     << ", RPC: " << auditResult.coinbase_outputs << ") ";
         }
         if(auditResult.total_mined != rec.total_mined) {
             match = false;
             details << "TotalMinerado (CSV: " << rec.total_mined
                     << ", RPC: " << auditResult.total_mined << ") ";
         }
         string csv_issues = (rec.issues == "Nenhum" ? "" : rec.issues);
         if(auditResult.issues_string() != csv_issues) {
             match = false;
             details << "Issues (CSV: " << rec.issues
                     << ", RPC: " << auditResult.issues_string() << ") ";
         }
         if(auditResult.status != rec.status) {
             match = false;
             details << "Status (CSV: " << rec.status
                     << ", RPC: " << auditResult.status << ") ";
         }
         if(match) {
             cout << "Bloco " << setw(6) << rec.height << ": OK\n";
             log_message(g_log_path, "Bloco " + std::to_string(rec.height) + " auditado: OK.");
             okCount++;
         } else {
             cout << "Bloco " << setw(6) << rec.height << ": ERRO (" << details.str() << ")\n";
             log_message(g_log_path, "Bloco " + std::to_string(rec.height) + " auditado: ERRO (" + details.str() + ").");
             errorCount++;
         }
    }

    cout << "------------------------\n";
    cout << "Resumo da Validação\n";
    cout << "------------------------\n";
    cout << "Blocos OK:       " << setw(6) << okCount << "\n";
    cout << "Blocos com erro: " << setw(6) << errorCount << "\n";
    cout << "------------------------\n";
    log_message(g_log_path, "Resumo: " + std::to_string(okCount) + " blocos OK, " + std::to_string(errorCount) + " blocos com discrepâncias.");
    log_message(g_log_path, "Validação via RPC finalizada.");
    return 0;
}
