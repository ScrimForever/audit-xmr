import json
import requests
import csv
import time
import subprocess
import argparse
from tqdm import tqdm
from datetime import datetime

# Constantes
RPC_URL = "http://148.63.215.132:18081/json_rpc"
TOLERANCE = 1e9  # 0.001 XMR
BATCH_SIZE = 50
ATOMIC_UNITS = 1e12
MONEROD_PATH = "monerod"
TIMEOUT = 20
MAX_RETRIES = 99

# Timestamp para arquivos
timestamp = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
CSV_FILE = f"auditoria_monero_{timestamp}.csv"
LOG_FILE = f"auditoria_monero_log_{timestamp}.txt"

def log_message(message):
    with open(LOG_FILE, "a") as f:
        f.write(f"[{datetime.now().isoformat()}] {message}\n")
        f.flush()

def rpc_call(method, params=None):
    payload = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": method,
        "params": params or {}
    }
    log_message(f"[DEBUG] Chamando RPC: {method} com params {params}")
    try:
        response = requests.post(RPC_URL, json=payload, timeout=TIMEOUT)
        response.raise_for_status()
        result = response.json().get("result")
        log_message(f"[DEBUG] Resposta RPC: {result}")
        return result
    except Exception as e:
        log_message(f"[ERRO] RPC {method}: {str(e)}")
        return None

def get_block_height():
    result = rpc_call("get_block_count")
    return result["count"] - 1 if result else None

def get_block_info(height):
    result = rpc_call("get_block", {"height": height})
    if not result:
        log_message(f"[ERRO] Falha no get_block {height}")
        return None
    log_message(f"[DEBUG] Bloco {height} obtido com sucesso")
    return {
        "height": height,
        "hash": result["block_header"]["hash"],
        "json": json.loads(result["json"])
    }

def get_transaction_details(tx_hash):
    result = rpc_call("get_transactions", {"txs_hashes": [tx_hash], "decode_as_json": True})
    if not result or "txs" not in result or not result["txs"]:
        log_message(f"[ERRO] Falha ao obter detalhes da transação {tx_hash}")
        return None
    return json.loads(result["txs"][0]["as_json"])

def get_real_reward_from_shell(height):
    retries = 0
    while retries < MAX_RETRIES:
        try:
            cmd = [MONEROD_PATH, "--rpc-bind-ip", "148.63.215.132", "print_block", str(height)]
            log_message(f"[DEBUG] Tentativa {retries+1}/{MAX_RETRIES} no bloco {height}: {' '.join(cmd)}")
            output = subprocess.check_output(cmd, text=True, timeout=TIMEOUT)
            log_message(f"[DEBUG] Output bloco {height}: {output}")
            for line in output.splitlines():
                if "reward:" in line:
                    reward = int(float(line.split(":")[1].strip()) * ATOMIC_UNITS)
                    log_message(f"[DEBUG] Reward bloco {height}: {reward}")
                    return reward
            log_message(f"[ERRO] 'reward:' não encontrado no bloco {height}")
            return None
        except subprocess.TimeoutExpired:
            retries += 1
            log_message(f"[TIMEOUT] Tentativa {retries}/{MAX_RETRIES} no bloco {height}")
        except Exception as e:
            retries += 1
            log_message(f"[ERRO] Shell bloco {height}: {e}")
        time.sleep(1)
    log_message(f"[FALHA] {MAX_RETRIES} tentativas no bloco {height}")
    return None

def audit_block(block_info):
    height = block_info["height"]
    block_data = block_info["json"]

    log_message(f"[DEBUG] Auditoria iniciada para bloco {height}")
    real_reward = get_real_reward_from_shell(height)
    if real_reward is None:
        log_message(f"[ERRO] Recompensa real não obtida para bloco {height}")
        return None
    log_message(f"[DEBUG] Recompensa real bloco {height}: {real_reward}")

    miner_tx = block_data["miner_tx"]
    coinbase_outputs = sum(output["amount"] for output in miner_tx["vout"])
    log_message(f"[DEBUG] Saídas CoinBase bloco {height}: {coinbase_outputs}")

    tx_hashes = block_data.get("tx_hashes", [])
    total_tx_outputs = 0
    for tx_hash in tx_hashes:
        tx_details = get_transaction_details(tx_hash)
        if tx_details:
            total_tx_outputs += sum(output["amount"] for output in tx_details["vout"])
    log_message(f"[DEBUG] Total saídas TX bloco {height}: {total_tx_outputs}")

    total_mined = coinbase_outputs + total_tx_outputs
    log_message(f"[DEBUG] Total minerado bloco {height}: {total_mined}")

    issues = []
    if abs(real_reward - coinbase_outputs) > TOLERANCE:
        issues.append(f"Reward != CoinBase ({real_reward} vs {coinbase_outputs})")
    if abs(real_reward - total_mined) > TOLERANCE:
        issues.append(f"Reward != TotalMined ({real_reward} vs {total_mined})")
    if len(miner_tx["vin"]) != 1 or miner_tx["vin"][0]["gen"]["height"] != height:
        issues.append("CoinBase inválida")

    result = {
        "height": height,
        "hash": block_info["hash"],
        "real_reward": real_reward,
        "coinbase_outputs": coinbase_outputs,
        "total_mined": total_mined,
        "issues": issues,
        "status": "Discrepância" if issues else "OK"
    }
    log_message(f"[DEBUG] Resultado bloco {height}: status={result['status']}, issues={result['issues']}")
    return result

def audit_block_range(start_block, end_block):
    log_message(f"Iniciando auditoria de {start_block} até {end_block}")

    with open(CSV_FILE, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "Altura", "Hash", "Recompensa Real", "Saídas CoinBase", "Total Minerado", "Problemas", "Status"
        ])

    with tqdm(total=(end_block - start_block + 1), desc="Auditando blocos") as pbar:
        for start in range(start_block, end_block + 1, BATCH_SIZE):
            batch_end = min(start + BATCH_SIZE - 1, end_block)
            log_message(f"[DEBUG] Processando lote {start}-{batch_end}")

            for height in range(start, batch_end + 1):
                block_info = get_block_info(height)
                if not block_info:
                    log_message(f"[SKIP] Bloco {height} não obtido")
                    pbar.update(1)
                    continue
                result = audit_block(block_info)
                if not result:
                    log_message(f"[SKIP] Auditoria falhou para bloco {height}")
                    pbar.update(1)
                    continue

                # Gravar cada bloco imediatamente no CSV
                with open(CSV_FILE, "a", newline="") as f:
                    writer = csv.writer(f)
                    writer.writerow([
                        result["height"], result["hash"], result["real_reward"],
                        result["coinbase_outputs"], result["total_mined"],
                        "; ".join(result["issues"]) if result["issues"] else "Nenhum",
                        result["status"]
                    ])
                log_message(f"[DEBUG] Bloco {height} gravado no CSV: status={result['status']}")
                pbar.update(1)

            log_message(f"[DEBUG] Lote {start}-{batch_end} concluído")
            time.sleep(0.1)

    log_message(f"Auditoria completa. Resultado salvo em {CSV_FILE}")

def audit_single_block(block_num):
    log_message(f"Auditando bloco único: {block_num}")
    block_info = get_block_info(block_num)
    if not block_info:
        print(f"[ERRO] Não foi possível obter o bloco {block_num}")
        return
    result = audit_block(block_info)
    if not result:
        print(f"[ERRO] Auditoria falhou para o bloco {block_num}")
        return

    print(f"\n🧱 Bloco {result['height']} - Hash: {result['hash']}")
    print(f"💰 Recompensa real: {result['real_reward']} atomic units")
    print(f"📤 Saídas CoinBase: {result['coinbase_outputs']}")
    print(f"📦 Total minerado: {result['total_mined']}")
    print(f"⚠️ Status: {result['status']}")
    if result["issues"]:
        print("❗ Problemas:")
        for issue in result["issues"]:
            print(f"  - {issue}")
    else:
        print("✅ Nenhum problema detectado.")

if __name__ == "__main__":
    log_message("Script iniciado")
    parser = argparse.ArgumentParser(description="Auditoria da blockchain Monero")
    parser.add_argument("--block", type=int, help="Número do bloco para auditar individualmente")
    args = parser.parse_args()

    try:
        if args.block is not None:
            audit_single_block(args.block)
        else:
            end_block = get_block_height()
            if end_block is None:
                print("Erro ao obter a altura atual da blockchain.")
                log_message("[ERRO] Falha ao obter altura da blockchain")
            else:
                audit_block_range(0, end_block)
    except Exception as e:
        log_message(f"[CRÍTICO] Erro geral no script: {str(e)}")
        print(f"Erro crítico: {str(e)}")
