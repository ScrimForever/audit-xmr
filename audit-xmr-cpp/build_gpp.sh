#!/bin/bash
# build_gpp.sh
# Compila os binários diretamente com g++

# Compila o binário principal
g++ audit-xmr.cpp audit.cpp rpc.cpp -o audit-xmr -std=c++17 -lcurl -lpthread

# Compila o binário de validação
g++ audit-xmr-check.cpp audit.cpp rpc.cpp -o audit-xmr-check -std=c++17 -lcurl -lpthread

echo "Build concluído. Os binários 'audit-xmr' e 'audit-xmr-check' foram gerados no diretório atual."
