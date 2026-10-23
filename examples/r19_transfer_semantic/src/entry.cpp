#include "emitter.hpp"
#include <fstream>
#include <string>
int main(int argc,char** argv) {
    if(argc!=3) return 3;
    std::ifstream source(argv[1]);
    std::ofstream rejected;
    rejected.open(argv[2]);
    std::string item;
    while(std::getline(source,item)) {
        processCatalogItem(item);
    }
    return source.bad()?4:0;
}
