#include <iostream>
#include <string>
#include "chess.h"
using namespace std;
int main(){
  Chess g; string m;
  while (true){
    for (int r=0;r<8;r++){ cout<<8-r<<" ";
      for (int c=0;c<8;c++) cout<<g.b[r][c]<<" "; cout<<"\n"; }
    cout<<"  a b c d e f g h\n";
    cout<<(g.whiteTurn?"White":"Black")<<" (e.g. e2e4, q=quit): ";
    if(!(cin>>m) || m=="q") break;
    if (!g.moveStr(m.c_str())) cout<<"Invalid move\n";
  }
}
