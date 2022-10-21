// Code written by Conan Birkett

// fixed width 12-bit LZW decompressor

#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <string>
#include <map>
#include <utility>  // std::pair

// read input file as bytes, input blocks of data are 3 bytes corresponding to 2 12-bit codes
// if odd # of codes in compressed file final input block is just 2 bytes.
constexpr std::size_t NBITS_HALF_BYTE = 4;
constexpr std::size_t NBITS_BYTE = 8;

using byte = std::bitset<NBITS_BYTE>;   // for printing bytes

constexpr uint16_t HALF_BYTE_MASK = 15u; // 15u = 00001111

constexpr uint16_t INIT_DICT_SIZE = 256;
constexpr uint16_t MAX_DICT_SIZE = 4096; // 12 bit = 0 to 4095 therefore max size = 4096

// read in next 2 12-bit codes from file
int getCodes(std::fstream* inFile, std::pair<uint16_t, uint16_t>* outCodes);

// initialise dictionary with first 256 chars
void initDict(std::map<uint16_t, std::string>* dict);

// performs a single LZW iteration
std::string LZW(uint16_t code, std::map<uint16_t, std::string>* dict, std::string lastString);

// helpful for debug
void printBytes(uint16_t in);

int main(int argc, char** argv)
{
    // get file to decompress from args
    char* filename;
    if (argc < 2) {
        std::cout << "\nRequires a target file\n";
        return 0;
    }
    if (argc > 2) {
        std::cout << "\nOnly give one target file to decompress\n";
        return 0;
    }
    else {
        filename = argv[1];
    }

    // open compressed file
    std::fstream inFile;
    inFile.open(filename, std::fstream::in | std::fstream::binary);

    if (!inFile.is_open()) {
        std::cout << "\nNo such file: " << filename << std::endl;
    }
    else {
        std::cout << "\nDecompressing " << filename << " ...";
    }

    // LZW variables
    std::pair<uint16_t, uint16_t> codes;
    std::string currentStr = "";
    std::string lastStr = "";
    std::string output = "";

    // initialise dictionary
    std::map<uint16_t, std::string> dict;
    initDict(&dict);

    // get first 2 codes
    //getCodes(&inFile, &codes);
    
    //TODO check for blank files or 1 byte long

    while (!inFile.eof()) {
        int numCodes = getCodes(&inFile, &codes);  // will set eof

        if (numCodes > 0) {
            // LZW first code
            lastStr = currentStr;
            currentStr = LZW(codes.first, &dict, lastStr);
            output.append(currentStr);

            //std::cout << "code: " << codes.first << std::endl;
            //std::cout << "string: " << currentStr << std::endl << std::endl;

            if (numCodes == 2) {
                // LZW second code
                lastStr = currentStr;
                currentStr = LZW(codes.second, &dict, lastStr);
                output.append(currentStr);

                //std::cout << "code: " << codes.second << std::endl;
                //std::cout << "string: " << currentStr << std::endl << std::endl;
            }
        }
        else {
            // if getCodes returns no codes, error or eof
            break;
        }
    }

    std::cout << " decompression complete!" << std::endl << std::endl;

    // final output
    // TODO
    // more advanced program might output to a file
    // would then be able to avoid output string getting large
    std::cout << output << std::endl;

    return 0;
}

int getCodes(std::fstream* inFile, std::pair<uint16_t, uint16_t>* outCodes) {

    int result = 0; // 0 = fail, no codes

    uint16_t byte1, byte2, byte3;   // from file
    uint16_t out1, out2;    //12-bit code outputs

    byte1 = inFile->get();
    // eof bit is set by a failed get()
    // check for end of file if previous byte3 was last byte
    if (inFile->eof()) {
        out1 = out2 = 0;
        inFile->close();
        return result;
    }

    // first code = byte 1 + first 1/2 of byte 2
    out1 = byte1;
    out1 = out1 << NBITS_BYTE;  // shift to high byte of word

    byte2 = inFile->get();
    // safety
    if (inFile->eof()) {
        out1 = out2 = 0;
        inFile->close();
        return result;
    }

    out1 += byte2;
    out1 = out1 >> NBITS_HALF_BYTE; // shift whole word right by 1/2 byte to get 12-bit code

    // second code = second 1/2 of byte 2 + byte 3
    out2 = byte2 & HALF_BYTE_MASK;  // = 15u = 00001111, second half mask
    out2 = out2 << NBITS_BYTE;      // shift to high byte of word

    // if byte 2 was last byte (odd # of codes) then return
    byte3 = inFile->get();
    if (inFile->eof()) {
        // final 2 bytes handled differently if odd # of codes
        out1 = byte1 << NBITS_BYTE;
        out1 += byte2;
        
        out2 = 0;   // there is no second code if byte 2 was last
        inFile->close();
        result = 1;
    }
    else {
        out2 += byte3;
        result = 2;
    }

    outCodes->first = out1;
    outCodes->second = out2;

    return result;

    /*  debug code
    std::cout << byte1 << std::endl;
    printBytes(byte1);
    std::cout << byte2 << std::endl;
    printBytes(byte2);
    std::cout << byte3 << std::endl << std::endl;
    printBytes(byte3);

    std::cout << out1 << std::endl;
    printBytes(out1);
    std::cout << out2 << std::endl;
    printBytes(out2);
    */
}

void initDict(std::map<uint16_t, std::string>* dict) {
    dict->clear();
    
    // fills dictionary with first 256 ASCII chars
    char c;
    for (uint16_t i = 0; i < INIT_DICT_SIZE; i++) {
        c = static_cast<char>(i);
        dict->insert({ i, std::string(1, c) });
        //std::cout << dict->at(i) << std::endl;
    }
}

std::string LZW(uint16_t code, std::map<uint16_t, std::string>* dict, std::string lastString) {

    //std::cout << "dict size = " << dict->size() << std::endl;

    // if new dict just output string from first code
    // otherwise there will be two copies in dict
    if (dict->size() == INIT_DICT_SIZE && lastString =="") {
        return dict->at(code); 
    }

    std::string newString = "";
    uint16_t nextCode = dict->size();    // next code to add to dictionary

    std::string result;

    if (dict->contains(code)) {    // map.contains() from C++20
        
        result = dict->at(code);    // decode

        // concat previous string with first symbol of current string
        // and add to dict
        newString = lastString + result.front();  
    }
    else {
        // concat previous string with first symbol of itself
        // add it to dict
        newString = lastString + lastString.front();
        result = newString;
    }

    dict->insert({ nextCode, newString });

    // if dict is full re-initialise
    if (dict->size() >= MAX_DICT_SIZE) initDict(dict);

    return result;
}

void printBytes(uint16_t in) {

    // second 8 bits
    std::cout << byte(in >> NBITS_BYTE).to_string();

    // first 8 bits
    std::cout << " " << byte(in).to_string();

    std::cout << std::endl;
}