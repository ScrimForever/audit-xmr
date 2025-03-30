cmake_minimum_required(VERSION 3.10)
project(audit_xmr)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)

add_executable(audit-xmr
    audit-xmr.cpp  # Corrigido de main.cpp para audit-xmr.cpp
    audit.cpp
    rpc.cpp
)

target_include_directories(audit-xmr PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(audit-xmr PRIVATE ${CURL_LIBRARIES})
