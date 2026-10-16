#include <stdio.h>
#include "tmac_ds.h"
#include "pico/stdlib.h"

/* ================= which bus, chosen at init ================= */

static bool use_bb = false;

/* Hardware SPI. */
static spi_inst_t *hw_spi;
static uint        hw_cs;

/* Bit banged bus. */
static uint bb_sck, bb_mosi, bb_miso, bb_cs, bb_half;

void tmac_init(spi_inst_t *spi, uint sck, uint mosi, uint miso, uint cs,
               uint baudrate)
{
    use_bb = false;
    hw_spi = spi;
    hw_cs  = cs;

    spi_init(spi, baudrate);
    spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(sck,  GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);

    /* Chip select driven by software: one transaction, one pulse. */
    gpio_init(cs);
    gpio_set_dir(cs, GPIO_OUT);
    gpio_put(cs, 1);
}

void tmac_init_bb(uint sck, uint mosi, uint miso, uint cs, uint baudrate)
{
    use_bb  = true;
    bb_sck  = sck;
    bb_mosi = mosi;
    bb_miso = miso;
    bb_cs   = cs;

    /* Half of the SCK period, in microseconds: 1e6 / (2 * baudrate). Rounds
       down to zero above 500 kHz, which means no added delay at all. */
    bb_half = baudrate ? (500000u / baudrate) : 0;

    gpio_init(sck);  gpio_set_dir(sck,  GPIO_OUT);
    gpio_init(mosi); gpio_set_dir(mosi, GPIO_OUT);
    gpio_init(cs);   gpio_set_dir(cs,   GPIO_OUT);
    gpio_init(miso); gpio_set_dir(miso, GPIO_IN);

    gpio_put(sck,  0);      /* SPI mode 0: the clock idles low */
    gpio_put(mosi, 0);
    gpio_put(cs,   1);
}

/* ================= the bus, both backends behind one door ================= */

static inline void bb_delay(void)
{
    if (bb_half) busy_wait_us(bb_half);
}

/* One byte, most significant bit first, SPI mode 0: the master puts MOSI out
   while SCK is low, raises SCK (the FPGA samples MOSI and the master samples
   MISO on that edge) and lowers it again (the FPGA advances its MISO mask on
   the falling edge). */
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

static void cs_put(int level)
{
    gpio_put(use_bb ? bb_cs : hw_cs, level);
}

static void bus_write(const uint8_t *tx, int n)
{
    if (use_bb) for (int i = 0; i < n; i++) bb_xfer(tx[i]);
    else        spi_write_blocking(hw_spi, tx, (size_t)n);
}

static void bus_read(uint8_t *rx, int n)
{
    /* Zeros go out on MOSI throughout: a one there would be taken as the
       start bit of a configuration word. */
    if (use_bb) for (int i = 0; i < n; i++) rx[i] = bb_xfer(0x00);
    else        spi_read_blocking(hw_spi, 0x00, rx, (size_t)n);
}

/* ================= configuration ================= */

void tmac_config(int channel, int level, tmac_out_t out, tmac_reset_t reset)
{
    uint16_t word = (1u << 9)
                  | ((uint16_t)(reset ? 1 : 0) << 8)
                  | ((uint16_t)(out   ? 1 : 0) << 7)
                  | ((uint16_t)level           << 4)
                  |  (uint16_t)channel;

    /* Left align the 10 bits inside the two bytes. The six padding bits must
       be zero, otherwise the FPGA reads one of them as the start bit of
       another word. */
    uint8_t tx[2] = { (uint8_t)((word >> 2) & 0xFF),
                      (uint8_t)((word << 6) & 0xFF) };

    cs_put(0);
    bus_write(tx, 2);
    cs_put(1);
}

void tmac_reset(int channel, int level, tmac_out_t out)
{
    tmac_config(channel, level, out, TMAC_RESET);

    /* Clear bit 8 in the word the FPGA keeps. Without this the reset request
       stays armed, and a later transaction that starts a reception by accident
       would fire a second reset. This word fires nothing: the FPGA samples the
       bit when the reception ends, and by then this word has replaced the
       previous one. */
    tmac_config(channel, level, out, TMAC_NO_RESET);
}

/* ================= readout ================= */

void tmac_read_raw(uint8_t *rx)
{
    cs_put(0);
    bus_read(rx, TMAC_FRAME_LEN);
    cs_put(1);
}

static uint32_t u32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

void tmac_read_counters(tmac_counters_t *c)
{
    uint8_t  rx[TMAC_FRAME_LEN];
    uint16_t f;

    tmac_read_raw(rx);
    f = ((uint16_t)rx[16] << 8) | rx[17];

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

/* ================= bring-up convenience ================= */

void tmac_start_aquisition(int level, tmac_out_t out, int freq_aquisition)
{
    uint32_t period_ms = 1000u / (freq_aquisition > 0 ? freq_aquisition : 1);
    uint32_t wraps[TMAC_CHANNELS] = {0};
    uint64_t wraps_t1 = 0, wraps_t2 = 0;
    tmac_counters_t c;

    printf("ms,ch,count,trigger1,trigger2\n");
    tmac_reset(0, level, out);

    while (true) {
        uint32_t start = to_ms_since_boot(get_absolute_time());

        for (int ch = 0; ch < TMAC_CHANNELS; ch++) {
            tmac_config(ch, level, out, TMAC_NO_RESET);
            tmac_read_counters(&c);

            /* The FPGA clears a channel flag only on the readout of the
               selected channel, so look at this channel's bit alone. The
               trigger flags are cleared on every readout, so they have to be
               accumulated on each channel. */
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
