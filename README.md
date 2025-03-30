# Audit-XMR

`Audit-XMR` é uma ferramenta open-source para auditar blocos da blockchain Monero, verificando a consistência entre recompensas de bloco, saídas Coinbase e o total minerado. Desenvolvido em **C++** para desempenho otimizado com multi-threading e em **Python** para facilidade de uso e depuração, este projeto permite a auditoria independente do suprimento (supply) do Monero (XMR). Ele se conecta a um nó Monero via RPC e analisa cada bloco para garantir a integridade da emissão de moedas, essencial para uma criptomoeda que prioriza privacidade e descentralização.

## Objetivos e Importância

O principal objetivo do `audit-xmr` é detectar fraudes, como emissões indevidas ou manipulações de recompensas, capacitando:
- **Operadores de nós**: Auditar suas próprias instâncias.
- **Auditores independentes**: Verificar nós de terceiros.
- **Entusiastas e pesquisadores**: Validar a supply sem depender de exploradores centralizados.

"Talk is cheap, show me the CODE" — o código está disponível para todos verificarem e usarem.

## Código Fonte

- Repositório: [https://github.com/area31/audit-xmr](https://github.com/area31/audit-xmr)
- Mais informações no X: [https://x.com/coffnix/status/1906320712349925854](https://x.com/coffnix/status/1906320712349925854)
- Mais informações no Área31: [https://area31.net.br/wiki/Audite_a_blockchain_do_Monero_(XMR)](https://area31.net.br/wiki/Audite_a_blockchain_do_Monero_(XMR))

  
Responsável: [Coffnix](https://github.com/coffnix)

## Funcionalidades

- Auditoria de blocos individuais (`--block`) ou intervalos (`--range`).
- Validação de recompensas de bloco contra saídas Coinbase.
- Suporte a servidores RPC remotos configuráveis.
- Geração de logs detalhados para depuração.
- Saída em CSV com altura, hash, recompensa real, saídas Coinbase, total minerado, problemas e status.
- Multi-threading para auditorias em larga escala (C++).
- Validação cruzada de resultados via `audit-xmr-check`.

## Requisitos

### C++
- Compilador C++17 (ex.: `g++`)
- Bibliotecas: `libcurl` e `pthread`
- Biblioteca JSON: `nlohmann/json` (inclusa no código)

### Python
- Python 3.x
- Bibliotecas: `requests`, `tqdm` (instaláveis via `pip`)

## Instalação

### C++
1. Clone o repositório:
   ```bash
   git clone https://github.com/area31/audit-xmr.git
   cd audit-xmr
   ```
2. Compile com g++:
   ```bash
   g++ audit-xmr.cpp audit.cpp rpc.cpp -o audit-xmr -std=c++17 -lcurl -lpthread
   ```
   Ou use o script:
   ```bash
   ./build_gpp.sh
   ```
   Alternativamente, com CMake:
   ```bash
   ./build_cmake.sh
   ```

### Python
Instale as dependências:
```bash
pip install requests tqdm
```
Execute diretamente os scripts Python no diretório correspondente.

## Uso

### Configuração
Edite `audit-xmr.cfg` com os parâmetros do nó:
```plaintext
rpc_url=http://192.168.200.252:18081/json_rpc
threads=8
output_dir=out
max_retries=99
timeout=30
server=192.168.200.252
```

### Auditoria (C++)
Auditar um bloco específico (ex.: altura 445):
```bash
./audit-xmr --block 445
```

Auditar um intervalo (ex.: 0 a 500000):
```bash
./audit-xmr --range 0 500000 --threads max
```

### Validação (C++)
Validar o CSV gerado:
```bash
./audit-xmr-check out/auditoria_monero.csv
```

## Resultados

- CSV: `out/auditoria_monero.csv` com colunas: Altura, Hash, RecompensaReal, CoinbaseOutputs, TotalMinerado, Problemas, Status.
- Log: `out/audit_log.txt` com detalhes de depuração.

## Componentes do Projeto

- `audit-xmr`: Audita blocos em massa e salva resultados em CSV.
- `audit-xmr-check`: Revalida os dados do CSV contra um nó Monero via RPC.
- Módulos auxiliares: Comunicação RPC (`rpc.cpp/hpp`), logging (`log.cpp/hpp`), multi-threading (mutexes e threads), configuração e scripts de build.

## Estrutura Técnica

A estrutura `AuditResult` armazena os dados auditados:
- `height`: Altura do bloco.
- `hash`: Hash do bloco.
- `real_reward`: Recompensa oficial do bloco.
- `coinbase_outputs`: Soma das saídas Coinbase.
- `total_mined`: Total minerado (igual a coinbase_outputs neste caso).
- `issues`: Lista de discrepâncias (ex.: "Reward != CoinBase").
- `status`: "OK" ou "Discrepância".

## Detecção de Fraudes

O sistema flagra:
- Reward != CoinBase: Recompensa diverge das saídas.
- Reward != TotalMined: Total minerado inconsistente.
- CoinBase inválida: Estrutura da transação Coinbase incorreta.

Essas discrepâncias podem indicar nós maliciosos ou corrupção de dados.

## Performance do Python vs C++

Abaixo estão os resultados visuais comparando o desempenho das versões Python e C++ ao auditar blocos da blockchain Monero.

**Tempo de bloco único (ex.: 445)**  
Legenda: O C++ com multi-threading é significativamente mais rápido que o Python single-threaded para pequenos intervalos.

**Tempo de bloco único (ex.: 437654)**  
Legenda: O C++ escalona melhor com o aumento do número de blocos, enquanto o Python mostra desempenho linear.

*Nota: Os tempos variam conforme hardware, configuração de threads e latência da rede RPC.*

## Aplicabilidade Prática

- **Auditoria Local**: Verifique a integridade do seu nó.
- **Auditoria Cruzada**: Compare resultados entre diferentes nós.
- **Supply Independente**: Valide a emissão total sem confiar em terceiros.
- **Ambiente Offline**: Use o CSV em sistemas isolados.

## Benefícios para a Comunidade Monero

- **Transparência**: Qualquer um pode auditar a supply.
- **Descentralização**: Sem depender de sites ou empresas.
- **Segurança**: Detecta anomalias em nós.
- **Performance**: Multi-threading para auditorias rápidas.

## Conclusão

O audit-xmr é uma ferramenta essencial para a comunidade Monero, oferecendo auditoria independente, precisa e reproduzível. Ele prova que sim, é possível auditar o Monero — uma resposta direta aos maximalistas e skeptics. Liberdade requer verificação, não confiança.

*Membro do hackerspace felizão por mostrar pros maximalistas macacos, e um certo "policial famosinho bitcoinheiro", que sim, é possível auditar o Monero.*

## Próximos Passos

Agende auditorias periódicas com cron:
```bash
0 0 * * * /path/to/audit-xmr --range 0 $(/path/to/audit-xmr --block-height) >> /path/to/audit.log 2>&1
```

Contribua com melhorias no repositório!

## Veja Também

- [Monero Official Website](https://www.getmonero.org)
- [nlohmann/json](https://github.com/nlohmann/json)
