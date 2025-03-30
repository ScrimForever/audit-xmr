// log.hpp
#pragma once
#include <string>

// Funções de log com e sem separador
void log_message(const std::string& log_path, const std::string& message);
void log_message(const std::string& log_path, const std::string& message, bool is_block_end);
