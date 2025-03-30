#pragma once
#include <string>
#include <vector>
#include <optional>

struct AuditResult {
    int height;
    std::string hash;
    uint64_t real_reward;
    uint64_t coinbase_outputs;
    uint64_t total_mined;
    std::vector<std::string> issues;
    std::string status;

    std::string issues_string() const {
        if (issues.empty()) return "Nenhum";
        std::string joined;
        for (size_t i = 0; i < issues.size(); ++i) {
            joined += issues[i];
            if (i < issues.size() - 1) joined += "; ";
        }
        return joined;
    }
};

std::optional<AuditResult> audit_block(int height);
