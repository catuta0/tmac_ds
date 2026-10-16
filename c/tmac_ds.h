/*
 * tmac_ds - C driver (Pico SDK) for the TMAC counter over SPI.
 *
 * Configuration word, 10 bits:
 *     [9]=1  [8]=reset  [7]=out  [6:4]=level  [3:0]=channel
 *
 * Readout frame, 18 bytes:
 *     t1 | t2 | channel | ms | flags
 *     flags: [15]=ovf t1  [14]=ovf t2  [13:2]=ovf channels  [1]=ovf time  [0]=1
 *
 * Configuration and readout are always separate transactions: the FPGA latches
 * the frame while CS is high, so a word sent inside a readout would only
 * govern the following one.
 *
 * READ THIS BEFORE USING IT IN AN EXISTING PROGRAM
 *
 * The bus. Call tmac_init for the hardware SPI, or tmac_init_bb to drive the
 * bus in software when the free pins cannot form a hardware SPI. The choice is
 * remembered and every other call uses it, so there is one API either way.
 *
 * Cost. A readout is 18 bytes and blocks for about 150 us at 1 MHz; a
 * configuration is 2 bytes. NEVER call any of this from an interrupt handler:
 * on a board that also services a fast IRQ you would miss events. The main
 * loop or a command interpreter is the right place. Being interrupted in the
 * middle of a transaction is harmless, because CS stays low and the FPGA only
 * advances on SCK edges.
 *
 * State. The word carries channel, level and out together, and the readout
 * frame does not carry it back, so the FPGA cannot be queried for it. This
 * driver keeps nothing: every tmac_config call states all three fields, and
 * remembering them between calls is the caller's job.
 *
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

/* ---- pick one of these once, everything else follows it ---- */

void tmac_init(spi_inst_t *spi, uint sck, uint mosi, uint miso, uint cs,
               uint baudrate);

/* half_period_us: half of the SCK period. 0 goes as fast as the GPIOs allow,
   1 gives about 500 kHz. The FPGA ceiling is never reached either way. */
void tmac_init_bb(uint sck, uint mosi, uint miso, uint cs,
                  uint half_period_us);

/* ---- configuration ---- */

/* One word with all three fields. level is 0 to 7: the majority trigger fires
   with level+1 or more active channels. channel selects which of the 12 per
   channel counters the readout reports. */
void tmac_config(int channel, int level, tmac_out_t out, tmac_reset_t reset);

/* Zero every counter and the time base. Takes the configuration to leave in
   place, because the word carries all three fields together. */
void tmac_reset(int channel, int level, tmac_out_t out);

/* ---- readout ---- */

/* The 18 frame bytes as they arrive, for pushing straight into a buffer of
   your own. */
void tmac_read_raw(uint8_t *rx);

/* The same frame, decoded. */
void tmac_read_counters(tmac_counters_t *c);

/* Bring-up only. See the warning at the top of this file. */
void tmac_start_aquisition(int level, tmac_out_t out, int freq_aquisition);

#endif
