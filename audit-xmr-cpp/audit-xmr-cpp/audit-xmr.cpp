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
#include <csignal>
#include <condition_variable>

#include "audit.hpp"
#include "rpc.hpp"

#define VER "0.1"

namespace fs = std::filesystem;

std::string g_log_path; // Definida globalmente para ser acessada por todos os arquivos
std::string g_rpc_url;  // Servidor RPC global
std::mutex log_mutex;   // Mutex para sincronizar os logs
std::mutex queue_mutex; // Mutex para sincronizar o mapa
std::condition_variable queue_cv; // Condição para notificar a thread de escrita
std::map<int, AuditResult> result_map; // Mapa ordenado por altura
std::atomic<bool> interrupted(false);  // Flag para interrupção
std::atomic<bool> done(false);         // Flag para indicar que todas as threads terminaram
std::string g_csv_path;                // Caminho do CSV global
int g_max_retries = 99;                // Número máximo de retentativas (padrão)
int g_timeout = 20;                    // Timeout em segundos (padrão)
int g_start_block = 0;                 // Bloco inicial para escrita sequencial

// Função de log com timestamp
void log_message(const std::string& log_path, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

    std::lock_guard<std::mutex> lock(log_mutex);
    std::ofstream log_file(log_path, std::ios::app);
    if (log_file.is_open()) {
        log_file << "[" << timestamp.str() << "] " << message << std::endl;
    }
}

// Função para escrever resultados no CSV a partir do mapa
void csv_writer_thread(int start_block, int end_block) {
    std::ofstream csv(g_csv_path, std::ios::app); // Abrir em modo append
    if (!csv.is_open()) {
        log_message(g_log_path, "[ERRO] Não foi possível abrir o arquivo CSV: " + g_csv_path);
        return;
    }

    // Escrever o cabeçalho apenas se o arquivo estiver vazio
    std::ifstream check_csv(g_csv_path);
    if (check_csv.peek() == std::ifstream::traits_type::eof()) {
        csv << "Altura,Hash,RecompensaReal,CoinbaseOutputs,TotalMinerado,Problemas,Status\n";
    }
    check_csv.close();

    int next_height = start_block;
    while (!done || next_height <= end_block) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, [&] { return result_map.count(next_height) || done; });

        if (result_map.count(next_height)) {
            AuditResult r = result_map[next_height];
            result_map.erase(next_height); // Remover após escrever
            lock.unlock(); // Liberar o mutex enquanto escreve no arquivo

            csv << r.height << ',' << r.hash << ',' << r.real_reward << ','
                << r.coinbase_outputs << ',' << r.total_mined << ','
                << r.issues_string() << ',' << r.status << '\n';

            next_height++;
            lock.lock(); // Rebloquear para verificar o próximo
        } else if (done) {
            break; // Se todas as threads terminaram e o próximo bloco não está presente, sair
        }
        lock.unlock();
    }

    csv.close();
    log_message(g_log_path, "[INFO] Thread de escrita finalizada. CSV salvo em: " + g_csv_path);
}

// Handler de sinal para Ctrl+C
void signal_handler(int signal) {
    if (signal == SIGINT) {
        interrupted = true;
        done = true; // Sinalizar que as threads devem parar
        queue_cv.notify_all(); // Acordar a thread de escrita
        log_message(g_log_path, "[INFO] Interrupção detectada (Ctrl+C). Finalizando e salvando resultados...");
        std::cout << "Interrompido. Resultados salvos em: " << g_csv_path << std::endl;
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
    std::signal(SIGINT, signal_handler);

    std::string config_file = "audit-xmr.cfg";
    auto config = load_config(config_file);

    std::string rpc_url = config.count("rpc_url") ? config["rpc_url"] : "http://127.0.0.1:18081/json_rpc";
    int user_thread_count = config.count("threads") ? std::stoi(config["threads"]) : 1;
    std::string output_dir = config.count("output_dir") ? config["output_dir"] : "out";
    g_max_retries = config.count("max_retries") ? std::stoi(config["max_retries"]) : 99;
    g_timeout = config.count("timeout") ? std::stoi(config["timeout"]) : 20;

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
    g_rpc_url = rpc_url;

    fs::path out_dir = fs::path(output_dir);
    fs::create_directories(out_dir);

    g_csv_path = (out_dir / "auditoria_monero.csv").string();
    std::string log_path = (out_dir / "audit_log.txt").string();
    g_log_path = log_path;

    auto log = [&](const std::string& msg) {
        log_message(log_path, msg);
    };

    log("[INFO] Script iniciado com servidor RPC: " + g_rpc_url);
    log("[INFO] Configurações: max_retries=" + std::to_string(g_max_retries) + ", timeout=" + std::to_string(g_timeout) + "s");

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

            std::lock_guard<std::mutex> lock(queue_mutex);
            result_map[single_block] = r;
            queue_cv.notify_one();
            std::ofstream csv(g_csv_path);
            csv << "Altura,Hash,RecompensaReal,CoinbaseOutputs,TotalMinerado,Problemas,Status\n";
            csv << r.height << ',' << r.hash << ',' << r.real_reward << ','
                << r.coinbase_outputs << ',' << r.total_mined << ','
                << r.issues_string() << ',' << r.status << '\n';
            csv.close();
        } else {
            std::cerr << "[ERRO] Auditoria falhou para o bloco " << single_block << std::endl;
            log("[ERRO] Auditoria falhou para o bloco " + std::to_string(single_block));
        }
        return 0;
    }

    if (start_block < 0 || end_block < 0) {
        start_block = 0;
        end_block = get_blockchain_height() - 1;
        log("[INFO] Intervalo não especificado. Usando do bloco " + std::to_string(start_block) + " até " + std::to_string(end_block));
    }

    g_start_block = start_block;
    log("[INFO] Iniciando auditoria de " + std::to_string(start_block) + " até " + std::to_string(end_block));

    int max_threads = std::thread::hardware_concurrency();
    if (user_thread_count > max_threads) {
        std::cerr << "[AVISO] Foi solicitado " << user_thread_count << " threads, mas o sistema só possui " << max_threads << ". Usando mesmo assim.\n";
        log("[AVISO] Threads solicitados: " + std::to_string(user_thread_count) + ", disponíveis: " + std::to_string(max_threads));
    }

    int thread_count = std::max(1, user_thread_count);
    int total_blocks = end_block - start_block + 1;

    std::thread csv_writer(csv_writer_thread, start_block, end_block);
    csv_writer.detach();

    auto worker = [&](int tid, int from, int to) {
        for (int h = from; h <= to && !interrupted; ++h) {
            log("[DEBUG] Thread " + std::to_string(tid) + " auditando bloco " + std::to_string(h));
            auto res = audit_block(h);
            if (res.has_value()) {
                const auto& r = res.value();
                log("[DEBUG] Bloco " + std::to_string(h) + " auditado com status " + r.status);
                std::lock_guard<std::mutex> lock(queue_mutex);
                result_map[h] = r;
                queue_cv.notify_one();
            } else {
                log("[ERRO] Falha na auditoria do bloco " + std::to_string(h));
            }
        }
    };

    std::vector<std::thread> threads;
    int chunk_size = total_blocks / thread_count;

    for (int i = 0; i < thread_count; ++i) {
        int from = start_block + i * chunk_size;
        int to = (i == thread_count - 1) ? end_block : from + chunk_size - 1;
        log("[DEBUG] Iniciando thread " + std::to_string(i) + " para blocos " + std::to_string(from) + "-" + std::to_string(to));
        threads.emplace_back(worker, i, from, to);
    }

    for (auto& t : threads) {
        t.join();
    }

    done = true;
    queue_cv.notify_one();

    if (!interrupted) {
        log("[INFO] Auditoria finalizada. Aguardando escrita final no CSV...");
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Dar tempo para a thread de escrita finalizar
        std::cout << "Auditoria concluída. Resultados salvos em: " << g_csv_path << std::endl;
    }

    return 0;
}
