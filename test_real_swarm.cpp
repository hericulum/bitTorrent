#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
#endif

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include "torrent_file.hpp"
#include "tracker.hpp"
#include "tcp_socket.hpp"
#include "peer_parser.hpp"
#include "handshake.hpp"

using namespace std;

int main() {
    try {
        #if defined(_WIN32) || defined(_WIN64)
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        #endif

        // 1. Load and parse your real .torrent file
        string torrentFilename = "rdr2.torrent";
        cout << "[*] Parsing torrent file: " << torrentFilename << "...\n";
        
        TorrentFile torrent = loadTorrentFile(torrentFilename); 

        cout << "[+] Announce URL: " << torrent.announce << "\n";
        cout << "[+] Torrent Name: " << torrent.name << "\n";
        cout << "[+] Total Length: " << torrent.totalLength << " bytes\n";
        cout << "[+] Info Hash successfully extracted!\n";

        string peerId = "-MY0001-123456789012";

        // 2. Query Tracker using the real Announce URL and Info Hash
        cout << "[*] Querying tracker for real swarm peers...\n";
        ParsedUrl parsedUrl = parseAnnounceUrl(torrent.announce);
        string queryPath = buildTrackerQuery(parsedUrl, torrent.infoHash, peerId, 1024);
        string httpRequest = buildHttpRequest(parsedUrl, queryPath);

        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(parsedUrl.host.c_str(), parsedUrl.port.c_str(), &hints, &res) != 0) {
            throw runtime_error("Failed to resolve tracker host: " + parsedUrl.host);
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
        
        size_t lenStart = peersKeyPos + 7;
        size_t colonPos = trackerResponse.find(':', lenStart);
        string lengthStr = trackerResponse.substr(lenStart, colonPos - lenStart);
        size_t peersDataLen = stoul(lengthStr);

        string rawPeersStr = trackerResponse.substr(colonPos + 1, peersDataLen);
        vector<uint8_t> peersBytes(rawPeersStr.begin(), rawPeersStr.end());
        vector<Peer> peers = parseCompactPeers(peersBytes);

        cout << "[+] Found " << peers.size() << " peer(s) in the real swarm.\n";
        if (peers.empty()) {
            throw runtime_error("Peer list is empty.");
        }

        // 4. Try connecting to real peers and shaking hands
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

            if (connect(peerSock, peerRes->ai_addr, peerRes->ai_addrlen) == 0) {
                freeaddrinfo(peerRes);
                activePeer = targetPeer;
                success = true;
                break; 
            }

            freeaddrinfo(peerRes);
            #if defined(_WIN32) || defined(_WIN64)
                closesocket(peerSock);
            #else
                close(peerSock);
            #endif
        }

        if (!success) {
            throw runtime_error("Could not connect to any peers in the real swarm.");
        }

        cout << "[+] Connected to real swarm peer " << activePeer.ip << ":" << activePeer.port << "!\n";

        // 5. Send Handshake using the real Info Hash from rdr2.torrent
        cout << "[*] Building and sending handshake with real info hash...\n";
        vector<uint8_t> handshakeMsg = createHandshake(torrent.infoHash, peerId);
        sendAll(peerSock, handshakeMsg);

        // 6. Read Response
        cout << "[*] Waiting for peer handshake response...\n";
        vector<uint8_t> responseHandshake = readExactBytes(peerSock, 68);

        cout << "[+] Success! Handshake echoed back from real peer (" << responseHandshake.size() << " bytes)!\n";
        string respProtocol(responseHandshake.begin() + 1, responseHandshake.begin() + 20);
        cout << "    -> Protocol echoed: " << respProtocol << "\n";

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