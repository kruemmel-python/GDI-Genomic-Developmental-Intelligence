#include "processor.hpp"
#include <fstream>
#include <string>
int main(int argc,char** argv) {
    if(argc!=3) return 3;
    std::ifstream input(argv[1]);
    std::ofstream errors;
    errors.open(argv[2]);
    std::string line;
    while(std::getline(input,line)) {
        processRecord(line);
    }
    return input.bad()?4:0;
}
