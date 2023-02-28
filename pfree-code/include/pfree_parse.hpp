
#include <memory>
#include <string>

#include "../include/pfree.hpp"

#pragma once


// -----------------------------------------------------------------
// class to maintain a window in a string and its KR fingerprint
struct KR_window {
    int wsize;
    int *window;
    int asize;
    const uint64_t prime = 1999999973;
    uint64_t hash;
    uint64_t tot_char;
    uint64_t asize_pot;   // asize^(wsize-1) mod prime 

    KR_window(int w): wsize(w) {
        asize = 256;
        asize_pot = 1;
        for(int i=1;i<wsize;i++) 
            asize_pot = (asize_pot*asize)% prime; // ugly linear-time power algorithm  
        // alloc and clear window
        window = new int[wsize];
        reset();     
    }

    // init window, hash, and tot_char 
    void reset() {
        for(int i=0;i<wsize;i++) window[i]=0;
        // init hash value and related values
        hash=tot_char=0;    
    }

    uint64_t addchar(int c) {
        int k = tot_char++ % wsize;
        // complex expression to avoid negative numbers 
        hash += (prime - (window[k]*asize_pot) % prime); // remove window[k] contribution  
        hash = (asize*hash + c) % prime;      //  add char i 
        window[k]=c;
        // cerr << get_window() << " ~~ " << window << " --> " << hash << endl;
        return hash; 
    }

    // debug only 
    std::string get_window() {
        std::string w = "";
        int k = (tot_char-1) % wsize;
        for(int i=k+1;i<k+1+wsize;i++)
            w.append(1,window[i%wsize]);
        return w;
    }

    ~KR_window() {
    delete[] window;
    } 

};
// -----------------------------------------------------------


// not my code !!! ~ sorta

uint64_t kr_hash64(const std::string s) {
    uint64_t hash = 0;
    const uint64_t prime = 27162335252586509; // next prime (2**54 + 2**53 + 2**47 + 2**13)
    for(uint64_t k = 0; k < s.size(); ++k) {
        char_t c = s[k];
        assert(c >= 0 && c < 256);
        hash = (256 * hash + c) % prime;    //  add char k
    } 
    return hash; 
}
/*
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
*/

struct phrase_entry {
  std::string p;
  uint32_t occ;
  uint32_t rank = 0;
};




// helper function, updates freq data structure and checks for errors
uint64_t update_freq(std::string phrase, std::map<size_t,phrase_entry>& freq);

// for debugging only, writes parse file as readable text (does not build freq data structure)
void write_parse_debug(const std::string input_path, const std::string output_path, const size_t w, const size_t p);

// actual method for building and writing the parse (writes to binary file with .parse_old extension)
void write_parse(const std::string input_path, const std::string output_path, std::map<size_t,phrase_entry>& freq, const size_t w, const size_t p);

// write data from sorted dictionary and frequency table to output files with .dict and .occ extensions
void write_dict_occ(const std::string output_path, std::vector<const std::string *> sorted_dict, std::map<size_t,phrase_entry>& freq);

// overwrite .parse_old with .parse (replace hash IDs with ranks)
void overwrite_parse(const std::string output_path, std::map<size_t,phrase_entry> freq);

// additional check for parse re-write step
void recompute_occ_check(const std::string output_path, std::map<size_t,phrase_entry> freq);
