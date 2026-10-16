/*
 * tmac_ds - driver C (Pico SDK) para o contador TMAC via SPI.
 *
 * Palavra de configuracao, 10 bits:
 *     [9]=1  [8]=reset  [7]=out  [6:4]=level  [3:0]=channel
 *
 * Frame de leitura, 18 bytes:
 *     t1 | t2 | channel | ms | flags
 *     flags: [15]=ovf t1  [14]=ovf t2  [13:2]=ovf channels  [1]=ovf time  [0]=1
 *
 * Cada funcao tem duas versoes: a normal usa o SPI de hardware, a terminada
 * em _bb faz o bus em software (bit banging) e serve para quando os pinos
 * livres nao formam um SPI de hardware. Nao se misturam: inicializa com
 * tmac_init ou com tmac_init_bb e usa sempre o conjunto correspondente.
 */

#ifndef TMAC_DS_H
#define TMAC_DS_H

#include <stdint.h>
#include "hardware/spi.h"

#define TMAC_FRAME_LEN 18

typedef struct {
    uint32_t trigger1;
    uint32_t trigger2;
    uint32_t channel;
    uint32_t ms;
    uint16_t ovf_ch;        /* 12 bits, um por canal */
    uint8_t  ovf_t1;
    uint8_t  ovf_t2;
    uint8_t  ovf_time;
    uint8_t  valid;
} tmac_counters_t;

/* ---- SPI de hardware ---- */
void tmac_init(spi_inst_t *spi, uint sck, uint mosi, uint miso, uint cs,
               uint baudrate);
void tmac_config(int channel, int level, int out_trigger, int reset);
void tmac_reset(int channel, int level);
void tmac_read_raw(uint8_t *rx);
void tmac_read_counters(tmac_counters_t *c);
void tmac_start_aquisition(int level, int out_trigger, int freq_aquisition);

/* ---- bit banging ---- */
/* half_period_us: metade do periodo de SCK. 0 vai o mais rapido que os GPIO
   deixam, 1 da cerca de 500 kHz. O limite da FPGA e 25 MHz, nao se atinge. */
void tmac_init_bb(uint sck, uint mosi, uint miso, uint cs,
                  uint half_period_us);
void tmac_config_bb(int channel, int level, int out_trigger, int reset);
void tmac_reset_bb(int channel, int level);
void tmac_read_raw_bb(uint8_t *rx);
void tmac_read_counters_bb(tmac_counters_t *c);
void tmac_start_aquisition_bb(int level, int out_trigger, int freq_aquisition);

#endif
