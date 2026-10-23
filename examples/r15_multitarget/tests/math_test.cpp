#include "r15/math.hpp"
#include <cstdlib>
int main(int argc,char** argv){if(argc!=4)return 2;return r15::add(std::atoi(argv[1]),std::atoi(argv[2]))==std::atoi(argv[3])?0:1;}
