
#include <iostream> // for cout
#include <ctime> // for time_t
#include <fstream> // for ifstream
#include <algorithm> // for sort for unique
#include <latch> // for latch
#include <thread> // for thread 

#include "../include/mt_pfree_parse.hpp" // for bwt_flags
#include "../include/pfree.hpp" 

//#define NUM_THREADS 4

std::atomic_size_t latch_atomic;

// helper function, updates freq data structure and checks for errors
uint64_t mt_update_freq(std::string phrase, [[maybe_unused]] std::unordered_map<size_t,phrase_entry> *freq) {
    
    
    // this is the hash of the complete phrase, this is what is written to the parse
    uint64_t hash = kr_hash64(phrase);
    
    if (freq->find(hash) == freq->end()) { // new phrase
        
        phrase_entry new_phrase;
        new_phrase.occ = 1;
        new_phrase.p = phrase;
        freq->emplace(std::pair(hash,new_phrase));
    
    } else { // known phrase

        freq->at(hash).occ++;
    
        if (freq->at(hash).occ <= 0) {
            std::cerr << "(!) parse_debug : Max occurances reached.\n";
            exit(1);
        }
        if (freq->at(hash).p != phrase) {
            std::cerr << "(!) parse_debug : Hash collision.\n";// << (*freq)[hash].p << "\n" << phrase << "\n";
            exit(1);    
        }
    }
    

    return hash;
}


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

// write data from sorted dictionary and frequency table to 
// output files with .dict and .occ extensions
void mt_write_dict_occ(struct Thread *t, const std::string output_path, std::vector<const std::string *> sorted_dict) {

    assert(sorted_dict.size() == t->freq->size());

    // open output files for writing
    std::ofstream dict_out(output_path + ".dict_" + std::to_string(t->id), std::ofstream::out | std::fstream::binary);
    std::ofstream occ_out(output_path + ".occ_" + std::to_string(t->id), std::ofstream::out | std::fstream::binary);

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
        occ_out.write(reinterpret_cast<const char *>(&(t->freq->at(hash).occ)), sizeof(uint32_t));
        // update rank
        t->freq->at(hash).rank = rank++;
    }

    // flush and close output files
    dict_out.flush();
    dict_out.close();
    occ_out.flush();
    occ_out.close();
}

// overwrite the parse with the ranks of the strings
void mt_overwrite_parse(struct Thread *t,const std::string output_path) {

    // open old parse file for reading and new parse file for output
    std::ifstream parse_in(output_path + ".parse_old_" + std::to_string(t->id), std::ofstream::in | std::fstream::binary);
    std::ofstream parse_out(output_path + ".parse_" + std::to_string(t->id), std::ofstream::out | std::fstream::binary);

    uint64_t hash;
    for (uint32_t i = 0; i < t->freq->size(); ++i) {
        parse_in.read(reinterpret_cast<char *>(&hash), sizeof(hash));
        uint32_t rank = t->freq->at(hash).rank;
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

    std::ofstream parse_old(output_path + ".parse_old_" + std::to_string(t->id), std::ofstream::out | std::ios::binary);

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
        window_id = t->E->at(window.hash);
    }


    uint64_t pos = t->start; // ending position+1 in text of previous word
    if (t->id) pos += skipped + w;  // or 0 for the first word  
    while ((c = fin.get()) != EOF) {

        phrase.append(1,c);
        window.addchar(c);
        parsed++;

        if (window.hash % p== 0 && parsed > w) {

            // end of word, save it and write its full hash to the output file
            // pos is the ending position+1 of previous word and is updated in the next call
            
            if (t->id == window_id) { // phrase belongs to me

                uint64_t phrase_hash = mt_update_freq(phrase,t->freq); 
                parse_old.write(reinterpret_cast<const char *>(&phrase_hash),sizeof(phrase_hash));

            } else { // phrase belongs to another thread
                (*(t->mail))[t->id + (NUM_THREADS * window_id)].push_back(phrase); // send mail from me to other thread
            }
            phrase.erase(0,phrase.size() - w); // keep only the last w chars 
            window_id = t->E->at(window.hash); // update phrase id to be new start window
            //save_update_word(*arg,word,*wordFreq,d->parse,d->last,d->sa,pos);
            
            if (t->start + skipped + parsed >= t->stop + w) {
                parse_old.close(); 
                return;
            }
        }
    } 

    if (t->id == NUM_THREADS - 1) {
        phrase.append(w,'$');
        if (t->id == window_id) { // phrase belongs to me

            uint64_t phrase_hash = mt_update_freq(phrase,t->freq); 
            parse_old.write(reinterpret_cast<const char *>(&phrase_hash),sizeof(phrase_hash));

        } else { // phrase belongs to another thread
            (*(t->mail))[t->id + (NUM_THREADS * window_id)].push_back(phrase); // send mail from me to other thread
        }
    }
}

//
// Phase II of the algorithm, recieve mail and write dict
//
void mt_write_dict(struct Thread *t, const std::string output_path, const size_t NUM_THREADS) {
    
    for (size_t from = 0; from < NUM_THREADS; from++) {
        if (from != t->id) { // skip self mail (should never be any)
            std::vector<std::string> *inbox = &((*(t->mail))[from + (NUM_THREADS * t->id)]);

            while (inbox->size() > 0) {
                mt_update_freq(inbox->back(),t->freq);
                inbox->pop_back();
            }
        }   
    }
    
    uint64_t num_phrases = t->freq->size();
    if (num_phrases > (INT32_MAX - 1)) {
        std::cerr << "Emergency exit! The number of distinc phrases (" << num_phrases << ")\n";
        std::cerr << "is larger than the current limit (" << (INT32_MAX - 1) << ")\n";
        exit(1);
    }

    // --------- second pass to create dictionary ---------
    std::vector<const std::string *> dict; 
    dict.reserve(num_phrases);

    // fill dictionary using freq
    // time and memory consuming !!!
    for (auto &p: *(t->freq)) dict.push_back(&p.second.p);
    std::sort(dict.begin(), dict.end(), 
        [](const std::string *p1, const std::string *p2) -> bool { 
            return *p1 <= *p2; 
        });

    mt_write_dict_occ(t, output_path, dict);
    mt_overwrite_parse(t, output_path);
    
}



/*
void send(const size_t id, const size_t th, std::vector<std::string> mail[]) {
    for (size_t to = 0; to < th; ++to) {
        if (to != id) {
            std::cout << id + to << std::endl;
            
        }
    }
}

void read(const size_t id, const size_t th, std::vector<std::string> mail[]) {
    for (size_t from = 0; from < th; ++from) {
        if (from != id) {
           assert(mail[from + (th * id)].size() == 1);
        }
    }
}

void mt(const size_t id, const size_t th, std::vector<std::string> mail[], std::latch latch) {
    send(id,th,mail);
    latch.count_down();
    latch.wait();
    read(id,th,mail);
}
*/

// all possible w-length strings

// Theta (w)
//
// NOT A REASONABLE APPROACH FOR LARGER ALPHABETS
//
/*
void buildDelims(uint64_t hash, int k, const size_t p, std::map<uint64_t,size_t> *E) {
    if (k==0) {
        if (hash % p == 0) E->insert(std::pair<uint64_t,size_t>(hash, -1));
    } else {
        buildDelims((256*hash + (int)'A') % 1999999973, k-1, p, E);
        buildDelims((256*hash + (int)'C') % 1999999973, k-1, p, E);
        buildDelims((256*hash + (int)'G') % 1999999973, k-1, p, E);
        buildDelims((256*hash + (int)'T') % 1999999973, k-1, p, E);
    }
}
*/
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
              [[maybe_unused]] const std::string output_path, 
              const size_t w, 
              const size_t p, 
              const size_t NUM_THREADS) {

    std::unordered_map<uint64_t,size_t> E = buildEMap(w, p, NUM_THREADS);

    std::vector<Thread *> structs;
    std::vector<std::thread> threads;
    std::vector<std::vector<std::string>> mail(NUM_THREADS * NUM_THREADS); // i -> j  :=  i + (th * j)
    std::vector<std::unordered_map<size_t,phrase_entry> *> freqs;
    std::latch sent{(std::ptrdiff_t)NUM_THREADS};

    std::ifstream fin;
	fin.open(input_path, std::ifstream::in | std::ifstream::ate);
	while (fin.fail()) {
		std::cerr << "(!) parse_debug : could not open input file \n";
        exit(1);
	}
    
    size_t file_size = fin.tellg();
    fin.close();

    size_t work_size = file_size / NUM_THREADS;

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
        t->freq = new std::unordered_map<size_t,phrase_entry>;
        freqs.push_back(t->freq);
        t->mail = &mail;
        t->E = &E;
        
        // example for adding a phrase
        //struct phrase_entry ph;
        //(*t.freqs)[id].p = "phrase";
        structs.push_back(t);
        
        threads.push_back(std::thread([t, &sent, input_path, output_path, w, p, NUM_THREADS](){
            //send(id, NUM_THREADS, start, stop, mail);
            mt_write_parse(t, input_path, output_path, w, p, NUM_THREADS);
            sent.count_down();
            sent.wait();
            mt_write_dict(t, output_path, NUM_THREADS);
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

    
    /*
    for (size_t i = 0; i < NUM_THREADS; i ++) {
        std::cout << "[ ";
        for (size_t j = 0; j < NUM_THREADS; j ++) {
            std::cout << mail[i + NUM_THREADS * j].size() << " ";
        }
        std:: cout << "]\n";
    }
    */

    for (Thread *t : structs) {
        free(t->freq);
        free(t);
    }

}

int main(int argc, char **argv) {
    assert(argc == 9);
    mt_parse(argv[1], argv[2], atoi(argv[4]), atoi(argv[6]), atoi(argv[8]));
    return 0;
}