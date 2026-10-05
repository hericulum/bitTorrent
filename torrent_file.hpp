#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <cctype>
#include "bencoder.hpp"
#include "sha1.hpp"
using namespace std;

struct TorrentFile {
    string announce;
    int64_t pieceLength{0};
    int64_t totalLength{0};
    string name;
    vector<uint8_t> infoHash;             // 20-byte SHA-1 of the raw info dictionary
    vector<vector<uint8_t>> pieceHashes; // List of 20-byte piece hashes
};

// Helper: Extract raw byte slice of the "info" dictionary and calculate its SHA-1 hash
inline vector<uint8_t> extractAndHashInfoDict(const string& buffer) {
    size_t keyPos = buffer.find("4:info");
    if (keyPos == string::npos) {
        throw runtime_error("Invalid .torrent file: '4:info' key not found");
    }

    size_t start = keyPos + 6; 
    if (start >= buffer.size() || buffer[start] != 'd') {
        throw runtime_error("Invalid .torrent file: 'info' value must be a dictionary starting with 'd'");
    }

    size_t i = start;
    int depth = 0;
    size_t end = start;

    while (i < buffer.size()) {
        char ch = buffer[i];

        if (ch == 'd' || ch == 'l') {
            depth++;
        }
        else if (ch == 'e') {
            depth--;
            if (depth == 0) {
                end = i;
                break;
            }
        }
        else if (ch == 'i') {
            // Jump directly to the closing 'e' of this integer
            size_t ePos = buffer.find('e', i);
            if (ePos == string::npos) {
                throw runtime_error("Malformed bencode: unclosed integer");
            }
            i = ePos; // Landing on 'e'; outer i++ moves to the next token
        }
        else if (isdigit(static_cast<unsigned char>(ch))) {
            string num = "";
            while (i < buffer.size() && buffer[i] != ':') {
                num += buffer[i];
                i++;
            }
            // i is at ':'. Add length of string; outer i++ steps past the payload
            i += stoul(num); 
        }

        i++; 
    }

    string infoSlice = buffer.substr(start, end - start + 1);
    vector<uint8_t> sliceBytes(infoSlice.begin(), infoSlice.end());
    return computeSHA1(sliceBytes);
}

inline TorrentFile loadTorrentFile(const string& filepath) {
    // 1. Read entire file into a string buffer
    ifstream file(filepath, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Failed to open torrent file: " + filepath);
    }
    
    stringstream ss;
    ss << file.rdbuf();
    string buffer = ss.str();

    TorrentFile torrent;

    // 2. Compute exact Info Hash from raw byte buffer
    torrent.infoHash = extractAndHashInfoDict(buffer);

    // 3. Parse Bencode structure
    stringstream parseStream(buffer);
    BencodeValue root = parseBencode(parseStream);

    if (root.type != BencodeType::Dictionary) {
        throw runtime_error("Root bencode value must be a dictionary");
    }

    // Extract metadata fields
    torrent.announce = root.dictVal.at("announce").stringVal;
    const auto& infoDict = root.dictVal.at("info").dictVal;
    torrent.name = infoDict.at("name").stringVal;
    torrent.pieceLength = infoDict.at("piece length").integerVal;

    const string& piecesRaw = infoDict.at("pieces").stringVal;
    if (piecesRaw.length() % 20 != 0) {
        throw runtime_error("Invalid pieces length: must be a multiple of 20 bytes");
    }

    for (size_t offset = 0; offset < piecesRaw.length(); offset += 20) {
        string chunk = piecesRaw.substr(offset, 20);
        vector<uint8_t> hashBytes(chunk.begin(), chunk.end());
        torrent.pieceHashes.push_back(hashBytes);
    }

    if (infoDict.count("length")) {
        // Single-file mode
        torrent.totalLength = infoDict.at("length").integerVal;
    } 
    else if (infoDict.count("files")) {
        // Multi-file mode: sum length of all files in list
        int64_t total = 0;
        for (const auto& fileItem : infoDict.at("files").listVal) {
            total += fileItem.dictVal.at("length").integerVal;
        }   
        torrent.totalLength = total;
    } 
    else {
        throw runtime_error("Torrent file missing size specification ('length' or 'files')");
    }

    return torrent;
}