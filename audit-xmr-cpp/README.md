emerge -av nlohmann_json

g++ audit-xmr.cpp audit.cpp rpc.cpp -o audit-xmr -std=c++17 -lcurl -lpthread
g++ audit-xmr-check.cpp audit.cpp rpc.cpp -o audit-xmr-check -std=c++17 -lcurl -lpthread
