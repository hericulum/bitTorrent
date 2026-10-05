#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
#endif

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include "tcp_socket.hpp"
#include "handshake.hpp"

using namespace std;

int main() {
    try {
        #if defined(_WIN32) || defined(_WIN64)
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        #endif

        string targetIp = "127.0.0.1";
        int targetPort = 8999;
        vector<uint8_t> infoHash(20, 'A');
        string peerId = "-MY0001-123456789012";

        cout << "[*] Connecting directly to mock peer at " << targetIp << ":" << targetPort << "...\n";

        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(targetIp.c_str(), to_string(targetPort).c_str(), &hints, &res) != 0) {
            throw runtime_error("Failed to resolve local target.");
        }

        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
            freeaddrinfo(res);
            throw runtime_error("Connection to mock peer refused. Is the mock server running?");
        }
        freeaddrinfo(res);
        cout << "[+] Connected to mock peer!\n";

        // Build and send handshake
        cout << "[*] Building and sending handshake...\n";
        vector<uint8_t> handshakeMsg = createHandshake(infoHash, peerId);
        sendAll(sock, handshakeMsg);

        // Read response
        cout << "[*] Waiting for mock peer handshake response...\n";
        vector<uint8_t> response = readExactBytes(sock, 68);

        cout << "[+] Success! Received " << response.size() << " bytes back from peer.\n";
        string respProtocol(response.begin() + 1, response.begin() + 20);
        cout << "    -> Echoed Protocol: " << respProtocol << "\n";

        #if defined(_WIN32) || defined(_WIN64)
            closesocket(sock);
            WSACleanup();
        #else
            close(sock);
        #endif

    } catch (const exception& e) {
        cerr << "[-] Error: " << e.what() << "\n";
        #if defined(_WIN32) || defined(_WIN64)
            WSACleanup();
        #endif
        return 1;
    }
    return 0;
}