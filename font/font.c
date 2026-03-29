#include "font.h"
#include <stdio.h>

int main() {

for (int i=0; i<sizeof(font)/sizeof(font[0]); i++) {
  printf("/* %c */\n", font[i].letter);
  for (int c=0; c<5; c++) {
    unsigned char col = 0x0;
    for (int r=0; r<7; r++) {
      if (font[i].code[r][c] != ' ') {
        col |= 0x80;
      }
      col = col >> 1;
    }
    printf("0x%1x, ", col);
  }
  printf("\n");
}

}


