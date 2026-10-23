#include "application.hpp"
#include "state_store.hpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc<2) { std::cout<<"START:FRESH\n"; return 0; }
    const auto loaded = readPersistedState(argv[1]);
    if(!loaded) { std::cout<<"START:FRESH\n"; return 0; }
    resumeApplication(*loaded);
    return 0;
}
