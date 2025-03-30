#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include "audit.hpp"
#include "rpc.hpp"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

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

// Definições dummy para g_log_path e log_message, para que as funções do projeto funcionem sem produzir log.
std::string g_log_path = "";
void log_message(const std::string& log_path, const std::string& message) {
    // Neste binário não se imprime log.
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        cout << "Uso: " << argv[0] << " <arquivo_csv>" << endl;
        return 1;
    }

    string csvFilename = argv[1];
    ifstream infile(csvFilename);
    if(!infile.is_open()){
        cerr << "Erro ao abrir arquivo: " << csvFilename << endl;
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
    
    int okCount = 0, errorCount = 0;
    for(const auto& rec : csvRecords) {
         auto auditResultOpt = audit_block(rec.height);
         if(!auditResultOpt.has_value()){
             cout << "Bloco " << rec.height << ": erro ao auditar via RPC" << endl;
             // Debug: chama get_block_info para imprimir a resposta bruta
             json blockInfo = get_block_info(rec.height);
             if(blockInfo.is_null()){
                 cout << "  Debug: get_block(" << rec.height << ") retornou null." << endl;
             } else {
                 cout << "  Debug: get_block(" << rec.height << ") retornou:" << endl;
                 cout << blockInfo.dump(2) << endl;
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
             okCount++;
         } else {
             cout << "Bloco " << rec.height << ": ERRO (" << details.str() << ")" << endl;
             errorCount++;
         }
    }

    cout << "Resumo: " << okCount << " blocos OK, " << errorCount 
         << " blocos com discrepâncias." << endl;
    return 0;
}
