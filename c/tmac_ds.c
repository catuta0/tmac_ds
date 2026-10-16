#include <stdio.h>
#include "tmac_ds.h"
#include "pico/stdlib.h"

/* ================= SPI de hardware ================= */

static spi_inst_t *tmac_spi;
static uint        tmac_cs;

void tmac_init(spi_inst_t *spi, uint sck, uint mosi, uint miso, uint cs,
               uint baudrate)
{
    tmac_spi = spi;
    tmac_cs  = cs;

    spi_init(spi, baudrate);
    spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(sck,  GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);

    /* CS por software: uma transacao, um pulso. */
    gpio_init(cs);
    gpio_set_dir(cs, GPIO_OUT);
    gpio_put(cs, 1);
}

/* Monta a palavra e alinha os 10 bits a esquerda nos dois bytes. Os seis bits
   de preenchimento tem de ficar a zero, senao a FPGA le um deles como bit de
   arranque de outra palavra. */
static void pack_word(int channel, int level, tmac_out_t out_trigger,
                      tmac_reset_t reset, uint8_t *tx)
{
    uint16_t word = (1u << 9)
                  | ((uint16_t)(reset       ? 1 : 0) << 8)
                  | ((uint16_t)(out_trigger ? 1 : 0) << 7)
                  | ((uint16_t)level                 << 4)
                  |  (uint16_t)channel;

    tx[0] = (uint8_t)((word >> 2) & 0xFF);
    tx[1] = (uint8_t)((word << 6) & 0xFF);
}

void tmac_config(int channel, int level, tmac_out_t out_trigger,
                 tmac_reset_t reset)
{
    uint8_t tx[2];
    pack_word(channel, level, out_trigger, reset, tx);

    gpio_put(tmac_cs, 0);
    spi_write_blocking(tmac_spi, tx, 2);
    gpio_put(tmac_cs, 1);
}

void tmac_reset(int channel, int level)
{
    tmac_config(channel, level, TMAC_MAJORITY, TMAC_RESET);
}

void tmac_read_raw(uint8_t *rx)
{
    gpio_put(tmac_cs, 0);
    /* O 0x00 repetido mantem o MOSI em baixo: um 1 ali seria lido como o
       arranque de uma palavra de configuracao. */
    spi_read_blocking(tmac_spi, 0x00, rx, TMAC_FRAME_LEN);
    gpio_put(tmac_cs, 1);
}

/* ================= bit banging ================= */

static uint bb_sck, bb_mosi, bb_miso, bb_cs, bb_half;

void tmac_init_bb(uint sck, uint mosi, uint miso, uint cs,
                  uint half_period_us)
{
    bb_sck  = sck;
    bb_mosi = mosi;
    bb_miso = miso;
    bb_cs   = cs;
    bb_half = half_period_us;

    gpio_init(sck);  gpio_set_dir(sck,  GPIO_OUT);
    gpio_init(mosi); gpio_set_dir(mosi, GPIO_OUT);
    gpio_init(cs);   gpio_set_dir(cs,   GPIO_OUT);
    gpio_init(miso); gpio_set_dir(miso, GPIO_IN);

    gpio_put(sck,  0);      /* SPI modo 0: o relogio repousa em baixo */
    gpio_put(mosi, 0);
    gpio_put(cs,   1);
}

static inline void bb_delay(void)
{
    if (bb_half) busy_wait_us(bb_half);
}

/* Um byte, MSB primeiro, SPI modo 0: o mestre poe o MOSI com o SCK em baixo,
   sobe o SCK (a FPGA amostra o MOSI e o mestre amostra o MISO nesse flanco) e
   volta a baixar (a FPGA avanca a mascara do MISO no flanco de descida). */
static uint8_t bb_xfer(uint8_t tx)
{
    uint8_t rx = 0;

    for (int i = 7; i >= 0; i--) {
        gpio_put(bb_mosi, (tx >> i) & 1);
        bb_delay();
        gpio_put(bb_sck, 1);
        rx = (uint8_t)((rx << 1) | (gpio_get(bb_miso) ? 1 : 0));
        bb_delay();
        gpio_put(bb_sck, 0);
    }
    return rx;
}

void tmac_config_bb(int channel, int level, tmac_out_t out_trigger,
                    tmac_reset_t reset)
{
    uint8_t tx[2];
    pack_word(channel, level, out_trigger, reset, tx);

    gpio_put(bb_cs, 0);
    bb_xfer(tx[0]);
    bb_xfer(tx[1]);
    gpio_put(bb_cs, 1);
}

void tmac_reset_bb(int channel, int level)
{
    tmac_config_bb(channel, level, TMAC_MAJORITY, TMAC_RESET);
}

void tmac_read_raw_bb(uint8_t *rx)
{
    gpio_put(bb_cs, 0);
    /* Manda zeros para manter o MOSI em baixo durante a leitura. */
    for (int i = 0; i < TMAC_FRAME_LEN; i++) rx[i] = bb_xfer(0x00);
    gpio_put(bb_cs, 1);
}

/* ================= comum as duas versoes ================= */

static uint32_t u32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static void decode(const uint8_t *rx, tmac_counters_t *c)
{
    uint16_t f = ((uint16_t)rx[16] << 8) | rx[17];

    c->trigger1 = u32be(rx + 0);
    c->trigger2 = u32be(rx + 4);
    c->channel  = u32be(rx + 8);
    c->ms       = u32be(rx + 12);
    c->ovf_t1   = (f >> 15) & 1;
    c->ovf_t2   = (f >> 14) & 1;
    c->ovf_ch   = (f >>  2) & 0x0FFF;
    c->ovf_time = (f >>  1) & 1;
    c->valid    =  f        & 1;
}

void tmac_read_counters(tmac_counters_t *c)
{
    uint8_t rx[TMAC_FRAME_LEN];
    tmac_read_raw(rx);
    decode(rx, c);
}

void tmac_read_counters_bb(tmac_counters_t *c)
{
    uint8_t rx[TMAC_FRAME_LEN];
    tmac_read_raw_bb(rx);
    decode(rx, c);
}

/* Reset, depois varrimentos dos 12 canais a freq_aquisition, uma linha CSV
   por leitura para o stdout, ou seja para o monitor serial. Nao termina: le-se
   no monitor e para-se com um reset da placa.
   Todas as contagens sao continuas, com 2^32 somado por cada volta. */
static void acquire(int level, tmac_out_t out_trigger, int freq_aquisition,
                    bool bb)
{
    uint32_t period_ms = 1000u / (freq_aquisition > 0 ? freq_aquisition : 1);
    uint32_t wraps[12] = {0};
    uint64_t wraps_t1 = 0, wraps_t2 = 0;
    tmac_counters_t c;

    printf("ms,ch,count,trigger1,trigger2\n");
    if (bb) tmac_config_bb(0, level, out_trigger, TMAC_RESET);
    else    tmac_config(0, level, out_trigger, TMAC_RESET);

    while (true) {
        uint32_t start = to_ms_since_boot(get_absolute_time());

        for (int ch = 0; ch < 12; ch++) {
            if (bb) { tmac_config_bb(ch, level, out_trigger, TMAC_KEEP_COUNT);
                      tmac_read_counters_bb(&c); }
            else    { tmac_config(ch, level, out_trigger, TMAC_KEEP_COUNT);
                      tmac_read_counters(&c); }

            /* A FPGA limpa a flag do canal apenas na leitura do canal
               selecionado, por isso olhamos so para o bit deste canal. As dos
               triggers sao limpas em todas as leituras, logo tem de ser
               acumuladas a cada canal. */
            if ((c.ovf_ch >> ch) & 1) wraps[ch]++;
            wraps_t1 += c.ovf_t1;
            wraps_t2 += c.ovf_t2;

            printf("%lu,%d,%llu,%llu,%llu\n",
                   (unsigned long)c.ms, ch,
                   (unsigned long long)(((uint64_t)wraps[ch] << 32) + c.channel),
                   (unsigned long long)((wraps_t1 << 32) + c.trigger1),
                   (unsigned long long)((wraps_t2 << 32) + c.trigger2));
        }

        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start;
        if (elapsed < period_ms) sleep_ms(period_ms - elapsed);
    }
}

void tmac_start_aquisition(int level, tmac_out_t out_trigger,
                           int freq_aquisition)
{
    acquire(level, out_trigger, freq_aquisition, false);
}

void tmac_start_aquisition_bb(int level, tmac_out_t out_trigger,
                              int freq_aquisition)
{
    acquire(level, out_trigger, freq_aquisition, true);
}
