#include <iostream>
#include <string>
using namespace std;

string board[8] = {
  "rnbqkbnr",
  "pppppppp",
  "........",
  "........",
  "........",
  "........",
  "PPPPPPPP",
  "RNBQKBNR"
};

void draw() {
  for (int r = 0; r < 8; r++) {
    cout << 8 - r << " ";
    for (int c = 0; c < 8; c++) cout << board[r][c] << " ";
    cout << "\n";
  }
  cout << "  a b c d e f g h\n";
}

int main() { draw(); }
