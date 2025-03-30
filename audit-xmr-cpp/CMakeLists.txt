cmake_minimum_required(VERSION 3.10)
project(audit_xmr)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# Executável principal audit-xmr
add_executable(audit-xmr
    audit-xmr.cpp
    audit.cpp
    rpc.cpp
    log.cpp  # Adicionado aqui
)

target_include_directories(audit-xmr PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(audit-xmr PRIVATE ${CURL_LIBRARIES} Threads::Threads)

# Executável de validação audit-xmr-check
add_executable(audit-xmr-check
    audit-xmr-check.cpp
    audit.cpp
    rpc.cpp
    log.cpp  # Adicionado aqui
)

target_include_directories(audit-xmr-check PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(audit-xmr-check PRIVATE ${CURL_LIBRARIES} Threads::Threads)
