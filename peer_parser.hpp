#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <sstream>
#include <iomanip>
using namespace std;

struct Peer {
    string ip;
    uint16_t port;
};

inline vector<Peer> parseCompactPeers(const vector<uint8_t>& peersBytes) {
    vector<Peer> peers;

    if(peersBytes.size() % 6 != 0)
        throw runtime_error("Invalid compact peers length");

    for(size_t i = 0; i < peersBytes.size(); i += 6)
    {
        string ip = to_string(peersBytes[i]) + "." +
                    to_string(peersBytes[i + 1]) + "." +
                    to_string(peersBytes[i + 2]) + "." +
                    to_string(peersBytes[i + 3]);

        uint16_t port = (static_cast<uint16_t>(peersBytes[i + 4]) << 8) |
                        static_cast<uint16_t>(peersBytes[i + 5]);

        peers.push_back({ip, port});
    }

    return peers;
}