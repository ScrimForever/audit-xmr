#!/bin/bash
# build_cmake.sh
# Cria o diretório de build, compila com CMake, copia os executáveis para o diretório atual e limpa o build

BUILD_DIR=build

mkdir -p $BUILD_DIR
cmake -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 -S . -B build
cmake --build build


# Copia os binários para o diretório atual
cp $BUILD_DIR/audit-xmr .
cp $BUILD_DIR/audit-xmr-check .

# Remove o diretório de build
rm -rf $BUILD_DIR

echo "Build concluído. Os binários 'audit-xmr' e 'audit-xmr-check' foram gerados no diretório atual."
