
#include <string> // for std::string
#include <iostream> // for cout
#include <ctime> // for time_t
#include <fstream> // for ifstream
#include <algorithm> // for sort

#include "../include/pfree_parse.hpp" // for bwt_flags
#include "../include/pfree.hpp" 

// helper function, updates freq data structure and checks for errors
uint64_t update_freq(std::string phrase, std::map<size_t,phrase_entry>& freq) {
    
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

// for debugging only, writes parse file as readable text
void write_parse_debug(const std::string input_path, const std::string output_path, const size_t w, const size_t p) {

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

    std::ofstream parse_old(output_path + ".parse_old", std::ofstream::out);

    // init phrase
    std::string phrase("#");

    int c;
    while( (c = fin.get()) != EOF ) {

        phrase.append(1,c);
        hash = window.addchar(c);

        if (hash % p == 0) {
            parse_old << phrase << '\n';
            phrase = phrase.substr(phrase.size() - w);
        }        

    }

    phrase.append((int)w,(int)'$'); // fix this in practice (use better stopper like 0x02)

    // write phrase
    parse_old << phrase << '\n';

    parse_old.close();
    fin.close();
}

// write parse file and fill in freq data structure used to build dict
void write_parse(const std::string input_path, const std::string output_path, std::map<size_t,phrase_entry>& freq, const size_t w, const size_t p) {

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

    phrase.append((int)w,(int)'$'); // fix this in practice (use better stopper like 0x02)

    // write phrase
    uint64_t phrase_hash = update_freq(phrase, freq);
    parse_old.write(reinterpret_cast<const char *>(&phrase_hash),sizeof(phrase_hash));

    parse_old.close();
    fin.close();
}

// write data from sorted dictionary and frequency table to 
// output files with .dict and .occ extensions
void write_dict_occ(const std::string output_path, std::vector<const std::string *> sorted_dict, std::map<size_t,phrase_entry> freq) {

    assert(sorted_dict.size() == freq.size());

    // open output files for writing
    std::ofstream dict_out(output_path + ".dict", std::ofstream::out | std::fstream::binary);
    std::ofstream occ_out(output_path + ".occ", std::ofstream::out | std::fstream::binary);

    uint32_t rank = 1;
    for (auto p: sorted_dict) {
        // determine data
        const char *phrase = (*p).data();
        size_t len = (*p).size();
        uint64_t hash = kr_hash64(*p);
        // write to dict
        dict_out.write(phrase, len);
        dict_out.write("\n",1);
        // write to occ
        std::string p_occ = std::to_string(freq.at(hash).occ);
        occ_out.write(p_occ.c_str(), p_occ.size());
        // update rank
        freq.at(hash).rank = rank++;
    }

    // flush and close output files
    dict_out.flush();
    dict_out.close();
    occ_out.flush();
    occ_out.close();
}

void overwrite_parse(const std::string output_path, std::map<size_t,phrase_entry> freq) {

    // open old parse file for reading and new parse file for output
    std::ifstream parse_in(output_path + ".parse_old", std::ofstream::in | std::fstream::binary);
    std::ofstream parse_out(output_path + ".parse", std::ofstream::out | std::fstream::binary);

    uint64_t hash;
    for (uint32_t i = 0; i < freq.size(); ++i) {
        parse_in.read(reinterpret_cast<char *>(&hash), sizeof(hash));
        uint32_t rank = freq.at(hash).rank;
        parse_out.write(reinterpret_cast<char *>(&rank), sizeof(rank));
    }

    parse_out.flush();
    parse_out.close();
    parse_in.close();
}

int main(int argc, char **argv) {

    assert(argc == 7); // not safe, just assume inputs from pfree

    int w = std::atol(argv[4]);
    int p = std::atol(argv[6]);
    std::cout << "Windows size: " << w << std::endl;
    std::cout << "Stop word modulus: " << p << std::endl;  

    const char *input_path = argv[1];
    const char *output_path = argv[2];

    // measure elapsed wall clock time
    time_t start_main = time(NULL);
    time_t start_phase = start_main;  

    // ------------ parsing input file 
    std::map<size_t,phrase_entry> freq;

    write_parse(input_path, output_path, freq, w, p);
    uint64_t num_phrases = freq.size();

    // logging messages from original Big-BWT-master repo
    std::cout << "Found " << num_phrases << " distinct phrases" << std::endl;
    std::cout << "Parsing took: " << difftime(time(NULL),start_phase) << " wall clock seconds\n";  

    if (num_phrases > (INT32_MAX - 1)) {
        std::cerr << "Emergency exit! The number of distinc phrases (" << num_phrases << ")\n";
        std::cerr << "is larger than the current limit (" << (INT32_MAX - 1) << ")\n";
        exit(1);
    }

    // -------------- second pass  
    start_phase = time(NULL);
    // create dictionary
    std::vector<const std::string *> dict;
    dict.reserve(num_phrases);

    // fill dictionary using freq
    // time and memory consuming !!!
    for (auto& p: freq) dict.push_back(&p.second.p);

    std::sort(dict.begin(), dict.end(), 
        [](const std::string *p1, const std::string *p2) -> bool { 
            return *p1 <= *p2; 
        });

    std::cout << "Writing plain dictionary and occ file\n";
    write_dict_occ(output_path, dict, freq);
    dict.clear(); // free memory
    std::cout << "Dictionary construction took: " << difftime(time(NULL),start_phase) << " wall clock seconds\n"; 

    start_phase = time(NULL);
    std::cout << "Generating remapped parse file\n";
    overwrite_parse(output_path, freq);
    std::cout << "Remapping parse file took: " << difftime(time(NULL),start_phase) << " wall clock seconds\n";  
    std::cout << "==== Elapsed time: " << difftime(time(NULL),start_main) << " wall clock seconds\n";    
    return 0;
}