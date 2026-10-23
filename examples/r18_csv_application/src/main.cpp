#include <fstream>
#include <iostream>
#include <string>

int main(int argc,char** argv) {
    if(argc!=3) return 3;
    std::ifstream input(argv[1]);
    std::ofstream errors(argv[2]);
    std::string line;
    while(std::getline(input,line)) {
        // GDI_R18_HOOK:record_processing
        std::cout<<"RECORD:"<<line<<'\n';
    }
    return input.bad()?4:0;
}
