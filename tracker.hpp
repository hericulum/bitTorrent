#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <random>
#include <cstring>
#include <sstream>
#include "url_encoder.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600 // Windows Vista or later required for getaddrinfo
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

using namespace std;

struct ParsedUrl {
    string scheme; // "http" or "udp"
    string host;
    string port;
    string path;
};

struct Peer {
    string ip;
    uint16_t port;
};

inline bool startsWith(const string& str, const string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

inline ParsedUrl parseAnnounceUrl(const string& url) {
    ParsedUrl result;
    
    if (startsWith(url, "http://") || startsWith(url, "https://")) {
        result.scheme = "http";
        size_t schemeLen = startsWith(url, "https://") ? 8 : 7;
        string remaining = url.substr(schemeLen);
        size_t pathPos = remaining.find('/');
        string hostAndPort;

        if (pathPos != string::npos) {
            result.path = remaining.substr(pathPos);
            hostAndPort = remaining.substr(0, pathPos);
        } else {
            result.path = "/";
            hostAndPort = remaining;
        }

        size_t colonPos = hostAndPort.find(':');
        if (colonPos != string::npos) {
            result.host = hostAndPort.substr(0, colonPos);
            result.port = hostAndPort.substr(colonPos + 1);
        } else {
            result.host = hostAndPort;
            result.port = (result.scheme == "https") ? "443" : "80";
        }
    }
    else if (startsWith(url, "udp://")) {
        result.scheme = "udp";
        string remaining = url.substr(6);
        size_t pathPos = remaining.find('/');
        string hostAndPort = (pathPos == string::npos) ? remaining : remaining.substr(0, pathPos);
        result.path = (pathPos == string::npos) ? "/" : remaining.substr(pathPos);

        size_t colonPos = hostAndPort.find(':');
        if (colonPos != string::npos) {
            result.host = hostAndPort.substr(0, colonPos);
            result.port = hostAndPort.substr(colonPos + 1);
        } else {
            throw runtime_error("Invalid UDP tracker URL format (missing port)");
        }
    }
    else {
        throw runtime_error("Invalid or unsupported URL scheme");
    }

    while (result.path.rfind("//", 0) == 0) {
        result.path.erase(0, 1);
    }

    return result;
}

inline string buildTrackerQuery(
    const ParsedUrl& parsedUrl,
    const vector<uint8_t>& infoHash,
    const string& peerId,
    int64_t totalLength,
    uint16_t port = 6881
) {
    string encodedInfoHash = urlEncodeBytes(infoHash);
    vector<uint8_t> peerIdBytes(peerId.begin(), peerId.end());
    string encodedPeerId = urlEncodeBytes(peerIdBytes);

    stringstream query;
    query << parsedUrl.path
          << "?info_hash=" << encodedInfoHash
          << "&peer_id=" << encodedPeerId
          << "&port=" << port
          << "&uploaded=0"
          << "&downloaded=0"
          << "&left=" << totalLength
          << "&compact=1";

    return query.str();
}

inline string buildHttpRequest(const ParsedUrl& parsedUrl, const string& queryPath) {
    stringstream req;
    req << "GET " << queryPath << " HTTP/1.1\r\n"
        << "Host: " << parsedUrl.host << ":" << parsedUrl.port << "\r\n"
        << "User-Agent: BitTorrentClient/1.0\r\n"
        << "Connection: close\r\n"
        << "\r\n";
    return req.str();
}

