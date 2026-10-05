#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
#endif

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include "handshake.hpp"
#include "tcp_socket.hpp"

using namespace std;

int main() {
    try {
        #if defined(_WIN32) || defined(_WIN64)
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        #endif

        int serverPort = 8999;
        cout << "[*] Mock Peer Server starting on 127.0.0.1:" << serverPort << "...\n";

        int serverSock = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSock < int(0)) {
            throw runtime_error("Failed to create server socket.");
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(serverPort);

        if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            throw runtime_error("Bind failed.");
        }

        listen(serverSock, 1);
        cout << "[*] Waiting for incoming client handshake...\n";

        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSock < 0) {
            throw runtime_error("Accept failed.");
        }
        cout << "[+] Client connected!\n";

        // Read the 68-byte handshake from your client
        vector<uint8_t> clientHandshake = readExactBytes(clientSock, 68);
        cout << "[+] Received 68-byte handshake from client!\n";

        // Validate protocol string
        string pstr(clientHandshake.begin() + 1, clientHandshake.begin() + 20);
        cout << "    -> Protocol: " << pstr << "\n";

        // Extract info hash received from client
        vector<uint8_t> clientInfoHash(clientHandshake.begin() + 28, clientHandshake.begin() + 48);

        // Echo back a valid handshake using the same info hash
        string mockPeerId = "-MP0001-987654321012";
        vector<uint8_t> responseHandshake = createHandshake(clientInfoHash, mockPeerId);

        cout << "[*] Sending handshake response back to client...\n";
        sendAll(clientSock, responseHandshake);
        cout << "[+] Handshake exchange complete successfully on mock server!\n";

        // Cleanup
        #if defined(_WIN32) || defined(_WIN64)
            closesocket(clientSock);
            closesocket(serverSock);
            WSACleanup();
        #else
            close(clientSock);
            close(serverSock);
        #endif

    } catch (const exception& e) {
        cerr << "[-] Mock Server Error: " << e.what() << "\n";
    }
    return 0;
}