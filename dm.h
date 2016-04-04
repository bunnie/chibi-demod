#include <math.h>


#define NB_SAMPLES 8

#define BAUD_RATE 9600

#define TESTING 0
#define SAMPLE_RATE 75000  // of the rx side, tx matches this rate when testing = 1
#define SAMPLE_RATE_TX 44100  // used to generate mod.raw when testing = 0

#define F_LO   10400
#define F_HI   15000

#define COS_BITS   14
#define COS_BASE   (1 << COS_BITS)

#define COS_TABLE_BITS 13
#define COS_TABLE_SIZE (1 << COS_TABLE_BITS)

#define PHASE_BITS 16
#define PHASE_BASE (1 << PHASE_BITS)

#define PI 3.14159265359

typedef short int s16;
typedef unsigned int u32;

/* bit I/O for data pumps */
typedef void (*put_bit_func)(void *opaque, int bit);
typedef int (*get_bit_func)(void *opaque);

typedef struct {
	float re,im;
} complex;

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

extern s16 cos_tab[COS_TABLE_SIZE];


static inline int dsp_cos(int phase) 
{
    return cos_tab[(phase >> (PHASE_BITS - COS_TABLE_BITS)) & (COS_TABLE_SIZE-1)];
}


