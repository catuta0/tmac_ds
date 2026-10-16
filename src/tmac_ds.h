/*
 * tmac_ds - driver Arduino para o contador TMAC via SPI.
 *
 * Palavra de configuracao, 10 bits:
 *     [9]=1  [8]=reset  [7]=out  [6:4]=level  [3:0]=channel
 *
 * Frame de leitura, 18 bytes:
 *     t1 | t2 | channel | ms | flags
 *     flags: [15]=ovf t1  [14]=ovf t2  [13:2]=ovf channels  [1]=ovf time  [0]=1
 *
 * Configuracao e leitura sao sempre transaccoes separadas: a FPGA captura o
 * frame enquanto o CS esta em alto, logo uma palavra enviada dentro de uma
 * leitura so governaria a seguinte.
 */

#ifndef TMAC_DS_H
#define TMAC_DS_H

#include <Arduino.h>
#include <SPI.h>

#define TMAC_FRAME_LEN 18
#define TMAC_CHANNELS  12

/* O tecto da FPGA: meio periodo de SCK tem de cobrir dois ciclos do clki de
   100 MHz. */
#define TMAC_SCK_MAX_HZ 25000000UL

/* out: o que chega ao pino out da FPGA. Os dois contadores de trigger contam
   sempre, independentemente desta escolha. */
enum TmacOut : uint8_t {
    TMAC_MAJORITY  = 0,
    TMAC_ADJACENCY = 1
};

/* reset: zera os 12 contadores de canal, os 2 de trigger e a base de tempo. */
enum TmacReset : uint8_t {
    TMAC_NO_RESET = 0,
    TMAC_RESET    = 1
};

struct TmacCounters {
    uint32_t trigger1;
    uint32_t trigger2;
    uint32_t channel;
    uint32_t ms;
    uint16_t ovfCh;         // 12 bits, um por canal
    bool     ovfT1;
    bool     ovfT2;
    bool     ovfTime;
    bool     valid;         // marcador de fim de frame
};

class TmacDS {
public:
    // Pinos de SPI por omissao da placa. Em qualquer placa.
    void begin(uint8_t cs, uint32_t hz = 1000000);

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
    // Pinos de SPI escolhidos. So nas placas cujo core o permite.
    void begin(uint8_t cs, int8_t sck, int8_t miso, int8_t mosi,
               uint32_t hz = 1000000);
#endif

    void config(uint8_t channel, uint8_t level, TmacOut out = TMAC_MAJORITY,
                TmacReset reset = TMAC_NO_RESET);
    void reset(uint8_t channel = 0, uint8_t level = 0);

    void readRaw(uint8_t *rx);
    void readCounters(TmacCounters &c);

    // Conveniencia para testes: reset, depois varrimentos dos 12 canais a
    // freqHz, uma linha CSV por leitura para o Serial. Nao termina. Para um
    // programa a serio usa config e readCounters e escreve o teu proprio loop.
    void startAquisition(uint8_t level, TmacOut out, uint16_t freqHz);

private:
    uint8_t     _cs;
    SPISettings _spi;

    void sendWord(uint8_t channel, uint8_t level, TmacOut out, TmacReset reset);
};

#endif
