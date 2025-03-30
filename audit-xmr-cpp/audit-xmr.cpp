// audit-xmr.cpp
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <optional>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <chrono>
#include <iomanip>

#include "audit.hpp"
#include "rpc.hpp"

#define VER "0.1"

namespace fs = std::filesystem;

std::string g_log_path; // Definida globalmente para ser acessada por todos os arquivos

// Função de log com timestamp
void log_message(const std::string& log_path, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

    std::ofstream log_file(log_path, std::ios::app);
    if (log_file.is_open()) {
        log_file << "[" << timestamp.str() << "] " << message << std::endl;
    }
}

// Função para carregar configurações de um arquivo
std::map<std::string, std::string> load_config(const std::string& config_file) {
    std::map<std::string, std::string> config;
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[AVISO] Não foi possível abrir o arquivo de configuração " << config_file << ". Usando padrões.\n";
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
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

int main(int argc, char* argv[]) {
    std::string config_file = "audit-xmr.cfg";
    auto config = load_config(config_file);

    std::string rpc_url = config.count("rpc_url") ? config["rpc_url"] : "http://127.0.0.1:18081/json_rpc";
    int user_thread_count = config.count("threads") ? std::stoi(config["threads"]) : 1;
    std::string output_dir = config.count("output_dir") ? config["output_dir"] : "out";

    int start_block = -1;
    int end_block = -1;
    int single_block = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--range" && i + 2 < argc) {
            start_block = std::stoi(argv[++i]);
            end_block = std::stoi(argv[++i]);
        } else if (arg == "--block" && i + 1 < argc) {
            single_block = std::stoi(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            std::string tval = argv[++i];
            if (tval == "max") {
                user_thread_count = std::thread::hardware_concurrency();
            } else {
                user_thread_count = std::stoi(tval);
            }
        } else if (arg == "--server" && i + 1 < argc) {
            std::string server = argv[++i];
            if (server.find(":") != std::string::npos) {
                rpc_url = "http://" + server + "/json_rpc";
            } else {
                rpc_url = "http://" + server + ":18081/json_rpc";
            }
        } else if (arg == "--output-dir" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "\nUso: ./audit-xmr [opções]\n"
                      << "  --range <inicio> <fim>     Audita blocos do início ao fim\n"
                      << "  --block <altura>           Audita apenas um bloco específico\n"
                      << "  --threads <N>|max          Define o número de threads (default do config: " << user_thread_count << ")\n"
                      << "  --server <ip[:porta]>      Define o servidor RPC (default do config: " << rpc_url << ")\n"
                      << "  --output-dir <dir>         Define o diretório de saída (default do config: " << output_dir << ")\n"
                      << "  -h, --help                 Mostra esta ajuda\n"
                      << "  -v, --version              Mostra a versão do programa\n";
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "Versão: " << VER << std::endl;
            return 0;
        }
    }

    set_rpc_url(rpc_url);

    fs::path out_dir = fs::path(output_dir);
    fs::create_directories(out_dir);

    std::string csv_path = (out_dir / "auditoria_monero.csv").string();
    std::string log_path = (out_dir / "audit_log.txt").string();
    g_log_path = log_path; // Atribuir o caminho do log à variável global

    auto log = [&](const std::string& msg) {
        log_message(log_path, msg);
    };

    log("[INFO] Script iniciado");

    if (single_block >= 0) {
        log("[INFO] Auditando bloco único: " + std::to_string(single_block));
        auto res = audit_block(single_block);
        if (res.has_value()) {
            const auto& r = res.value();
            std::cout << "\n🧱 Bloco " << r.height << " - Hash: " << r.hash << std::endl;
            std::cout << "💰 Recompensa real: " << r.real_reward << " atomic units\n";
            std::cout << "📤 Saídas CoinBase: " << r.coinbase_outputs << std::endl;
            std::cout << "📦 Total minerado: " << r.total_mined << std::endl;
            std::cout << "⚠️ Status: " << r.status << std::endl;
            if (!r.issues.empty()) {
                std::cout << "❗ Problemas:\n";
                for (const auto& issue : r.issues) {
                    std::cout << "  - " << issue << std::endl;
                }
            } else {
                std::cout << "✅ Nenhum problema detectado.\n";
            }
            log("[INFO] Auditoria do bloco " + std::to_string(single_block) + " concluída: status=" + r.status);
        } else {
            std::cerr << "[ERRO] Auditoria falhou para o bloco " << single_block << std::endl;
            log("[ERRO] Auditoria falhou para o bloco " + std::to_string(single_block));
        }
        return 0;
    }

    if (start_block < 0 || end_block < 0) {
        start_block = 0;
        end_block = get_blockchain_height() - 1;
    }

    log("[INFO] Iniciando auditoria de " + std::to_string(start_block) + " até " + std::to_string(end_block));

    int max_threads = std::thread::hardware_concurrency();
    if (user_thread_count > max_threads) {
        std::cerr << "[AVISO] Foi solicitado " << user_thread_count << " threads, mas o sistema só possui " << max_threads << ". Usando mesmo assim.\n";
        log("[AVISO] Threads solicitados: " + std::to_string(user_thread_count) + ", disponíveis: " + std::to_string(max_threads));
    }

    int thread_count = std::max(1, user_thread_count);
    int total_blocks = end_block - start_block + 1;

    std::vector<AuditResult> all_results;
    std::mutex results_mutex;

    auto worker = [&](int tid, int from, int to) {
        std::vector<AuditResult> local_results;
        for (int h = from; h <= to; ++h) {
            log("[DEBUG] Thread " + std::to_string(tid) + " auditando bloco " + std::to_string(h));
            auto res = audit_block(h);
            if (res.has_value()) {
                local_results.push_back(res.value());
                log("[DEBUG] Bloco " + std::to_string(h) + " auditado com status " + res.value().status);
            } else {
                log("[ERRO] Falha na auditoria do bloco " + std::to_string(h));
            }
        }
        std::lock_guard<std::mutex> lock(results_mutex);
        all_results.insert(all_results.end(), local_results.begin(), local_results.end());
    };

    std::vector<std::thread> threads;
    int chunk_size = total_blocks / thread_count;

    for (int i = 0; i < thread_count; ++i) {
        int from = start_block + i * chunk_size;
        int to = (i == thread_count - 1) ? end_block : from + chunk_size - 1;
        log("[DEBUG] Iniciando thread " + std::to_string(i) + " para blocos " + std::to_string(from) + "-" + std::to_string(to));
        threads.emplace_back(worker, i, from, to);
    }

    for (auto& t : threads) t.join();

    std::sort(all_results.begin(), all_results.end(), [](const auto& a, const auto& b) {
        return a.height < b.height;
    });

    std::ofstream csv(csv_path);
    csv << "Altura,Hash,RecompensaReal,CoinbaseOutputs,TotalMinerado,Problemas,Status\n";
    for (const auto& res : all_results) {
        csv << res.height << ',' << res.hash << ',' << res.real_reward << ','
            << res.coinbase_outputs << ',' << res.total_mined << ','
            << res.issues_string() << ',' << res.status << '\n';
    }
    csv.close();

    log("[INFO] Auditoria finalizada. Resultado salvo em: " + csv_path);
    std::cout << "Auditoria concluída. Resultados salvos em: " << csv_path << std::endl;
    return 0;
}
