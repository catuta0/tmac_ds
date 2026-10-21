/*
 * tmac_start_aquisition is a convenience for bring-up only: it never returns
 * and it printf()s CSV text to stdout. Do not call it from a program that
 * sends binary data over the same stdout, because the text corrupts the
 * alignment of whatever is reading it. For anything real, use tmac_config and
 * tmac_read_counters and write your own loop.
 */

#ifndef TMAC_DS_H
#define TMAC_DS_H

#include <stdint.h>
#include "hardware/spi.h"

#define TMAC_FRAME_LEN 18
#define TMAC_CHANNELS  12

/* The FPGA ceiling: half an SCK period must cover two cycles of the 100 MHz
   clki. */
#define TMAC_SCK_MAX_HZ 25000000UL

/* out: what reaches the FPGA's out pin. Both trigger counters always run,
   whatever this is set to. */
typedef enum {
    TMAC_MAJORITY  = 0,
    TMAC_ADJACENCY = 1
} tmac_out_t;

/* reset: zeroes the 12 channel counters, the 2 trigger counters and the time
   base. */
typedef enum {
    TMAC_NO_RESET = 0,
    TMAC_RESET    = 1
} tmac_reset_t;

typedef struct {
    uint32_t trigger1;
    uint32_t trigger2;
    uint32_t channel;
    uint32_t ms;
    uint16_t ovf_ch;        /* 12 bits, one per channel */
    uint8_t  ovf_t1;
    uint8_t  ovf_t2;
    uint8_t  ovf_time;
    uint8_t  valid;         /* end of frame marker; false means the master
                               lost byte alignment and the rest is junk */
} tmac_counters_t;


void tmac_init(spi_inst_t *spi, uint sck, uint mosi, uint miso, uint cs,
               uint baudrate);

/* baudrate in Hz, same meaning as in tmac_init. Bit banging cannot be exact:
   toggling the GPIOs takes time of its own, so the real clock comes out
   somewhat slower than asked, and above a few hundred kHz the delay rounds to
   nothing and the speed is simply whatever the GPIOs manage. Treat it as a
   ceiling. The FPGA ceiling of 25 MHz is never reached either way. */
void tmac_init_bb(uint sck, uint mosi, uint miso, uint cs, uint baudrate);

void tmac_config(int channel, int level, tmac_out_t out, tmac_reset_t reset);

/* Zero every counter and the time base. Takes the configuration to leave in
   place, because the word carries all three fields together. */
void tmac_reset(int channel, int level, tmac_out_t out);



/* The 18 frame bytes as they arrive, for pushing straight into a buffer of
   your own. */
void tmac_read_raw(uint8_t *rx);

/* The same frame, decoded. */
void tmac_read_counters(tmac_counters_t *c);

/* Bring-up only. See the warning at the top of this file. */
void tmac_start_aquisition(int level, tmac_out_t out, int freq_aquisition);

#endif
