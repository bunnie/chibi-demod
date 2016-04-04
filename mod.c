#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dm.h"

FILE *fout;

static inline int dsp_dot_prod(const s16 *tab1, const s16 *tab2, 
                               int n, int sum)
{
    int i;

    for(i=0;i<n;i++) {
        sum += tab1[i] * tab2[i];
    }

    return sum;
}

void FSK_mod(FSK_mod_state *s, s16 *samples, unsigned int nb)
{
    int phase,baud_frac,b,i;

    phase = s->phase;
    baud_frac = s->baud_frac;
    b = s->current_bit;

    for(i=0;i<nb;i++) {
        baud_frac += s->baud_incr;
        if (baud_frac >= 0x10000) {
            baud_frac -= 0x10000;
            b = s->get_bit(s->opaque);
        }
        samples[i] = dsp_cos(phase);
        phase += s->omega[b];
    }
    s->phase = phase;
    s->baud_frac = baud_frac;
    s->current_bit = b;
}

void FSK_mod_init(FSK_mod_state *s)
{
    int b;

    s->omega[0] = (PHASE_BASE * s->f_lo) / s->sample_rate;
    s->omega[1] = (PHASE_BASE * s->f_hi) / s->sample_rate;
    s->baud_incr = (s->baud_rate * 0x10000) / s->sample_rate;
    s->phase = 0;
    s->baud_frac = 0;
    b = 0;
    s->current_bit = b;
}

void mod_init(FSK_mod_state *s, int calling, get_bit_func get_bit, void *opaque)
{
  /* 1200 bauds */
  s->f_lo = F_LO;
  s->f_hi = F_HI;
  s->baud_rate = BAUD_RATE;

#if TESTING
  s->sample_rate = SAMPLE_RATE;
#else
  s->sample_rate = SAMPLE_RATE_TX;
#endif

  s->get_bit = get_bit;
  s->opaque = opaque;

  FSK_mod_init(s);
}

static int g_bit_pos = 0;
static unsigned char g_curbyte = 0;

static int test_get_bit(void *opaque)
{
    FILE *fin;
    int bit;

    fin = (FILE *) opaque;

    if( g_bit_pos == 0 ) {
      if(!feof(fin))
	fread( &g_curbyte, 1, sizeof(unsigned char), fin );
      else
	g_curbyte = 0;
      g_bit_pos = 8;
    }

    bit = g_curbyte & 1;
    g_curbyte >>= 1;
    g_bit_pos--;

    return bit;
}

int main(int argc, char **argv) {
  FSK_mod_state tx;
  s16 buf1[NB_SAMPLES];
  int err, calling;
  FILE *fin;

  fout = fopen("mod.raw", "wb");
  if( fout == NULL) {
    perror("mod.raw");
    exit(1);
  }

  fin = fopen("data.bin", "rb");
  if( fout == NULL) {
    perror("data.bin");
    exit(1);
  }
  
  dsp_init();

  calling = 0;
  mod_init(&tx, 1 - calling, test_get_bit, fin);

  while(!feof(fin)) {
    FSK_mod(&tx, buf1, NB_SAMPLES);

    fwrite(buf1, 1, NB_SAMPLES * sizeof(s16), fout);
  }

  fflush(fout);
  fclose(fout);

  return 0;
}
