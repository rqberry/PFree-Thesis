
#include <iostream> // for cout
#include <ctime> // for time_t
#include <fstream> // for ifstream
#include <algorithm> // for sort for unique
#include <latch> // for latch
#include <thread> // for thread 

#include <string_view> // also join?
#include <ranges> // for join

#include "../include/mt_pfree_parse_bigbwt.hpp" // for bwt_flags
#include "../include/pfree.hpp" 

//#define NUM_THREADS 4

std::atomic_size_t latch_atomic;

// helper function, updates freq data structure and checks for errors
uint64_t mt_update_freq(std::unordered_map<size_t, phrase_entry> *freq_table, std::string phrase) {
    
    
    // this is the hash of the complete phrase, this is what is written to the parse
    uint64_t hash = kr_hash64(phrase);

    if (freq_table->find(hash) == freq_table->end()) { // new phrase
        
        phrase_entry new_phrase;
        new_phrase.occ = 1;
        new_phrase.p = phrase;
        freq_table->emplace(std::pair(hash,new_phrase));
    
    } else { // known phrase

        freq_table->at(hash).occ++;
    
        if (freq_table->at(hash).occ <= 0) {
            std::cerr << "(!) parse_debug : Max occurances reached.\n";
            exit(1);
        }
        if (freq_table->at(hash).p != phrase) {
            std::cerr << "(!) parse_debug : Hash collision.\n";// << (*freq)[hash].p << "\n" << phrase << "\n";
            exit(1);    
        }
    }

    return hash;
}

// write data from sorted dictionary and frequency table to 
// output files with .dict and .occ extensions
void write_dict_occ(std::unordered_map<size_t,phrase_entry> *freq_table, const std::string output_path, std::vector<uint64_t> sorted_dict) {

    // open output files for writing
    std::ofstream dict_out(output_path + ".dict", std::ofstream::out | std::fstream::binary);
    std::ofstream occ_out(output_path + ".occ", std::ofstream::out | std::fstream::binary);

    uint32_t rank = 1;
    for (auto hash: sorted_dict) {

        dict_out.write(freq_table->at(hash).p.data(), freq_table->at(hash).p.size());

        //occ_out << freq_table->at(hash).occ << '\n';

        occ_out.write(reinterpret_cast<const char *>(&(freq_table->at(hash).occ)), sizeof(uint32_t));

        freq_table->at(hash).rank = rank++;
    }

    // flush and close output files
    dict_out.flush();
    dict_out.close();
    occ_out.flush();
    occ_out.close();
}

// overwrite the parse with the ranks of the strings
void mt_overwrite_parse(std::unordered_map<size_t,phrase_entry> *freq_table, const std::string output_path, const size_t NUM_THREADS) {

    // open old parse file for reading and new parse file for output

    std::ofstream parse_out(output_path + ".parse", std::ofstream::out | std::fstream::binary);

    uint64_t hash;
    uint32_t rank;

    for (size_t id = 0; id < NUM_THREADS; id++) {

        std::ifstream parse_in(output_path + "." + std::to_string(id) + ".parse_old", std::ofstream::in | std::fstream::binary);

        while (parse_in.read(reinterpret_cast<char *>(&hash), sizeof(hash))) {

            rank = freq_table->at(hash).rank;
            parse_out.write(reinterpret_cast<char *>(&rank), sizeof(rank));
        }

        parse_in.close();

    }
    parse_out.flush();
    parse_out.close();
}

// write parse file and fill in freq data structure used to build dict
// each thread should start at some point until their window is a phrase
// then progress until their stopping point
// then either add a phrase or mail a phrase based on whether or not that phrase belongs to them
//
// Phase I of the algorithm, send out mail
//
void mt_write_parse(struct Thread *t,
                    const std::string input_path, 
                    const std::string output_path, 
                    const size_t w, 
                    const size_t p,
                    const size_t NUM_THREADS
                    ) {

    std::ifstream fin;
	fin.open(input_path, std::ifstream::in);
	while (fin.fail()) {
		std::cerr << "(!) parse_debug : could not open input file \n";
        exit(1);
	}

    std::ofstream parse_old(output_path + "." + std::to_string(t->id) + ".parse_old", std::ofstream::out | std::ios::binary);
    //std::ofstream parse_old(output_path + "." + std::to_string(t->id) + ".parse_old", std::ofstream::out);

    // init phrase
    std::string phrase("");
    // init window
    KR_window window((int)w);
    // seek to the start
    fin.seekg(t->start);

    int c;

    size_t skipped = 0;
    size_t parsed = 0;

    if (t->id == 0) { 

        phrase.append(1,'#');
        //phrase = phrase.substr(phrase.size() - w);

    } else { // seek to first full phrase
        
        while ((c = fin.get()) != EOF) {

            skipped++;

            if (t->start + skipped == t->stop + w) { 
                parse_old.close(); 
                return;
            }

            phrase.append(1,c);
            window.addchar(c);

            if (window.hash % p == 0 && skipped >= w) break;
        }

        if (c == EOF) {
            parse_old.close(); 
            return;
        }
        parsed = w;
        skipped -= w; // ... so w less chars have been skipped
        phrase.erase(0,phrase.size() - w); // keep only the last w chars 
        //for (auto p : *(t->E)) std::cout << p.first << '\n';
    }

    uint64_t phrase_start = t->start + skipped;
    if (t->id == 0) phrase_start--; // off by 1 error otherwise
    uint64_t pos = t->start; // ending position+1 in text of previous word
    if (t->id != 0) pos += skipped + w;  // or 0 for the first word  
    while ((c = fin.get()) != EOF) {

        
        if (c != (int)'A' && c != (int)'C' && c != (int)'G' && c != (int)'T') {
            std::cerr << " (!) parse_debug : input does not belong to \"ACGT\" only alphabet : " << c << "=\'" << (char)c << "\'" << std::endl;
            exit(1);
        }

        phrase.append(1,c);
        window.addchar(c);
        parsed++;

        if (window.hash % p== 0 && parsed > w) {

            // end of word, save it and write its full hash to the output file
            // pos is the ending position+1 of previous word and is updated in the next call

            uint64_t phrase_hash;
            t->lock->lock();
            phrase_hash = mt_update_freq(t->freq_table, phrase); // send mail to thread the phrase belongs to
            t->lock->unlock();

            // always write what you see to the parse
            parse_old.write(reinterpret_cast<const char *>(&phrase_hash),sizeof(phrase_hash));

            phrase_start += phrase.size() - w;
            phrase.erase(0,phrase.size() - w); // keep only the last w chars 
            
            if (t->start + skipped + parsed >= t->stop + w) {
                parse_old.close(); 
                return;
            }
        }
    } 



    if (t->id == NUM_THREADS - 1) {

        uint64_t phrase_hash;

        phrase.append(w,'$');
        
        // only update freq table if its mine
        t->lock->lock();
        phrase_hash = mt_update_freq(t->freq_table, phrase); // send mail to thread the phrase belongs to
        t->lock->unlock();

        parse_old.write(reinterpret_cast<const char *>(&phrase_hash),sizeof(phrase_hash));
        //parse_old << phrase.substr(w,phrase.size());
        parse_old.close(); 
        return;
    }
}

//
// Phase II of the algorithm, recieve mail and write dict
//
void write_dict(std::unordered_map<uint64_t,phrase_entry> *freq_table, const std::string output_path) {

    uint64_t num_phrases = freq_table->size();
    //mail << "\n\n" << freq_table1->size() << std::endl;
    if (num_phrases > (INT32_MAX - 1)) {
        std::cerr << "Emergency exit! The number of distinc phrases (" << num_phrases << ")\n";
        std::cerr << "is larger than the current limit (" << (INT32_MAX - 1) << ")\n";
        exit(1);
    }

    // --------- second pass to create dictionary ---------
    std::vector<uint64_t> dict; 
    dict.reserve(num_phrases);

    // fill dictionary using freq
    // time and memory consuming !!!
    // std::pair<size_t,phrase_entry>
    for (auto phrase : *freq_table) {
        //mail << phrase.second.p << std::endl;
        dict.push_back(phrase.first);
    }
    std::sort(dict.begin(), dict.end(), 
        [freq_table](const uint64_t p1, const uint64_t p2) -> bool { 
            return freq_table->find(p1)->second.p <= freq_table->find(p2)->second.p; 
        });

    //mail.close();
    write_dict_occ(freq_table, output_path, dict);
}


void mt_parse(const std::string input_path, 
              [[maybe_unused]] const std::string output_path, 
              const size_t w, 
              const size_t p, 
              const size_t NUM_THREADS) {

    std::vector<Thread *> structs;
    std::vector<std::thread> threads;
    //std::vector<std::vector<size_t>> mail(NUM_THREADS * NUM_THREADS); // i -> j  :=  i + (th * j)
    std::unordered_map<size_t,phrase_entry> *freq_table = new std::unordered_map<size_t,phrase_entry>();
    std::mutex *lock = new std::mutex();
    //std::vector<std::vector<std::vector<size_t>>> mail(NUM_THREADS, std::vector<std::vector<size_t>>(NUM_THREADS)); // i -> j  :=  i + (th * j)
    //std::vector<std::unordered_map<size_t,phrase_entry> *> freqs;
    //std::latch sent{(std::ptrdiff_t)NUM_THREADS};
    //std::latch recieved{(std::ptrdiff_t)NUM_THREADS};

    std::ifstream fin;
	fin.open(input_path, std::ifstream::in | std::ifstream::ate);
	while (fin.fail()) {
		std::cerr << "(!) parse_debug : could not open input file: " << input_path << "\n";
        exit(1);
	}
    
    size_t file_size = fin.tellg();
    //std::cout << fin.tellg() << '\n';
    fin.close();

    size_t work_size = file_size / NUM_THREADS;

    //std::cout << " ===== Beginning thread launch" << std::endl;

    for (size_t id = 0; id < NUM_THREADS; id++) {
        
        struct Thread *t = new Thread;

        t->id = id;

        if (id == NUM_THREADS - 1) {
            t->start = work_size * id;
            t->stop = file_size;
        } else {
            t->start = work_size * id;
            t->stop = work_size * (id + 1);
        }

        // GOOD STUFF KEEP THIS
        //t->freq = new std::unordered_map<size_t,phrase_entry>;
        //freqs.push_back(t->freq);
        t->freq_table = freq_table;
        t->lock = lock;
        
        // example for adding a phrase
        //struct phrase_entry ph;
        //(*t.freqs)[id].p = "phrase";
        structs.push_back(t);

        threads.push_back(std::thread([t, input_path, output_path, w, p, NUM_THREADS](){

            mt_write_parse(t, input_path, output_path, w, p, NUM_THREADS);
        }));
    }

    std::for_each(threads.begin(), threads.end(), [](std::thread &th){th.join();});

    std::cout << " ==== Threads returned" << std::endl;

    write_dict(freq_table, output_path);

    delete(freq_table);
    for (Thread * t : structs) free(t);

}

int main(int argc, char **argv) {
    assert(argc == 9);
    mt_parse(argv[1], argv[2], atoi(argv[4]), atoi(argv[6]), atoi(argv[8]));
    return 0;
}