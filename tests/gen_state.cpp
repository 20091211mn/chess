#include <stdio.h>
#include "../core/lan.hpp"
using namespace ce;
int main(int argc,char**argv){ Game g; if(argc>1) g.loadFen(argv[1]); int you = argc>2 ? atoi(argv[2]) : 0; printf("%s\n", jsonState(g, you, "").c_str()); }
