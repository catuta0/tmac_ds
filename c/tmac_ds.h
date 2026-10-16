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
 * Every function comes in two versions: the plain one uses the hardware SPI,
 * the one suffixed _bb drives the bus in software (bit banging), for boards
 * where the free pins cannot form a hardware SPI. They do not mix: initialise
 * with either tmac_init or tmac_init_bb and then use only the matching set.
 */

#ifndef TMAC_DS_H
#define TMAC_DS_H

#include <stdint.h>
#include "hardware/spi.h"

#define TMAC_FRAME_LEN 18

/* out_trigger: what reaches the out pin. Both trigger counters always run,
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
    uint8_t  valid;
} tmac_counters_t;

/* ---- hardware SPI ---- */
void tmac_init(spi_inst_t *spi, uint sck, uint mosi, uint miso, uint cs,
               uint baudrate);
void tmac_config(int channel, int level, tmac_out_t out_trigger,
                 tmac_reset_t reset);
void tmac_reset(int channel, int level);
void tmac_read_raw(uint8_t *rx);
void tmac_read_counters(tmac_counters_t *c);
void tmac_start_aquisition(int level, tmac_out_t out_trigger,
                           int freq_aquisition);

/* ---- bit banging ---- */
/* half_period_us: half of the SCK period. 0 goes as fast as the GPIOs allow,
   1 gives about 500 kHz. The FPGA ceiling is 25 MHz and is never reached. */
void tmac_init_bb(uint sck, uint mosi, uint miso, uint cs,
                  uint half_period_us);
void tmac_config_bb(int channel, int level, tmac_out_t out_trigger,
                    tmac_reset_t reset);
void tmac_reset_bb(int channel, int level);
void tmac_read_raw_bb(uint8_t *rx);
void tmac_read_counters_bb(tmac_counters_t *c);
void tmac_start_aquisition_bb(int level, tmac_out_t out_trigger,
                              int freq_aquisition);

#endif
