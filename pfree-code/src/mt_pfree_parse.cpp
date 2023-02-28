
#include <iostream> // for cout
#include <ctime> // for time_t
#include <fstream> // for ifstream
#include <algorithm> // for sort
#include <thread>

#include "../include/pfree_parse.hpp" // for bwt_flags
#include "../include/pfree.hpp" 

// helper function, updates freq data structure and checks for errors
/*
uint64_t mt_update_freq(std::string phrase, std::map<size_t,phrase_entry>& freq) {
    
    // this is the hash of the complete phrase, this is what is written to the parse
    uint64_t hash = kr_hash64(phrase);

    if (freq.find(hash) == freq.end()) { // new phrase

        freq[hash].occ = 1;
        freq[hash].p = phrase;

    } else { // known phrase

        freq[hash].occ++;

        if (freq[hash].occ <= 0) {
            std::cerr << "(!) parse_debug : Max occurances reached.\n";
            exit(1);
        }
        if (freq[hash].p != phrase) {
            std::cerr << "(!) parse_debug : Hash collision.\n";
            exit(1);    
        }
    }

    return hash;
}
*/

// write parse file and fill in freq data structure used to build dict
/*
void mt_write_parse(const std::string input_path, const std::string output_path, std::map<size_t,phrase_entry>& freq, const size_t w, const size_t p) {

    std::ifstream fin;
	fin.open(input_path, std::ifstream::in);
	while (fin.fail()) {
		std::cerr << "(!) parse_debug : could not open input file \n";
        exit(1);
	}

    // init window
    KR_window window((int)w);
    window.reset();
    uint64_t hash = 0; // this is the hash of the window, note this is different than the hash of the entire phrase which gets written to the parse

    std::ofstream parse_old(output_path + ".parse_old", std::ofstream::out | std::ios::binary);

    // init phrase
    std::string phrase("#");

    int c;
    while ((c = fin.get()) != EOF ) {

        phrase.append(1,c);
        hash = window.addchar(c);

        if (hash % p == 0) {
            uint64_t phrase_hash = update_freq(phrase, freq);
            parse_old.write(reinterpret_cast<const char *>(&phrase_hash),sizeof(phrase_hash));
            std::string delim = phrase.substr(phrase.size() - w);
            phrase = delim;
        }        

    }
}
*/

void mt_parse(/*const std::string input_path,*/ const size_t th/*, const size_t w, const size_t p*/) {
    
    std::vector<std::thread> threads;
    for (size_t id = 0; id < th; ++id) {
        threads.push_back(std::thread([id](){std::cout << "th:" << id << std::endl;}));
    }

    std::for_each(threads.begin(), threads.end(), [](std::thread &t){t.join();});
}

int main(int argc, char **argv) {
    assert(argc == 9);
    mt_parse(std::atoi(argv[8]));
    return 0;
}