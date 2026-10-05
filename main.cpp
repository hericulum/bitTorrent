#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")

    #if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
        using ssize_t = SSIZE_T;
        #define _SSIZE_T_DEFINED
    #endif
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

#include "tcp_socket.hpp"
#include "handshake.hpp"
#include "peer_message.hpp"
#include "sha1.hpp"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <iostream>
using namespace std;

int main()
{
    #if defined(_WIN32) || defined(_WIN64)
        WSADATA wsaData;
        int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        
        if(wsaResult != 0)
            throw runtime_error("WSAStartup failed with error: " + to_string(wsaResult));
    #endif

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    #if defined(_WIN32) || defined(_WIN64)
        if(sockfd == INVALID_SOCKET)
            throw runtime_error("Invalid socket error");
    #else
        if(sockfd < 0)
            throw runtime_error("Invalid socket error");
    #endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
        throw runtime_error("Invalid address format");
    }

    int connectResult = ::connect(
        sockfd, 
        reinterpret_cast<struct sockaddr*>(&serverAddr), 
        sizeof(serverAddr)
    );

    #if defined(_WIN32) || defined(_WIN64)
        if (connectResult == SOCKET_ERROR)
            throw runtime_error("Connection failed with error: " + to_string(WSAGetLastError()));
    #else
        if (connectResult < 0)
            throw runtime_error("Connection failed with error: " + to_string(errno));
    #endif

    vector<uint8_t> infoHash(20, 0xAB);
    string peerId = "-MY0001-012345678901";
    vector<uint8_t> handshakeBuffer = createHandshake(infoHash, peerId);
    sendAll(sockfd, handshakeBuffer);
    vector<uint8_t> responseBuffer = readExactBytes(sockfd, 68);

    if(responseBuffer[0] == 19)
    {
        string checker(responseBuffer.begin() + 1, responseBuffer.begin() + 20);

        if(checker == "BitTorrent protocol")
        {
            if(vector<uint8_t>(responseBuffer.begin() + 28, responseBuffer.begin() + 48) == infoHash)
            {
                cout << "[+] Handshake successful!" << endl;

                // Read bitfield from server
                PeerMessage msg = readPeerMessage(sockfd);

                // Send interested message and await unchoke
                vector<uint8_t> interestedMsg = createInterestedMessage();
                sendAll(sockfd, interestedMsg);
                PeerMessage unchokeMsg = readPeerMessage(sockfd);

                // Download full 64 KiB piece (4 blocks of 16384 bytes)
                uint32_t pieceSize = 65536;
                vector<uint8_t> pieceData = downloadPiece(sockfd, 0, pieceSize);
                cout << "[+] Downloaded Full Piece! Total bytes: " << pieceData.size() << endl;

                // Calculate expected SHA-1 hash for matching 'A' byte payload from mock server
                vector<uint8_t> dummyData(pieceSize, 'A');
                vector<uint8_t> expectedPieceHash = computeSHA1(dummyData);

                // Verify downloaded piece hash
                vector<uint8_t> computedHash = computeSHA1(pieceData);

                if (computedHash == expectedPieceHash) 
                    cout << "[+] Piece SHA-1 verification PASSED!" << endl;
                else 
                    cerr << "[-] Hash mismatch! Piece corrupted." << endl;
            }
            else
                throw runtime_error("Info hash verification failed!");
        }
        else
            throw runtime_error("Protocol string verification failed!");
    }
    else
        throw runtime_error("Protocol length byte not equal to 19!");

    #if defined(_WIN32) || defined(_WIN64)
        closesocket(sockfd);
        WSACleanup();
    #else
        close(sockfd);
    #endif

    return 0;
}