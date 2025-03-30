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
            // Remover espaços em branco
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
    if(server.empty()){
        auto config = load_config("audit-xmr.cfg");
        if(config.find("rpc_url") != config.end()){
            server = config["rpc_url"];
        }
    }
    if(!server.empty()){
        // Se não incluir o protocolo, assume http://
        if(server.find("http://") == std::string::npos && server.find("https://") == std::string::npos)
            server = "http://" + server;
        // Se não conter "/json_rpc", acrescenta-o
        if(server.find("/json_rpc") == std::string::npos)
            server = server + "/json_rpc";
        set_rpc_url(server);
        log_message(g_log_path, "Server RPC configurado: " + server);
    } else {
        log_message(g_log_path, "Nenhum servidor RPC definido; usando o padrão.");
    }

    // Carrega os registros do arquivo CSV
    string csvFilename = argv[1];
    ifstream infile(csvFilename);
    if(!infile.is_open()){
        cerr << "Error opening file: " << csvFilename << endl;
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
    for(const auto& rec : csvRecords) {
         log_message(g_log_path, "Auditoria do bloco " + std::to_string(rec.height) + " iniciada.");
         auto auditResultOpt = audit_block(rec.height);
         if(!auditResultOpt.has_value()){
             cout << "Bloco " << rec.height << ": erro ao auditar via RPC" << endl;
             log_message(g_log_path, "Erro: audit_block(" + std::to_string(rec.height) + ") retornou null.");
             // Debug: chama get_block_info para exibir a resposta bruta
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
         if(auditResult.issues_string() != rec.issues) {
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
             cout << "Bloco " << rec.height << ": OK" << endl;
             log_message(g_log_path, "Bloco " + std::to_string(rec.height) + " auditado: OK.");
             okCount++;
         } else {
             cout << "Bloco " << rec.height << ": ERRO (" << details.str() << ")" << endl;
             log_message(g_log_path, "Bloco " + std::to_string(rec.height) + " auditado: ERRO (" + details.str() + ").");
             errorCount++;
         }
    }

    ostringstream summary;
    summary << "Resumo: " << okCount << " blocos OK, " << errorCount << " blocos com discrepâncias.";
    cout << summary.str() << endl;
    log_message(g_log_path, summary.str());
    log_message(g_log_path, "Validação via RPC finalizada.");
    return 0;
}
