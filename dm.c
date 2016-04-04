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

void FSK_demod(FSK_demod_state *s, const s16 *samples, unsigned int nb)
{
    int buf_ptr, corr, newsample, baud_pll, i;
    int sum;
    s16 tempo;

    baud_pll = s->baud_pll;
    buf_ptr = s->buf_ptr;

    for(i=0;i<nb;i++) {
        /* add a new sample in the demodulation filter */
        s->filter_buf[buf_ptr++] = samples[i] >> s->shift;
        if (buf_ptr == FSK_FILTER_BUF_SIZE) {
            memmove(s->filter_buf, 
                    s->filter_buf + FSK_FILTER_BUF_SIZE - s->filter_size, 
                    s->filter_size * sizeof(s16));
            buf_ptr = s->filter_size;
        }
        
        /* non coherent FSK demodulation - not optimal, but it seems
           very difficult to do another way */
        corr = dsp_dot_prod(s->filter_buf + buf_ptr - s->filter_size,
                            s->filter_hi_i, s->filter_size, 0);
        corr = corr >> COS_BITS;
        sum = corr * corr;
        
        corr = dsp_dot_prod(s->filter_buf + buf_ptr - s->filter_size,
                            s->filter_hi_q, s->filter_size, 0);
        corr = corr >> COS_BITS;
        sum += corr * corr;

        corr = dsp_dot_prod(s->filter_buf + buf_ptr - s->filter_size,
                            s->filter_lo_i, s->filter_size, 0);
        corr = corr >> COS_BITS;
        sum -= corr * corr;
        
        corr = dsp_dot_prod(s->filter_buf + buf_ptr - s->filter_size,
                            s->filter_lo_q, s->filter_size, 0);
        corr = corr >> COS_BITS;
        sum -= corr * corr;

	//        lm_dump_sample(CHANNEL_SAMPLESYNC, sum / 32768.0);
	tempo = (s16) (sum / 65536.0);
	fwrite(&tempo, 1, sizeof(s16), fout);
	//        printf("sum=%0.3f\n", sum / 65536.0);
	
        newsample = sum > 0;

        /* baud PLL synchronisation : when we see a transition of
           frequency, we tend to modify the baud phase so that it is
           in the middle of two bits */
        if (s->lastsample != newsample) {
            s->lastsample = newsample;
            //            printf("pll=%0.3f (%d)\n", baud_pll / 65536.0, newsample);
            if (baud_pll < 0x8000)
                baud_pll += s->baud_pll_adj;
            else
                baud_pll -= s->baud_pll_adj;
        }
        
        baud_pll += s->baud_incr;

        if (baud_pll >= 0x10000) {
            baud_pll -= 0x10000;
            //            printf("baud=%f (%d)\n", baud_pll / 65536.0, s->lastsample);
            s->put_bit(s->opaque, s->lastsample);
        }
    }

    s->baud_pll = baud_pll;
    s->buf_ptr = buf_ptr;
}


void FSK_demod_init(FSK_demod_state *s)
{
    float phase;
    int i, a;

    s->baud_incr = (s->baud_rate * 0x10000) / s->sample_rate;
    s->baud_pll = 0;
    s->baud_pll_adj = s->baud_incr / 4;

    s->filter_size = s->sample_rate / s->baud_rate;

    memset(s->filter_buf, 0, sizeof(s->filter_buf));
    s->buf_ptr = s->filter_size;
    s->lastsample = 0;

    /* compute the filters */
    for(i=0;i<s->filter_size;i++) {
        phase = 2 * M_PI * s->f_lo * i / (float)s->sample_rate;
        s->filter_lo_i[i] = (int) (cos(phase) * COS_BASE);
        s->filter_lo_q[i] = (int) (sin(phase) * COS_BASE);

        phase = 2 * M_PI * s->f_hi * i / (float)s->sample_rate;
        s->filter_hi_i[i] = (int) (cos(phase) * COS_BASE);
        s->filter_hi_q[i] = (int) (sin(phase) * COS_BASE);
    }

    s->shift = -2;
    a = s->filter_size;
    while (a != 0) {
        s->shift++;
        a /= 2;
    }
    printf("shift=%d\n", s->shift);
}

void demod_init(FSK_demod_state *s, int calling, put_bit_func put_bit, void *opaque)
{
  /* 1200 bauds */
  s->f_lo = F_LO;
  s->f_hi = F_HI;
  s->baud_rate = BAUD_RATE;

  s->sample_rate = SAMPLE_RATE;
  s->put_bit = put_bit;
  s->opaque = opaque;
 
  FSK_demod_init(s);
}

static int g_bitpos = 9;
static unsigned char g_curbyte = 0;

static void test_put_bit(void *opaque, int bit)
{
  /*
  if(bit)
    putchar('1');
  else
    putchar('0');
  */
  
  g_curbyte >>= 1;
  g_bitpos--;
  if( bit )
    g_curbyte |= 0x80;
  
  if( g_bitpos == 0 ) {
    putchar(g_curbyte);
    g_bitpos = 8;
    g_curbyte = 0;
  }
}

int main(int argc, char **argv) {
  FSK_demod_state rx;
  s16 buf1[NB_SAMPLES];
  int err, calling;
  FILE *f1;

  f1 = fopen("mod.raw", "rb");
  if( f1 == NULL) {
    perror("mod.raw");
    exit(1);
  }

  fout = fopen("dout.raw", "wb");
  if( fout == NULL) {
    perror("dout.raw");
    exit(1);
  }
  
  dsp_init();

  calling = 0;
  demod_init(&rx, 1 - calling, test_put_bit, NULL);

  

  while(!feof(f1)) {
    fread(buf1, 1, NB_SAMPLES * sizeof(s16), f1);
    
    FSK_demod(&rx, buf1, NB_SAMPLES);
  }
  fflush(fout);
  fclose(fout);

  return 0;
}
