#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
#endif

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include "tracker.hpp"
#include "tcp_socket.hpp"
#include "peer_parser.hpp"
#include "handshake.hpp"

using namespace std;

int main() {
    try {
        // 1. Initialize Winsock on Windows
        #if defined(_WIN32) || defined(_WIN64)
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                throw runtime_error("WSAStartup failed.");
            }
        #endif

        string announceUrl = "http://tracker.opentrackr.org:1337/announce";
        vector<uint8_t> infoHash(20, 'A'); 
        string peerId = "-MY0001-123456789012";

        // 2. Query Tracker for Peers
        cout << "[*] Querying tracker for peers...\n";
        ParsedUrl parsedUrl = parseAnnounceUrl(announceUrl);
        string queryPath = buildTrackerQuery(parsedUrl, infoHash, peerId, 1024);
        string httpRequest = buildHttpRequest(parsedUrl, queryPath);

        // Resolve tracker host
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(parsedUrl.host.c_str(), parsedUrl.port.c_str(), &hints, &res) != 0) {
            throw runtime_error("Failed to resolve tracker host.");
        }

        int trackerSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (connect(trackerSock, res->ai_addr, res->ai_addrlen) < 0) {
            freeaddrinfo(res);
            throw runtime_error("Failed to connect to tracker.");
        }
        freeaddrinfo(res);

        vector<uint8_t> reqBytes(httpRequest.begin(), httpRequest.end());
        sendAll(trackerSock, reqBytes);

        // Read tracker response
        vector<uint8_t> trackerRespBuf;
        char tempBuf[4096];
        ssize_t n;
        while ((n = recv(trackerSock, tempBuf, sizeof(tempBuf), 0)) > 0) {
            trackerRespBuf.insert(trackerRespBuf.end(), tempBuf, tempBuf + n);
        }
        
        #if defined(_WIN32) || defined(_WIN64)
            closesocket(trackerSock);
        #else
            close(trackerSock);
        #endif

        string trackerResponse(trackerRespBuf.begin(), trackerRespBuf.end());

        // 3. Parse Bencoded 'peers' field dynamically
        size_t peersKeyPos = trackerResponse.find("5:peers");
        if (peersKeyPos == string::npos) {
            throw runtime_error("No peers found in tracker response.");
        }
        
        // "5:peers" is 7 characters long. Right after it is the length of the string value (e.g., "6:")
        size_t lenStart = peersKeyPos + 7;
        size_t colonPos = trackerResponse.find(':', lenStart);
        if (colonPos == string::npos) {
            throw runtime_error("Malformed bencoded peers field.");
        }

        string lengthStr = trackerResponse.substr(lenStart, colonPos - lenStart);
        size_t peersDataLen = stoul(lengthStr);

        // Extract exact raw peer bytes
        if (colonPos + 1 + peersDataLen > trackerResponse.size()) {
            throw runtime_error("Peers data length exceeds response size.");
        }
        
        string rawPeersStr = trackerResponse.substr(colonPos + 1, peersDataLen);
        vector<uint8_t> peersBytes(rawPeersStr.begin(), rawPeersStr.end());
        vector<Peer> peers = parseCompactPeers(peersBytes);

        cout << "[+] Found " << peers.size() << " peer(s) from tracker.\n";
        if (peers.empty()) {
            throw runtime_error("Peer list is empty.");
        }

        // 4. Loop through peers and attempt connection & handshake
        int peerSock = -1;
        bool success = false;
        Peer activePeer;

        for (const auto& targetPeer : peers) {
            cout << "[*] Trying peer " << targetPeer.ip << ":" << targetPeer.port << "...\n";
            
            addrinfo peerHints{}, *peerRes = nullptr;
            peerHints.ai_family = AF_INET;
            peerHints.ai_socktype = SOCK_STREAM;

            if (getaddrinfo(targetPeer.ip.c_str(), to_string(targetPeer.port).c_str(), &peerHints, &peerRes) != 0) {
                continue;
            }

            peerSock = socket(peerRes->ai_family, peerRes->ai_socktype, peerRes->ai_protocol);
            if (peerSock < 0) {
                freeaddrinfo(peerRes);
                continue;
            }

            // Attempt connection
            if (connect(peerSock, peerRes->ai_addr, peerRes->ai_addrlen) == 0) {
                freeaddrinfo(peerRes);
                activePeer = targetPeer;
                success = true;
                break; // Connected successfully!
            }

            freeaddrinfo(peerRes);
            #if defined(_WIN32) || defined(_WIN64)
                closesocket(peerSock);
            #else
                close(peerSock);
            #endif
        }

        if (!success) {
            throw runtime_error("Failed to connect to any available peers. Try running again for a fresh peer list.");
        }

        cout << "[+] Connected to peer " << activePeer.ip << ":" << activePeer.port << " successfully!\n";

        // 5. Build and Send Handshake
        cout << "[*] Building 68-byte handshake...\n";
        vector<uint8_t> handshakeMsg = createHandshake(infoHash, peerId);

        cout << "[*] Sending handshake to peer...\n";
        sendAll(peerSock, handshakeMsg);

        // 6. Read Peer's Handshake Response (Exact 68 bytes)
        cout << "[*] Waiting for peer handshake response...\n";
        vector<uint8_t> responseHandshake = readExactBytes(peerSock, 68);

        cout << "[+] Handshake received back from peer (" << responseHandshake.size() << " bytes)!\n";
        
        // Validate protocol string
        string respProtocol(responseHandshake.begin() + 1, responseHandshake.begin() + 20);
        cout << "    -> Protocol echoed: " << respProtocol << "\n";

        // Cleanup socket and Winsock
        #if defined(_WIN32) || defined(_WIN64)
            closesocket(peerSock);
            WSACleanup();
        #else
            close(peerSock);
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