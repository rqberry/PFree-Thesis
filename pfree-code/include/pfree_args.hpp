
#include <stdint.h>
#include <string>
#include <iostream>

#include "../include/exceptions.hpp"
#include "../include/pfree.hpp"

#pragma once

// helper print method
Args print_help(Args args, std::string msg) {
    args.helper_mode = true;

    if (!msg.compare("") == 0) {
        std::cout << "There was an issue with your arguemts: \n (!) " << msg << std::endl; 
    }

    std::string help_msg = "\nExpected command-line format: ./bin/pfree input_file_path [-[flag] <arg> | -[flag]] ...\n";
    help_msg += "Arguments and flags: ";
    help_msg += "\n\tinput_file_path\t\t(!) Required. Currently accepts .fasta files*";
    help_msg += "\n\t-w\t\t\tWindow size parameter, defaults to 2.";
    help_msg += "\n\t-p\t\t\tModulus parameter, defaults to 3.";
    help_msg += "\n\t-h\t\t\tEnable helper mode, prints this message and ignores other inputs.";
    help_msg += "\n\t-D\t\t\tEnable debug mode; gives periodic debug messages and outputs to readable .txt format";
    help_msg += "\n\t--parsing\t\tEnable parse only mode; only generates the parse and does not compute BWT";
    std::cout << help_msg << std::endl;

    return args;
}

// function for handling command-line arguments
Args read_args(uint16_t argc, char ** argv) {

    Args args;

    if (argc == 1) return print_help(args, "");

    for (uint16_t i = 1; i < argc;  ++i) {
        std::string flag(argv[i]);

        if (flag.compare("-h") == 0) {

            return print_help(args, "");

        } else if (flag.compare("-D") == 0) {
            // Debug mode flag
            args.debug_mode = true;

        } else if (flag.compare("-w") == 0) {
            // Window size argument

            // assert there is argument to flag
            if (i == argc - 1) {
                std::string msg("Expected command-line argument after flag at " + std::to_string(i) + ": \"" + flag + "\".");
                return print_help(args, msg);
            }
            // try to coerce int argument
            try {
                uint16_t iw = std::stoi(argv[++i]);
                args.w = iw;
            } catch (std::exception& e) {
                std::string msg("Command-line argument \"" + std::string(argv[i]) + "\" could not be coerced to int expected after flag \"" + flag + "\": ");
                return print_help(args, msg);
            }

        } else if (flag.compare("-p") == 0) {
            // Modulus argument 

            // assert there is argument to flag
            if (i == argc - 1) {
                std::string msg("Expected command-line argument after flag at " + std::to_string(i) + ": \"" + flag + "\".");
                return print_help(args, msg);
            }
            // try to coerce int argument
            try {
                uint16_t ip = std::stoi(argv[++i]);
                args.p = ip;
            } catch (std::exception& e) {
                std::string msg("Command-line argument \"" + std::string(argv[i]) + "\" could not be coerced to int expected after flag \"" + flag + "\": ");
                return print_help(args, msg);
            }

        } else if (flag.compare("-o") == 0) {

            // NOT WORKING!
            //
            // need output path to be const for std::ofstream

            throw(bad_flag("Bad flag, -o output option currently not working. " + std::to_string(i) + ": \"" + flag + "\"."));

            /*
            // assert there is argument to flag
            if (i == argc - 1) {
                std::string msg("Expected command-line argument after flag at " + std::to_string(i) + ": \"" + flag + "\".");
                throw(bad_flag(msg));
            }

            args.output_path = argv[++i];
            */

        } else if (flag[0] != '-') {
            // assume input file name

            // probably not best practice
            args.input_path = flag;

        } else if (flag.compare("--parsing") == 0) {
            // Parse only mode flag
            args.parse_only = true;

        } else {
            std::string msg("Unrecognized flag at command-line argument " + std::to_string(i) + ": \"" + flag + "\"");
            return print_help(args, msg);
        }
    }

    return args;
}

