#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "tcp_socket.hpp"
using namespace std;

struct PeerMessage {
    uint32_t length;
    uint8_t id;
    vector<uint8_t> payload;
};

struct ParsedPiece {
    uint32_t index;
    uint32_t begin;
    vector<uint8_t> blockData;
};

inline uint32_t bytesToUint32(const vector<uint8_t>& bytes) 
{
    if (bytes.size() < 4) {
        throw invalid_argument("Need at least 4 bytes to parse uint32_t");
    }

    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8)  |
            static_cast<uint32_t>(bytes[3]);
}

inline vector<uint8_t> uint32ToBytes(uint32_t val) 
{
    return {
        static_cast<uint8_t>((val >> 24) & 0xFF),
        static_cast<uint8_t>((val >> 16) & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>(val & 0xFF)
    };
}

inline PeerMessage readPeerMessage(int socket_fd)
{
    PeerMessage msg;
    vector<uint8_t> bufferRead = readExactBytes(socket_fd, 4);
    msg.length = bytesToUint32(bufferRead);

    if(msg.length == 0)
    {
        msg.id = 0;
        return msg;
    }

    vector<uint8_t> bodyBuffer = readExactBytes(socket_fd, msg.length);
    msg.id = bodyBuffer[0];

    if(msg.length > 1)
        msg.payload.assign(bodyBuffer.begin() + 1, bodyBuffer.end());

    return msg;
}

inline vector<uint8_t> createInterestedMessage()
{   
    vector<uint8_t> message = uint32ToBytes(1);
    message.push_back(2);
    return message;
}

inline vector<uint8_t> createRequestMessage(uint32_t index, uint32_t begin, uint32_t length)
{
    vector<uint8_t> message = uint32ToBytes(13); 
    message.push_back(6);                       
    vector<uint8_t> idxBytes = uint32ToBytes(index);
    message.insert(message.end(), idxBytes.begin(), idxBytes.end());
    vector<uint8_t> beginBytes = uint32ToBytes(begin);
    message.insert(message.end(), beginBytes.begin(), beginBytes.end());
    vector<uint8_t> lenBytes = uint32ToBytes(length);
    message.insert(message.end(), lenBytes.begin(), lenBytes.end());
    return message;
}

inline ParsedPiece parsePieceMessage(const PeerMessage& msg)
{
    ParsedPiece messagePiece;

    if(msg.id == 7 && msg.payload.size() >= 8)
    {
        vector<uint8_t> indexBytes(msg.payload.begin(), msg.payload.begin() + 4);
        messagePiece.index = bytesToUint32(indexBytes);

        vector<uint8_t> beginBytes(msg.payload.begin() + 4, msg.payload.begin() + 8);
        messagePiece.begin = bytesToUint32(beginBytes);
        messagePiece.blockData.assign(msg.payload.begin() + 8, msg.payload.end());
        return messagePiece;
    }

    throw runtime_error("Invalid or corrupted piece message received!");
}

inline vector<uint8_t> downloadPiece(int socket_fd, uint32_t pieceIndex, uint32_t pieceSize, uint32_t blockSize = 16384) 
{
    vector<uint8_t> fullPiece;
    fullPiece.reserve(pieceSize);

    uint32_t begin = 0;

    while (begin < pieceSize) 
    {
        uint32_t currentBlockSize = min(blockSize, pieceSize - begin);
        vector<uint8_t> reqMsg = createRequestMessage(pieceIndex, begin, currentBlockSize);
        sendAll(socket_fd, reqMsg);

        PeerMessage responseMsg = readPeerMessage(socket_fd);
        ParsedPiece parsed = parsePieceMessage(responseMsg);

        if (parsed.index != pieceIndex || parsed.begin != begin) 
            throw runtime_error("Received block index/offset mismatch!");

        fullPiece.insert(fullPiece.end(), parsed.blockData.begin(), parsed.blockData.end());
        begin += currentBlockSize;
    }

    return fullPiece;
}