
#include <iostream> // for cout
#include <ctime> // for time_t
#include <fstream> // for ifstream
#include <algorithm> // for sort for unique
#include <latch> // for latch
#include <thread> // for thread 

#include <string_view> // also join?
#include <ranges> // for join

#include "../include/mt_pfree_parse.hpp" // for bwt_flags
#include "../include/pfree.hpp" 

//#define NUM_THREADS 4

std::atomic_size_t latch_atomic;


// helper function, updates freq data structure and checks for errors
void mt_merge_freq(std::unordered_map<size_t, phrase_entry> *freq_table1, std::unordered_map<size_t, phrase_entry> *freq_table2) {
    
    for (std::pair<size_t, phrase_entry> phrase : *freq_table2) {

        // this is the hash of the complete phrase, this is what is written to the parse
        if (freq_table1->find(phrase.first) == freq_table1->end()) { // new phrase

            freq_table1->emplace(phrase);
            freq_table2->erase(phrase.first);
        
        } else { // known phrase

            freq_table1->at(phrase.first).occ += phrase.second.occ;
        
            if (freq_table1->at(phrase.first).occ <= 0) {
                std::cerr << "(!) parse_debug : Max occurances reached in merge.\n";
                exit(1);
            }
            if (freq_table1->at(phrase.first).p != phrase.second.p) {
                std::cerr << "(!) parse_debug : Hash collision in merge.\n";// << (*freq)[hash].p << "\n" << phrase << "\n";
                exit(1);    
            }
        }
    }
}

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
/*
void mt_write_dict_occ(struct Thread *t, const std::string output_path, std::vector<const std::string *> sorted_dict) {

    assert(sorted_dict.size() == t->freq->size());

    // open output files for writing
    std::ofstream dict_out(output_path + "." + std::to_string(t->id) + ".dict", std::ofstream::out | std::fstream::binary);
    std::ofstream occ_out(output_path + "." + std::to_string(t->id) +  ".occ", std::ofstream::out | std::fstream::binary);

    uint32_t rank = 1;
    for (auto p: sorted_dict) {
        // determine data
        const char *phrase = (*p).data();
        [[maybe_unused]] size_t len = (*p).size();
        uint64_t hash = kr_hash64(*p);
        // write to dict
        dict_out << phrase << '\n';
        //dict_out.write(phrase, len);
        //dict_out.write("\n",1);
        // write to occ
        //occ_out.write(reinterpret_cast<const char *>(&(t->freq->at(hash).occ)), sizeof(uint32_t));
        occ_out << t->freq->at(hash).occ;
        // update rank
        t->freq->at(hash).rank = rank++;
    }

    // flush and close output files
    dict_out.flush();
    dict_out.close();
    occ_out.flush();
    occ_out.close();
}*/

// overwrite the parse with the ranks of the strings
void mt_overwrite_parse(struct Thread *t, const std::vector<std::unordered_map<size_t,phrase_entry> *> freqs,const std::string output_path) {

    // open old parse file for reading and new parse file for output
    std::ifstream parse_in(output_path + "." + std::to_string(t->id) + ".parse_old", std::ofstream::in | std::fstream::binary);
    std::ofstream parse_out(output_path + "." + std::to_string(t->id) + ".parse", std::ofstream::out | std::fstream::binary);

    uint64_t hash;
    uint32_t rank;
    while (parse_in.read(reinterpret_cast<char *>(&hash), sizeof(hash))) {

        bool foundHash = false;
        size_t i = 0;
        while(i < freqs.size() && !foundHash) {
        
            if (freqs[i]->find(hash) != freqs[i]->end()) {
                rank = freqs[i]->at(hash).rank;
                foundHash = true;
            }

            i++;
        }
        assert(foundHash);
        parse_out.write(reinterpret_cast<char *>(&rank), sizeof(rank));
    }

    parse_out.flush();
    parse_out.close();
    parse_in.close();
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
    size_t window_id = 0; 

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
        window_id = t->E->at(window.hash);
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
            
            if (t->id == window_id) { // phrase belongs to me

                phrase_hash = mt_update_freq((*(t->mail))[t->id + (NUM_THREADS * t->id)], phrase); 
                
                //parse_old << "<<<" << phrase << '\n';

            } else { // phrase belongs to another thread

                phrase_hash = mt_update_freq((*(t->mail))[t->id + (NUM_THREADS * window_id)], phrase); 

                //parse_old << ">>>(" << window_id << ")>>>" << phrase << '\n';

                //phrase_hash = kr_hash64(phrase);
                //(*(t->mail))[t->id + (NUM_THREADS * window_id)].push_back(phrase_start); // send mail from me to other thread
                //(*(t->mail))[window_id][t->id].push_back(phrase_start); // send mail from me to other thread
                
            }

            // always write what you see to the parse
            parse_old.write(reinterpret_cast<const char *>(&phrase_hash),sizeof(phrase_hash));

            phrase_start += phrase.size() - w;
            phrase.erase(0,phrase.size() - w); // keep only the last w chars 
            window_id = t->E->at(window.hash); // update phrase id to be new start window
            
            if (t->start + skipped + parsed >= t->stop + w) {
                parse_old.close(); 
                return;
            }
        }
    } 



    if (t->id == NUM_THREADS - 1) {

        uint64_t phrase_hash;

        phrase.append(w,'$');
        if (t->id == window_id) { // phrase belongs to me

            // only update freq table if its mine
            phrase_hash = mt_update_freq((*(t->mail))[t->id + (NUM_THREADS * t->id)], phrase); 
            
            //parse_old << phrase_start << "<<<" << phrase << '\n';
            

        } else { // phrase belongs to another thread

            //parse_old << phrase_start << ">>>" << phrase << '\n';
            //(*(t->mail))[t->id + (NUM_THREADS * window_id)].push_back(phrase); // send mail from me to other thread
            //phrase_hash = kr_hash64(phrase);
            //(*(t->mail))[t->id + (NUM_THREADS * window_id)].push_back(phrase_start); // send mail from me to other thread
            phrase_hash = mt_update_freq((*(t->mail))[t->id + (NUM_THREADS * window_id)], phrase); 

            //parse_old << ">>>(" << window_id << ")>>>" << phrase << '\n';
            //(*(t->mail))[window_id][t->id].push_back(phrase_start); // send mail from me to other thread
        }

        parse_old.write(reinterpret_cast<const char *>(&phrase_hash),sizeof(phrase_hash));
        //parse_old << phrase.substr(w,phrase.size());
        parse_old.close(); 
        return;
    }
}

//
// Phase II of the algorithm, recieve mail and write dict
//
void mt_write_dict(struct Thread *t, 
                   [[maybe_unused]] const std::string input_path, 
                   [[maybe_unused]] const std::string output_path, 
                   [[maybe_unused]] const size_t w, 
                   [[maybe_unused]] const size_t p,
                   const size_t NUM_THREADS) {

    /*
    std::ifstream fin;
	fin.open(input_path, std::ifstream::in);
	while (fin.fail()) {
		std::cerr << "(!) parse_debug : could not open input file \n";
        exit(1);
	}
    */
    std::unordered_map<size_t,phrase_entry> *freq_table1 = (*(t->mail))[t->id + (NUM_THREADS * t->id)];
    //std::ofstream mail(output_path + "." + std::to_string(t->id) + ".mail", std::ofstream::out | std::ios::binary);

    for (size_t from = 0; from < NUM_THREADS; from++) {

        if (from != t->id) {
            //mt_merge_freq(t, (*(t->mail))[t->id + (NUM_THREADS * t->id)], (*(t->mail))[from + (NUM_THREADS * t->id)], msg);

            std::unordered_map<size_t,phrase_entry> *freq_table2 = (*(t->mail))[from + (NUM_THREADS * t->id)];

            while (freq_table2->size() > 0) {

                std::pair<size_t,phrase_entry> phrase = *(freq_table2->begin());

                //mail << "<<<(" << from << ")<<<" << phrase.second.p << " /" << freq_table2->size() << '\n';

                // this is the hash of the complete phrase, this is what is written to the parse
                if (freq_table1->find(phrase.first) == freq_table1->end()) { // new phrase

                    freq_table1->emplace(phrase);
                
                } else { // known phrase

                    freq_table1->at(phrase.first).occ += phrase.second.occ;
                
                    if (freq_table1->at(phrase.first).occ <= 0) {
                        std::cerr << "(!) parse_debug : Max occurances reached in merge.\n";
                        exit(1);
                    }
                    if (freq_table1->at(phrase.first).p != phrase.second.p) {
                        std::cerr << "(!) parse_debug : Hash collision in merge.\n";// << (*freq)[hash].p << "\n" << phrase << "\n";
                        exit(1);    
                    }
                }

                freq_table2->erase(phrase.first);
            }
        }

        /*
        for (uint64_t phrase_start : (*(t->mail))[from + (NUM_THREADS * t->id)]) {

            // init phrase
            //std::string phrase("");
            // init window
            //KR_window window((int)w);
            // seek to start of phrase
            //mail << phrase_start << '\n';
            fin.seekg(phrase_start - fin.tellg(),std::ios::cur);

            // find two window instances or end of file.
            
            [[maybe_unused]] int c;
            c = fin.get();

            // Garunteed to have at least a window
            for (size_t i = 0; i < w; i++) {
                c = fin.get();
                //phrase.append(1,c);
                //window.addchar(c);
            }

            //phrase.erase();
            bool phraseTerminated = false;
            // go until next phrase
            while((c = fin.get()) != EOF && !phraseTerminated) {

                phrase.append(1,c);
                window.addchar(c);

                if (window.hash % p == 0) {
                    mt_update_freq(t, phrase);
                    phraseTerminated = true;
                }
            }

            // must be last phrase
            if (!phraseTerminated) {
                phrase.append(w,'$');
                mt_update_freq(t, phrase);
            }
            
        }
        */
    }

    //fin.close();
    

    uint64_t num_phrases = freq_table1->size();
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
    for (auto phrase : *freq_table1) {
        //mail << phrase.second.p << std::endl;
        dict.push_back(phrase.first);
    }
    std::sort(dict.begin(), dict.end(), 
        [freq_table1](const uint64_t p1, const uint64_t p2) -> bool { 
            return freq_table1->find(p1)->second.p <= freq_table1->find(p2)->second.p; 
        });

    //mail.close();
    mt_write_dict_occ(t, output_path, dict);
}


// Theta (w)
// all possible w-length strings
void buildDelims(uint64_t hash, int k, const size_t p, std::vector<uint64_t> *delims) {
    if (k==0) {
        if (hash % p == 0) delims->push_back(hash);
    } else {
        buildDelims((256*hash + (int)'A') % 1999999973, k-1, p, delims);
        buildDelims((256*hash + (int)'C') % 1999999973, k-1, p, delims);
        buildDelims((256*hash + (int)'G') % 1999999973, k-1, p, delims);
        buildDelims((256*hash + (int)'T') % 1999999973, k-1, p, delims);
    }
}

// Theta (w + |E|) 
//
std::unordered_map<uint64_t,size_t> buildEMap(const size_t w, const size_t p, const size_t NUM_THREADS) {
    std::vector<uint64_t> delims;
    std::unordered_map<uint64_t,size_t> E;
    buildDelims(0, w, p, &delims);
    size_t domain = delims.size() / NUM_THREADS;
    size_t i = 0; 
    for (uint64_t h : delims) { // evenly assign thread work
        size_t id = i / domain;
        if (id > NUM_THREADS - 1) id = NUM_THREADS - 1; // solves rounding issue // maybe a more clever way
        E.insert(std::pair(h,id));
        i++;
    }
    return E;
}

void mt_parse(const std::string input_path, 
              const std::string output_path, 
              const size_t w, 
              const size_t p, 
              const size_t NUM_THREADS) {

    //std::cout << " ==== Building E" << std::endl;

    std::unordered_map<uint64_t,size_t> E = buildEMap(w, p, NUM_THREADS);

    std::vector<Thread *> structs;
    std::vector<std::thread> threads;
    //std::vector<std::vector<size_t>> mail(NUM_THREADS * NUM_THREADS); // i -> j  :=  i + (th * j)
    std::vector<std::unordered_map<size_t,phrase_entry> *> mail(NUM_THREADS * NUM_THREADS); // i -> j  :=  i + (th * j)
    //std::vector<std::vector<std::vector<size_t>>> mail(NUM_THREADS, std::vector<std::vector<size_t>>(NUM_THREADS)); // i -> j  :=  i + (th * j)
    //std::vector<std::unordered_map<size_t,phrase_entry> *> freqs;
    std::latch sent{(std::ptrdiff_t)NUM_THREADS};
    std::latch recieved{(std::ptrdiff_t)NUM_THREADS};


    for (size_t i = 0; i < NUM_THREADS * NUM_THREADS; i++) mail[i] = new std::unordered_map<size_t,phrase_entry>;

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
        t->mail = &mail;
        t->E = &E;
        
        // example for adding a phrase
        //struct phrase_entry ph;
        //(*t.freqs)[id].p = "phrase";
        structs.push_back(t);
        
        threads.push_back(std::thread([t, &sent, &recieved, input_path, output_path, w, p, NUM_THREADS](){

            //send(id, NUM_THREADS, start, stop, mail);

            mt_write_parse(t, input_path, output_path, w, p, NUM_THREADS);
            //std::cout << t->id << std::endl;
            
            sent.count_down();
            sent.wait();
            //std::cout << t->id << std::endl;

            mt_write_dict(t, input_path, output_path, w, p, NUM_THREADS);

            //recieved.count_down();
            //recieved.wait();

            //mt_overwrite_parse(t, freqs, output_path);
        }));
    }

    /*
    std::cout << "<\n";
    for (size_t id = 0; id < NUM_THREADS; id++) {
        std::cout << "\t[";
        for (auto ph : (*(freqs[id]))) std::cout << ph.first << ":" << ph.second.p << ", ";
        std::cout << "]\n";
    }
    std::cout << ">" << std::endl;
    std::cout << "[\n";
    for (std::vector<std::string> v : mail) {
        std::cout << "\t[";
        for (std::string ph : v) {
            std::cout << ph << ", ";
        }
        std::cout << "]\n";
    }
    std::cout << "]" << std::endl;
    */

    //sent.wait();

    std::for_each(threads.begin(), threads.end(), [](std::thread &th){th.join();});

    std::cout << " ==== Threads returned" << std::endl;

    
    /*
    for (size_t i = 0; i < NUM_THREADS; i ++) {
        std::cout << "[ ";
        for (size_t j = 0; j < NUM_THREADS; j ++) {
            std::cout << mail[i + NUM_THREADS * j].size() << " ";
        }
        std:: cout << "]\n";
    }
    */

    for (std::unordered_map<size_t,phrase_entry> *freq_table : mail) free(freq_table);
    for (Thread * t : structs) free(t);

}

int main(int argc, char **argv) {
    assert(argc == 9);
    mt_parse(argv[1], argv[2], atoi(argv[4]), atoi(argv[6]), atoi(argv[8]));
    return 0;
}