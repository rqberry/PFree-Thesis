
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include "exceptions.hpp"

#include "../include/pfree.hpp" 

extern "C" {
    #include "../gsa/gsacak.h"
}

#pragma once


void read_input(Args args) {

    if (args.input_path.compare("") == 0) {
        throw bwt_error("No file passed to arguments. (Input File Name Empty)");
    }
	
    std::ifstream fin;

	fin.open(args.input_path, std::ifstream::in);
	while (fin.fail()){
		throw bwt_error("Failed to open input file: \"" + args.input_path + "\"");
	}

    char element;
    while (fin >> element)
    {
        Data::input.push_back((char_t)element);
    }

	fin.close();
}

bool comp(const std::pair<size_t, entry_t> &e1, const std::pair<size_t, entry_t> &e2) {
    return e1.second.phrase < e2.second.phrase;
}

void write_data_to_disk(size_t w) {
    // sort the dictionary
    // this is terrible for space complexity find a better way in practice
    /*
    std::vector<std::pair<size_t, entry_t>> sorted_dict(Data::dict.begin(), Data::dict.end());
    
    */

    std::vector<std::pair<size_t, entry_t>> sorted_dict(Data::dict.begin(), Data::dict.end());
    std::sort(sorted_dict.begin(), sorted_dict.end(), comp);
    for (size_t i = 0; i < sorted_dict.size(); ++i) { 
        Data::dict.find(sorted_dict[i].first)->second.rank = i; //ranking from 1 since gsa uses 0
    }

    // find a good way to keep parse in a file on disk, then replace entries
    // likely this can be done by generating a buffer and flushing at the end or something
    std::ofstream parse_out(Settings::OUTPUT_PATH + "parse.dat", std::fstream::out);
    std::ifstream parse_in(Settings::OUTPUT_PATH + "tmp_parse.dat", std::fstream::in);
    
    std::string line;
    while (std::getline(parse_in, line)) {
        size_t hash = std::stoul(line);
        parse_out << Data::dict.find(hash)->second.rank << '\n';
    }
    parse_out.flush();
    parse_out.close();
    parse_in.close();
    std::remove((Settings::OUTPUT_PATH + "tmp_parse.dat").c_str());

    /*
    for (size_t hash : parse) parse_out << Data::dict.find(hash)->second.rank << '\n';
    parse_out.flush();
    parse_out.close();
    */ 
    std::ofstream dict_out(Settings::OUTPUT_PATH + "dict.dat", std::ofstream::out);
    for (std::pair<size_t, entry_t> entry : sorted_dict) {
        for (char_t c : entry.second.phrase) {
            dict_out << c;
        }
        dict_out << '\n';
    }
    dict_out.flush();
    dict_out.close();

    std::ofstream freq_out(Settings::OUTPUT_PATH + "freq.dat", std::ofstream::out);
    for (std::pair<size_t, entry_t> entry : sorted_dict) freq_out << entry.second.occ << '\n';
    freq_out.flush();
    freq_out.close();

    std::ofstream w_out(Settings::OUTPUT_PATH + "w.dat", std::ofstream::out);
    for (std::pair<size_t, entry_t> entry : sorted_dict) w_out << entry.second.phrase.at(entry.second.phrase.size() - (w + 1));
    w_out.flush();
    w_out.close();
}

void write_data_to_disk_bin(size_t w) {

    std::vector<std::pair<size_t, entry_t>> sorted_dict(Data::dict.begin(), Data::dict.end());
    std::sort(sorted_dict.begin(), sorted_dict.end(), comp);
    for (size_t i = 0; i < sorted_dict.size(); ++i) {
        Data::dict.find(sorted_dict[i].first)->second.rank = i;
    }

    // find a good way to keep parse in a file on disk, then replace entries
    // likely this can be done by generating a buffer and flushing at the end or something
    std::ofstream parse_out(Settings::OUTPUT_PATH + "parse.bin", std::fstream::out | std::fstream::binary);
    std::ifstream parse_in(Settings::OUTPUT_PATH + "tmp_parse.bin", std::fstream::in);
    
    uint64_t hash;
    for (size_t i = 0; i < Data::P; ++i) {
        parse_in.read(reinterpret_cast<char *>(&hash), sizeof(hash));
        uint32_t rank = Data::dict.find(hash)->second.rank;
        parse_out.write(reinterpret_cast<char *>(&rank), sizeof(rank));
    }
    parse_out.flush();
    parse_out.close();
    parse_in.close();
    std::remove((Settings::OUTPUT_PATH + "tmp_parse.bin").c_str());

    std::ofstream dict_out(Settings::OUTPUT_PATH + "dict.bin", std::ofstream::out | std::fstream::binary);
    for (std::pair<size_t, entry_t> entry : sorted_dict) {
        for (char_t c : entry.second.phrase) {
            dict_out.write(reinterpret_cast<char *>(&c), sizeof(c));
        }
    }
    dict_out.flush();
    dict_out.close();

    std::ofstream freq_out(Settings::OUTPUT_PATH + "freq.bin", std::ofstream::out | std::fstream::binary);
    for (std::pair<size_t, entry_t> entry : sorted_dict) {
        uint16_t o = entry.second.occ;
        freq_out.write(reinterpret_cast<char *>(&o), sizeof(o));
    }
    freq_out.flush();
    freq_out.close();

    std::ofstream w_out(Settings::OUTPUT_PATH + "w.bin", std::ofstream::out | std::fstream::binary);
    for (std::pair<size_t, entry_t> entry : sorted_dict) {
        uint16_t i = entry.second.phrase.at(entry.second.phrase.size() - (w + 1));
        w_out.write(reinterpret_cast<char *>(&i), sizeof(i));
    }
    w_out.flush();
    w_out.close();
}

void read_parse(int_text *Text) {
    std::ifstream parse_in(Settings::OUTPUT_PATH + "parse.dat", std::fstream::in);

    std::string line;
    uint32_t i = 0;
    while (std::getline(parse_in, line)) {
        uint32_t rank = std::stoul(line);
        Text[i] = rank;
        i++;
    }
    parse_in.close();

    Text[Data::P] = 0;
}

void read_parse_bin(int_text *Text) {
    std::ifstream parse_in(Settings::OUTPUT_PATH + "parse.bin", std::fstream::in);

    uint32_t rank;
    for (size_t i = 0; i < Data::P; ++i) {
        parse_in.read(reinterpret_cast<char *>(&rank), sizeof(rank));
        Text[i] = rank;
    }
    parse_in.close();

    Text[Data::P] = 0;
}

void write_bwt(uint32_t *Text, uint32_t *SA) {
    std::ofstream bwt_out(Settings::OUTPUT_PATH + "parse.bwt", std::ofstream::out);
    uint32_t rank;
    for (uint32_t i = 0; i < Data::P + 1; ++i) {
        if (SA[i] > 0) {
            rank = Text[SA[i]-1];
        } else {
            rank = 0;
        }
        bwt_out << rank << '\n';
    }
    free(Text);
    free(SA);
}

void write_bwt_bin(uint32_t *Text, uint32_t *SA) {
    std::ofstream bwt_out(Settings::OUTPUT_PATH + "parse_bwt.bin", std::ofstream::out | std::ofstream::binary);
    uint32_t rank;
    for (uint32_t i = 0; i < Data::P + 1; ++i) {
        if (SA[i] > 0) {
            rank = Text[SA[i]-1];
        } else {
            rank = 0;
        }
        bwt_out.write(reinterpret_cast<char *>(&rank), sizeof(rank));
    }
    free(Text);
    free(SA);
}