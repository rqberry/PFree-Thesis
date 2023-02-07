#include <stdint.h>
#include <vector>
#include <cassert>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <memory>

#pragma once

typedef char char_t;
typedef std::vector<char_t> string_t;

typedef size_t uint64_t; 

struct string_t_Hash {
    // not my code.
    // https://stackoverflow.com/questions/29855908/c-unordered-set-of-vectors
    /*
    size_t operator()(const std::vector<char_t>& v) const {
        std::hash<int> hasher;
        size_t seed = 0;
        for (int i : v) {
            seed ^= hasher(i) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }
        return seed;
    }*/
    
    // not my code.
    // https://github.com/alshai/Big-BWT
    // compute 64-bit KR hash of a string 
    // to avoid overflows in 64 bit aritmethic the prime is taken < 2**55
    // if collisions occur use a prime close to 2**63 and 128 bit variables 
    size_t operator()(const string_t& s) const {
        size_t hash = 0;
        //const uint64_t prime = 3355443229;     // next prime(2**31+2**30+2**27)
        const size_t prime = 27162335252586509; // next prime (2**54 + 2**53 + 2**47 + 2**13)
        for(size_t k = 0; k < s.size(); k++) {
            char_t c = s[k];
            assert(c >= 0 && c < 256);
            hash = (256*hash + c) % prime;    //  add char k
        } 
        return hash; 
    }
};


// O(N)
bool operator==(string_t s1, string_t s2) {
    if (s1.size() != s2.size()) return false;
    for (size_t i = 0; i < s1.size(); ++i) if (s1[i] != s2[i]) return false;
    return true;
}

// O(N)
bool operator!=(string_t s1, string_t s2) {
    return !(s1 == s2);
}

// O(N)
bool operator<(string_t s1, string_t s2) {
    for (size_t i = 0; i < s1.size(); ++i) {
        if (s1[i] < s2[i]) {
            return true;
        } else if (s1[i] > s2[i]) {
            return false;
        }
    }
    return false;
}

typedef struct DictEntry {
    string_t phrase;
    uint16_t occ;
    uint32_t rank = 0;
} entry_t;

typedef std::shared_ptr<entry_t> entry_ptr;

namespace Settings {
    inline const char_t START_CHAR = (char_t)'#';
    inline const char_t END_CHAR = (char_t)'$';
    inline const std::string OUTPUT_PATH = "./output/";
    
}

namespace Data {
    // The input T
    inline string_t input;
    // The set E
    inline std::unordered_set<string_t,string_t_Hash> delims; 
    // The dictionary D
    inline std::unordered_map<size_t,entry_t> dict;
    // The size of P
    inline size_t P = 0;
}

struct Args {
    bool helper_mode = false;
    bool debug_mode = false;
    bool parse_only = false;
    std::string input_path = "";
    // not working, need output path to be const for std::ofstream
    //std::string output_path = "./output/parse.txt";
    size_t w = 6;
    char_t p = 20;
};
