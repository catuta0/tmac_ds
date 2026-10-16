#include "tmac_ds.h"

void TmacDS::begin(uint8_t cs, uint32_t hz)
{
    _cs  = cs;
    _spi = SPISettings(hz, MSBFIRST, SPI_MODE0);

    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    SPI.begin();
}

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
void TmacDS::begin(uint8_t cs, int8_t sck, int8_t miso, int8_t mosi,
                   uint32_t hz)
{
    _cs  = cs;
    _spi = SPISettings(hz, MSBFIRST, SPI_MODE0);

    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    // O -1 impede a biblioteca de mexer num chip select proprio: o CS e
    // conduzido a mao para cada transaccao levar exactamente um pulso.
    SPI.begin(sck, miso, mosi, -1);
}
#endif

void TmacDS::sendWord(uint8_t channel, uint8_t level, TmacOut out,
                      TmacReset reset)
{
    uint16_t word = (1u << 9)
                  | ((uint16_t)reset << 8)
                  | ((uint16_t)out   << 7)
                  | ((uint16_t)level << 4)
                  |  (uint16_t)channel;

    // Alinha os 10 bits a esquerda nos dois bytes. Os seis bits de
    // preenchimento tem de ficar a zero, senao a FPGA le um deles como bit de
    // arranque de outra palavra.
    uint16_t packed = word << 6;

    SPI.beginTransaction(_spi);
    digitalWrite(_cs, LOW);
    delayMicroseconds(10);
    SPI.transfer((uint8_t)(packed >> 8));
    SPI.transfer((uint8_t)(packed & 0xFF));
    delayMicroseconds(10);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    delayMicroseconds(20);          // a FPGA aplica o reset neste flanco
}

void TmacDS::config(uint8_t channel, uint8_t level, TmacOut out,
                    TmacReset reset)
{
    sendWord(channel, level, out, reset);

    if (reset == TMAC_RESET) {
        // Limpa o bit 8 na palavra que a FPGA guarda. Sem isto o pedido de
        // reset fica armado e uma transaccao futura que arranque uma recepcao
        // por acidente disparava um segundo reset. Esta palavra nao dispara
        // nada: a FPGA amostra o bit no fim da recepcao e nessa altura ja e
        // esta que esta em mode.
        sendWord(channel, level, out, TMAC_NO_RESET);
    }
}

void TmacDS::reset(uint8_t channel, uint8_t level)
{
    config(channel, level, TMAC_MAJORITY, TMAC_RESET);
}

void TmacDS::readRaw(uint8_t *rx)
{
    SPI.beginTransaction(_spi);
    digitalWrite(_cs, LOW);
    delayMicroseconds(10);
    // O 0x00 mantem o MOSI em baixo: um 1 ali seria lido como o arranque de
    // uma palavra de configuracao.
    for (uint8_t i = 0; i < TMAC_FRAME_LEN; i++) rx[i] = SPI.transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    delayMicroseconds(10);
}

static uint32_t u32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

void TmacDS::readCounters(TmacCounters &c)
{
    uint8_t rx[TMAC_FRAME_LEN];
    readRaw(rx);

    uint16_t f = ((uint16_t)rx[16] << 8) | rx[17];

    c.trigger1 = u32be(rx + 0);
    c.trigger2 = u32be(rx + 4);
    c.channel  = u32be(rx + 8);
    c.ms       = u32be(rx + 12);
    c.ovfT1    = (f >> 15) & 1;
    c.ovfT2    = (f >> 14) & 1;
    c.ovfCh    = (f >>  2) & 0x0FFF;
    c.ovfTime  = (f >>  1) & 1;
    c.valid    =  f        & 1;
}

// O Print do Arduino nao sabe imprimir 64 bits nas placas de 8 bits, por isso
// formata-se o numero a mao.
static void printU64(uint64_t v)
{
    char  buf[21];
    char *p = buf + 20;

    *p = '\0';
    do { *--p = (char)('0' + (v % 10)); v /= 10; } while (v);
    Serial.print(p);
}

void TmacDS::startAquisition(uint8_t level, TmacOut out, uint16_t freqHz)
{
    const uint32_t periodMs = 1000UL / (freqHz > 0 ? freqHz : 1);
    uint32_t       wraps[TMAC_CHANNELS] = {0};
    uint64_t       wrapsT1 = 0, wrapsT2 = 0;
    TmacCounters   c;

    Serial.println(F("ms,ch,count,trigger1,trigger2"));
    config(0, level, out, TMAC_RESET);

    for (;;) {
        uint32_t start = millis();

        for (uint8_t ch = 0; ch < TMAC_CHANNELS; ch++) {
            config(ch, level, out, TMAC_NO_RESET);
            readCounters(c);

            // A FPGA limpa a flag do canal apenas na leitura do canal
            // selecionado, por isso olhamos so para o bit deste canal. As dos
            // triggers sao limpas em todas as leituras, logo tem de ser
            // acumuladas a cada canal.
            if ((c.ovfCh >> ch) & 1) wraps[ch]++;
            wrapsT1 += c.ovfT1;
            wrapsT2 += c.ovfT2;

            Serial.print(c.ms);   Serial.print(',');
            Serial.print(ch);     Serial.print(',');
            printU64(((uint64_t)wraps[ch] << 32) + c.channel); Serial.print(',');
            printU64((wrapsT1 << 32) + c.trigger1);            Serial.print(',');
            printU64((wrapsT2 << 32) + c.trigger2);            Serial.println();
        }

        uint32_t elapsed = millis() - start;
        if (elapsed < periodMs) delay(periodMs - elapsed);
    }
}
