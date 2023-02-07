
#include <memory>

#include "../include/pfree.hpp"
#include "../include/exceptions.hpp"


// prepend and append start and termination characters
// O(w)
void pad_input(string_t &T, const size_t w) {

    T.emplace(T.begin(), Settings::START_CHAR);

    for (size_t i = 0; i < w; ++i) T.push_back(Settings::END_CHAR);
}


// karp-rabin hash over text using window size w, 
// when hash % p = 0, add to E 
// O(N)
void gen_delims(const string_t T, const size_t w, const size_t p) {

    assert(w < T.size());
 
    char_t w_sum = 0;

    // calculate initial sum (excluding last character)
    for (size_t i = 1; i < w - 1; ++i) w_sum += T[i]; 

    for (size_t i = 0; i + w < Data::input.size(); ++i) {

        w_sum += T[i + w - 1];

        // if hash is 0, add slice to E
        if (w_sum * (2 ^ (8 * sizeof(char_t))) % p == 0) {
            string_t e(T.begin() + i, T.begin() + i + w);
            Data::delims.emplace(e);
        }

        w_sum -= T[i];
    }

    string_t start = {Settings::START_CHAR};
    string_t end;
    for (size_t i = 0; i < w; ++i) end.push_back(Settings::END_CHAR);
    Data::delims.emplace(start);
    Data::delims.emplace(end);
}



// not my code !!! ~ sorta
uint64_t kr_hash64(const string_t& s) {
    uint64_t hash = 0;
    const uint64_t prime = 27162335252586509; // next prime (2**54 + 2**53 + 2**47 + 2**13)
    for(uint64_t k = 0; k < s.size(); ++k) {
        char_t c = s[k];
        assert(c >= 0 && c < 256);
        hash = (256*hash + c) % prime;    //  add char k
    } 
    return hash; 
}

// not my code !!! ~ sorta
uint32_t kr_hash32(const string_t& s) {
    uint32_t hash = 0;
    const uint32_t prime = 3355443229;     // next prime(2**31+2**30+2**27)
    for(uint32_t k = 0; k < s.size(); ++k) {
        char_t c = s[k];
        assert(c >= 0 && c < 256);
        hash = (256*hash + c) % prime;    //  add char k
    } 
    return hash; 
}

// O(N)
// primary workhorse of the code
void prefix_free_parse(const string_t T, const size_t w) {

    assert(w < T.size());

    string_t::const_iterator entry_start = T.begin();
    string_t::const_iterator window_end = T.begin() + w;

    std::ofstream parse_out(Settings::OUTPUT_PATH + "tmp_parse.dat", std::ofstream::out);

    // calculate initial sum (excluding last character)
    for (string_t::const_iterator window_start = entry_start; 
         window_end != T.cend(); 
         ++window_end, ++window_start) {
        
        string_t window(window_start, window_end);
        
        if (Data::delims.find(window) != Data::delims.end()) {
            string_t entry(entry_start, window_end);

            uint64_t hash = kr_hash64(entry);
            if (Data::dict.find(hash) != Data::dict.end()) {
                assert(Data::dict.find(hash)->second.phrase == entry);
                Data::dict.find(hash)->second.occ++;
            } else {
                entry_t dict_entry;
                dict_entry.phrase = entry;
                dict_entry.occ = 1;
                Data::dict.emplace(hash,dict_entry);
            }
            parse_out << hash << '\n';
            Data::P += 1;

            entry_start = window_start;
        }
    }

    // last entry
    string_t entry(entry_start, T.cend());
    uint64_t hash = kr_hash64(entry);
    entry_t dict_entry;
    dict_entry.phrase = entry;
    dict_entry.occ = 1;
    Data::dict.emplace(hash,dict_entry);
    
    parse_out << hash << std::endl; // should flush the buffer
    Data::P += 1;
    
    parse_out.close();
}

// O(N)
// same as above but with binary output file
void prefix_free_parse_bin(const string_t T, const size_t w) {

    assert(w < T.size());

    string_t::const_iterator entry_start = T.begin();
    string_t::const_iterator window_end = T.begin() + w;
    
    std::ofstream parse_out(Settings::OUTPUT_PATH + "tmp_parse.bin", std::ofstream::out | std::ios::binary);
    
    // calculate initial sum (excluding last character)
    for (string_t::const_iterator window_start = entry_start; 
         window_end != T.cend(); 
         ++window_end, ++window_start) {
        
        string_t window(window_start, window_end);
        
        if (Data::delims.find(window) != Data::delims.end()) {
            string_t entry(entry_start, window_end);

            uint64_t hash = kr_hash64(entry);
            if (Data::dict.find(hash) != Data::dict.end()) {
                assert(Data::dict.find(hash)->second.phrase == entry);
                Data::dict.find(hash)->second.occ++;
            } else {
                entry_t dict_entry;
                dict_entry.phrase = entry;
                dict_entry.occ = 1;
                Data::dict.emplace(hash,dict_entry);
            }

            parse_out.write(reinterpret_cast<const char *>(&hash),sizeof(hash));
            Data::P += 1;

            entry_start = window_start;
        }
    }

    // last entry
    string_t entry(entry_start, T.cend());
    uint64_t hash = kr_hash64(entry);
    entry_t dict_entry;
    dict_entry.phrase = entry;
    dict_entry.occ = 1;
    Data::dict.emplace(hash,dict_entry);

    parse_out.write(reinterpret_cast<const char *>(&hash),sizeof(hash));
    Data::P += 1;

    parse_out.flush();
    parse_out.close();
}
