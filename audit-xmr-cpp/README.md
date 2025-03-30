emerge -av nlohmann_json

zypper install gcc13 gcc13-c++ libcurl-devel nlohmann_json-devel

update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100

g++ audit-xmr.cpp audit.cpp rpc.cpp -o audit-xmr -std=c++17 -lcurl -lpthread

g++ audit-xmr-check.cpp audit.cpp rpc.cpp -o audit-xmr-check -std=c++17 -lcurl -lpthread
