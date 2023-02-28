#include <stdint.h>
#include <vector>
#include <cassert>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <utility>
#include <memory>

#pragma once

typedef char char_t;
typedef std::vector<char_t> string_t;

typedef size_t uint64_t; 


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
    std::string phrase;
    uint16_t occ;
    uint32_t rank = 0;
} entry_t;

typedef std::shared_ptr<entry_t> entry_ptr;

namespace Settings {
    inline const char_t START_CHAR = (char_t)'#';
    inline const char_t END_CHAR = (char_t)'$';
    inline const std::string OUTPUT_PATH = "./output/";
}
