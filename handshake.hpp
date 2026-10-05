#pragma once

#include <vector>
#include <string>
#include <cstdint>
using namespace std;

vector<uint8_t> createHandshake (const vector<uint8_t>& infoHash, const string& peerId)
{
    vector<uint8_t> handshake(68, 0);
    handshake[0] = 19; //1 byte: 19
    string pstr = "BitTorrent protocol"; 
    copy(pstr.begin(), pstr.end(), handshake.begin() + 1); //19 bytes "BitTorrent protocol"
    //8 bytes left as zeroes (reserved for errors)
    copy(infoHash.begin(), infoHash.end(), handshake.begin() + 28); // 28 to 47 (Info Hash)
    copy(peerId.begin(), peerId.end(), handshake.begin() + 48); // 48 to 67 (Peer ID)
    return handshake;
}