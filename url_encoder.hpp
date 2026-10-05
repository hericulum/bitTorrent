#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <cctype>
using namespace std;

inline string urlEncodeBytes(const vector<uint8_t>& bytes) {
    string encoded = "";

    for (uint8_t b : bytes) {
        // 1. Check if 'b' is an unreserved (safe) character
        if (isalnum(b) || b == '-' || b == '_' || b == '.' || b == '~') 
            encoded += char(b);
        
        else {
            // 2. Otherwise, convert byte to %XX hex format
            encoded += '%';
            stringstream ss;
            ss << hex << uppercase << setw(2) << setfill('0') << static_cast<int>(b);
            encoded += ss.str();    
        }
    }

    return encoded;
}