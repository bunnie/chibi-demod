#include <stdio.h>
#include <stdlib.h>

#define BUF_SAMPLES 32
#define SAMPLE_RATE 8000
#define F_LO   13000
#define F_HI   21000

#include <math.h>

#define PI 3.14159265359

typedef short int s16;
typedef unsigned int u32;

typedef struct {
    /* parameters */
    int f_lo,f_hi;
    int sample_rate;
    int baud_rate;

    /* local variables */
    int phase, baud_frac, baud_incr;
    int omega[2];
    int current_bit;
    void *opaque;
    get_bit_func get_bit;
} FSK_mod_state;

/* max = 106 for 75 bauds */
#define FSK_FILTER_SIZE 128  // <-- this can be 8 or so with optimal bauds
#define FSK_FILTER_BUF_SIZE 256

typedef struct {
    /* parameters */
    int f_lo,f_hi;
    int sample_rate;
    int baud_rate;

    /* local variables */
    int filter_size;
    s16 filter_lo_i[FSK_FILTER_SIZE];
    s16 filter_lo_q[FSK_FILTER_SIZE];
    
    s16 filter_hi_i[FSK_FILTER_SIZE];
    s16 filter_hi_q[FSK_FILTER_SIZE];

    s16 filter_buf[FSK_FILTER_BUF_SIZE];
    int buf_ptr;

    int baud_incr;
    int baud_pll, baud_pll_adj, baud_pll_threshold;
    int lastsample;
    int shift;

    void *opaque;
    put_bit_func put_bit;
} FSK_demod_state;



void correlate(s16 *src, s16 *dst, u32 *t, u32 freq) {
  int i;
  double s, c, r;

  for( i = 0; i < BUF_SAMPLES; i++ ) {
    s = sin( (((double)*t) * 2.0 * PI * (double) freq) / ((double) SAMPLE_RATE) ) 
      * (double) src[i];
    c = cos( (((double)*t) * 2.0 * PI * (double) freq) / ((double) SAMPLE_RATE) )
      * (double) src[i];
    r = sqrt(s*s + c*c);
    dst[i] = (s16) r;
    t++;
  }
}

void FSK_demod(FSK_demod_state *s, const s16 *samples, unsigned int nb)
{
    int buf_ptr, corr, newsample, baud_pll, i;
    int sum;

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

        lm_dump_sample(CHANNEL_SAMPLESYNC, sum / 32768.0);
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


void demod_test(void) {
  FILE *f1, *flo, *fhi;
  s16 buf[BUF_SAMPLES];
  s16 buf_lo[BUF_SAMPLES];
  s16 buf_hi[BUF_SAMPLES];
  u32 t_lo;
  u32 t_hi;

  f1 = fopen("cal.sw", "rb");
  if( f1 == NULL) {
    perror("cal.sw");
    exit(1);
  }

  flo = fopen("flo.raw", "wb");
  if( flo == NULL) {
    perror("flo.raw");
    exit(1);
  }
  
  fhi = fopen("fhi.raw", "wb");
  if( fhi == NULL ) {
    perror("fhi.raw");
    exit(1);
  }

  t_lo = 0;
  t_hi = 0;
  while(!feof(f1)) {
    fread(buf, 1, BUF_SAMPLES * sizeof(s16), f1);
    
    correlate(buf, buf_lo, &t_lo, F_LO);
    correlate(buf, buf_hi, &t_hi, F_HI);
    
    fwrite(buf_lo, 1, BUF_SAMPLES * sizeof(s16), flo);
    fwrite(buf_hi, 1, BUF_SAMPLES * sizeof(s16), fhi);
  }
  fflush(flo);
  fflush(fhi);
  fclose(flo);
  fclose(fhi);
}

int main(int argc, char **argv) {

  dsp_init();


  demod_test();

  return 0;
}
