#include <fstream>
#include <iostream>
#include <string>

int main(int argc,char** argv) {
    // GDI_R18_HOOK:state_startup
    std::cout<<"START:FRESH\n";
    return argc<0?1:0;
}
