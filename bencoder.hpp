#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <cctype>

using namespace std;

enum class BencodeType {
    Integer,
    String,
    List,
    Dictionary
};

struct BencodeValue {
    BencodeType type;

    int64_t integerVal{0};
    string stringVal;
    vector<BencodeValue> listVal;
    map<string, BencodeValue> dictVal;
};

inline BencodeValue parseBencode(istream& is) {
    int peekChar = is.peek();
    if (peekChar == EOF) {
        throw runtime_error("Unexpected End of Stream");
    }

    char ch = static_cast<char>(peekChar);

    // Parse Integers: i<integer>e
    if (ch == 'i') {
        is.get(); // consume 'i'
        string numStr = "";

        while (is.peek() != 'e' && is.good()) {
            numStr += static_cast<char>(is.get());
        }

        if (is.get() != 'e') {
            throw runtime_error("Invalid integer format: missing terminating 'e'");
        }

        BencodeValue val;
        val.type = BencodeType::Integer;
        val.integerVal = stoll(numStr);
        return val;
    }

    // Parse Byte Strings: <length>:<contents>
    else if (isdigit(static_cast<unsigned char>(ch))) {
        string stringSize = "";

        while (is.peek() != ':' && is.good()) {
            stringSize += static_cast<char>(is.get());
        }

        if (is.get() != ':') {
            throw runtime_error("Invalid string format: missing ':'");
        }

        size_t len = stoull(stringSize);
        BencodeValue val;
        val.type = BencodeType::String;
        val.stringVal.resize(len);

        if (len > 0) {
            is.read(&val.stringVal[0], len);
            if (static_cast<size_t>(is.gcount()) != len) {
                throw runtime_error("Unexpected EOF while reading bencode string payload");
            }
        }
        return val;
    }

    // Parse Lists: l<values>e
    else if (ch == 'l') {
        is.get(); // consume 'l'
        BencodeValue val;
        val.type = BencodeType::List;

        while (is.peek() != 'e' && is.good()) {
            val.listVal.push_back(parseBencode(is));
        }

        if (is.get() != 'e') {
            throw runtime_error("Invalid list format: missing terminating 'e'");
        }
        return val;
    }

    // Parse Dictionaries: d<key-value pairs>e
    else if (ch == 'd') {
        is.get(); // consume 'd'
        BencodeValue val;
        val.type = BencodeType::Dictionary;

        while (is.peek() != 'e' && is.good()) {
            BencodeValue key = parseBencode(is);

            if (key.type != BencodeType::String) {
                throw runtime_error("Dictionary key must be a string");
            }

            BencodeValue value = parseBencode(is);
            val.dictVal[key.stringVal] = value;
        }

        if (is.get() != 'e') {
            throw runtime_error("Invalid dictionary format: missing terminating 'e'");
        }
        return val;
    }

    throw runtime_error(string("Unexpected character in Bencode stream: ") + ch);
}