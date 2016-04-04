#include <stdio.h>

int main(int argc, char **argv) {
  FILE *fout;
  int i;
  unsigned char c;

  fout = fopen("data.bin", "wb");
  if( fout == NULL) {
    perror("data.bin");
    return 1;
  }

  for( i = 0; i < 65536; i++ ) {
#if 0
    if( ((i >> 4) & 0x3) == 0 ) {
      c = 0x55;
    } else if( ((i >> 4) & 0x3) == 1 ) {
      c = 0x33;
    } else if( ((i >> 4) & 0x3) == 2 ) {
      c = 0xAA;
    } else {
      c = 0xFF;
    }
#else
    c = 0x55;
#endif
    
    fwrite(&c, 1, 1, fout);
  }
  fclose(fout);
  
  return 0;
}
