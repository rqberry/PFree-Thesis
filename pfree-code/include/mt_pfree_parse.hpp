
#include <memory>
#include <string>

#include "../include/pfree.hpp"

#pragma once


// for ACGT alphabet only!
int normalize_char(int c) {
    if (c == 65) return 0;
    if (c == 67) return 1;
    if (c == 71) return 2;
    if (c == 84) return 3;
    return -1; // necessary for special characters
}

// -----------------------------------------------------------------
// class to maintain a window in a string and its KR fingerprint
struct KR_window {
    int wsize;
    int *window;

    //int anorm_min_c = 65; // smallest expected normal character
    //int anorm_max_c = 84; // largest expected normal character
    //int anorm_min; // these represent the expected min and max "rank"
    //int anorm_max; // of a window length string i.e. "AAA" -> "ZZZ" 
    int64_t rank; // for domaining
    int rsize;
    uint64_t rsize_max; // rsize^(wsize-1) maximum of a single character position
    //uint64_t rmin;  // AAA...
    uint64_t rmax;  // TTT...

    int asize;
    //int rank; // this is to determine which thread domain this phrase belongs to
    const uint64_t prime = 1999999973;
    uint64_t hash;
    uint64_t tot_char;
    uint64_t asize_pot;   // asize^(wsize-1) mod prime 

    KR_window(int w): wsize(w) {

        asize = 256;
        rsize = 4;

        asize_pot=rsize_max=1;
        rmax=rsize-1;
        
        for(int i = 1; i < wsize; i++) {
            rsize_max *= rsize;
            rmax = rmax * rsize + (rsize - 1);
            asize_pot = (asize_pot * asize) % prime; // ugly linear-time power algorithm  
        }

        //rmax = (rsize_max * rsize - 1) / rsize - 1; // approximate geometric series -> maximum string rank

        // alloc and clear window
        window = new int[wsize];
        reset();     
    }

    // init window, hash, and tot_char 
    void reset() {
        for(int i=0;i<wsize;i++) window[i]=0;
        // init hash value and related values
        hash=tot_char=rank=0;    
    }

    uint64_t addchar(int c) {
        int k = tot_char++ % wsize;

        rank = (rank - (window[k] * rsize_max)) * rsize + normalize_char(c);
        
        // complex expression to avoid negative numbers 
        hash += (prime - (window[k]*asize_pot) % prime); // remove window[k] contribution  
        hash = (asize*hash + c) % prime;      //  add char i 
        window[k]=c;
        // cerr << get_window() << " ~~ " << window << " --> " << hash << endl;
        return hash; 
    }

    size_t get_domain(int th) {


        for (int id = 0; id < th - 1; id++) {


            if (rank < (int64_t)((id + 1) * ((rmax + 1) / th))) return id; 
        }
        return th - 1;
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
/*
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
*/

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

uint64_t kr_window_hash64(const std::string s) {

    const uint64_t prime = 1999999973; 
    uint64_t hash = 0;

    for(uint64_t k = 0; k < s.size(); ++k) {
        //rank = (rank - (window[k] * rsize_max)) * rsize + normalize_char(c);
        hash = (256*hash + s[k]) % prime;      //  add char i 
        // cerr << get_window() << " ~~ " << window << " --> " << hash << endl;
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

struct Thread {
    size_t id;
    int start;
    int stop;

    //size_t saw = 0;         // for debugging
    //size_t sent = 0;        // for debugging
    //size_t recieved = 0;        // for debugging
    //std::vector<std::map<size_t,phrase_entry>> *freqs;
    std::unordered_map<size_t,phrase_entry> *freq; // make this unordered
    std::vector<std::vector<std::string>> *mail;
    std::unordered_map<uint64_t,size_t> *E;
};

