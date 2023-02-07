

#include <string>
#include <iostream>
#include <time.h>

#include "../include/exceptions.hpp"
#include "../include/pfree_args.hpp" // for bwt_flags
#include "../include/pfree_io.hpp"
#include "../include/pfree.hpp" 
#include "../include/pfree_parse.hpp" // for gen_E()

extern "C" {
    #include "../gsa/gsacak.h"
}

void print_arg(std::string arg_name, std::string value) {
    std::cout << "[" << arg_name << ":" << value << "] ";
}

int main(int argc, char **argv) {

    Args args;

    try {
        args = read_args(argc, argv);
    } catch (std::exception& e) {
        if (args.debug_mode) std::cout << ">>> From bwt_flags() in main(): " << std::endl << ">>>    ";
        std::cout << "Caught exception: " << e.what() << std::endl;
    }

    if (args.helper_mode) return 0;

    try {
        
        read_input(args); // try to read flag as file input

    } catch (bwt_error& e) {
        throw(e);
    }

    std::cout << "=== Parsing" << std::endl;

    if (args.debug_mode) {
        std::cout << " <D> Generating E ... " << std::endl;
    }

    // record time of each step
    uint32_t parse_sec, parse_nsec;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    gen_delims(Data::input, args.w, args.p);

    clock_gettime(CLOCK_MONOTONIC, &end);
    parse_sec = (end.tv_sec - start.tv_sec);
    parse_nsec = (end.tv_nsec - start.tv_nsec);

    if (args.debug_mode) {
        std::cout << " <D> Padding input ... " << std::endl;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    pad_input(Data::input, args.w);

    clock_gettime(CLOCK_MONOTONIC, &end);
    parse_sec += (end.tv_sec - start.tv_sec);
    parse_nsec += (end.tv_nsec - start.tv_nsec);
    
    if (args.debug_mode) {
        std::cout << " <D> |E| = " << Data::delims.size() << std::endl;     
        std::cout << " <D> Generating DP_PARSE ... " << std::endl;
    }

    /* If in debug mode the parse is output to a readable text file format, otherwise the 
    parse is output to a binary .bin file. Discrepencies in size are due to the fact that
    the text based parse file is delimited by newline characters while the binary is not.
    */
    if (args.debug_mode) {
        /* note we likely also get worse performance in debug mode since writing less and
        to a binary output file should be slightly faster.
        */ 
        clock_gettime(CLOCK_MONOTONIC, &start);

        prefix_free_parse(Data::input, args.w);

        clock_gettime(CLOCK_MONOTONIC, &end);
        parse_sec += (end.tv_sec - start.tv_sec);
        parse_nsec += (end.tv_nsec - start.tv_nsec);
    } else {
        clock_gettime(CLOCK_MONOTONIC, &start);

        prefix_free_parse_bin(Data::input, args.w);

        clock_gettime(CLOCK_MONOTONIC, &end);
        parse_sec += (end.tv_sec - start.tv_sec);
        parse_nsec += (end.tv_nsec - start.tv_nsec);
    }
    
    if (args.debug_mode) std::cout << " <D> Writing data to disk ..." << std::endl;
    
    /* If in debug mode data structures are output to a readable text file format, 
    otherwise they are output to a binary .bin file. 
    */
    if (args.debug_mode) {
        clock_gettime(CLOCK_MONOTONIC, &start);

        write_data_to_disk(args.w);

        clock_gettime(CLOCK_MONOTONIC, &end);
        parse_sec += (end.tv_sec - start.tv_sec);
        parse_nsec += (end.tv_nsec - start.tv_nsec);
    } else {
        clock_gettime(CLOCK_MONOTONIC, &start);

        write_data_to_disk_bin(args.w);

        clock_gettime(CLOCK_MONOTONIC, &end);
        parse_sec += (end.tv_sec - start.tv_sec);
        parse_nsec += (end.tv_nsec - start.tv_nsec);
    }

    std::cout << "    Parsing time: " << (parse_sec + (parse_nsec / (uint32_t)(1e+9))) << "." << (parse_nsec % (uint32_t)(1e9)) << std::endl;

    if (args.parse_only) return 0;

    /* During this phase of the algorithm we use the gsa library to compute the SA
    of the parse P. 
    */
    std::cout << "=== Computing BWT of P" << std::endl;
    
    int_text *Text = (int_text *)malloc((Data::P + 1) * sizeof(uint32_t));

    /* During the construction of the parse, if in debug mode, we must read from a
    text file format, otherwise we expect the data to be stored in a .bin file.
    */

    if (args.debug_mode) std::cout <<  " <D> Reading in the parse from disk ... "  << std::endl;

    if (args.debug_mode) {
        read_parse(Text);
    } else {
        read_parse_bin(Text);
    }

    if (args.debug_mode) std::cout <<  " <D> Computing SA ... "  << std::endl;        

    uint_t *SA = (uint_t *)malloc((Data::P + 1)*sizeof(uint_t));
    int depth = sacak_int(Text, SA, Data::P + 1, Data::dict.size());
    if (args.debug_mode) std::cout << " <D> SA computed with depth: " << depth << std::endl;
    assert(SA[0]==Data::P); // ?

    if (args.debug_mode) std::cout <<  " <D> Writing BWT of P ... "  << std::endl; 

    /* We write the BWT to a readable text file in debug mode and to a .bin file otherwise.
    */
    if (args.debug_mode) {
        write_bwt(Text, SA);
    } else {
        write_bwt_bin(Text, SA);
    }

    

    return 0;
}