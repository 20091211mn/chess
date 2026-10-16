#pragma once
#include <cstdlib>
struct Chess {
  char b[8][8];
  bool whiteTurn = true;
  Chess() {
    const char* s[8] = {"rnbqkbnr","pppppppp","........","........",
                        "........","........","PPPPPPPP","RNBQKBNR"};
    for (int r=0;r<8;r++) for (int c=0;c<8;c++) b[r][c]=s[r][c];
  }
  static bool white(char p){ return p>='A' && p<='Z'; }
  bool clear(int r1,int c1,int r2,int c2){
    int dr=(r2>r1)-(r2<r1), dc=(c2>c1)-(c2<c1);
    r1+=dr; c1+=dc;
    while (r1!=r2 || c1!=c2){ if (b[r1][c1]!='.') return false; r1+=dr; c1+=dc; }
    return true;
  }
  bool valid(int r1,int c1,int r2,int c2){
    if (r1<0||r1>7||c1<0||c1>7||r2<0||r2>7||c2<0||c2>7) return false;
    char p=b[r1][c1], t=b[r2][c2];
    if (p=='.' || white(p)!=whiteTurn) return false;
    if (t!='.' && white(t)==white(p)) return false;
    int dr=r2-r1, dc=c2-c1, ar=abs(dr), ac=abs(dc);
    if (!ar && !ac) return false;
    switch (p|32) {
      case 'p': { int d=white(p)?-1:1, st=white(p)?6:1;
        if (dc==0 && t=='.')
          return dr==d || (dr==2*d && r1==st && b[r1+d][c1]=='.');
        return ac==1 && dr==d && t!='.'; }
      case 'n': return ar*ac==2;
      case 'b': return ar==ac && clear(r1,c1,r2,c2);
      case 'r': return (dr==0||dc==0) && clear(r1,c1,r2,c2);
      case 'q': return (ar==ac||dr==0||dc==0) && clear(r1,c1,r2,c2);
      case 'k'

cat > main.cpp << 'EOF'
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
    cout<<(g.whiteTurn?"الابيض":"الاسود")<<" (مثال e2e4, q=خروج): ";
    cin>>m; if (m=="q") break;
    if (m.size()<4 || !g.move(8-(m[1]-'0'), m[0]-'a', 8-(m[3]-'0'), m[2]-'a'))
      cout<<"حركة غير صحيحة\n";
  }
}
