# tmac_ds

Biblioteca para ler e configurar o contador TMAC via SPI, em três versões que
falam o mesmo protocolo: **MicroPython** (`tmac_ds.py`), **Arduino IDE**
(`src/`) e **C com o Pico SDK** (`c/`). Cada uma instala-se e usa-se
independentemente das outras.

Protocolo, igual nas três:

```
Palavra de configuração, 10 bits:
    [9]=1  [8]=reset  [7]=out  [6:4]=level  [3:0]=channel

Frame de leitura, 18 bytes:
    t1 | t2 | channel | ms | flags
    flags: [15]=ovf t1  [14]=ovf t2  [13:2]=ovf channels  [1]=ovf time  [0]=1
```

Configuração e leitura são sempre transacções separadas: a FPGA captura o frame
enquanto o CS está em alto, logo uma palavra enviada dentro de uma leitura só
governaria a seguinte. Durante a leitura o MOSI fica em baixo, porque um 1 ali
seria lido como o bit de arranque de uma palavra de configuração.

---

# MicroPython

## Instalação

No computador, com o [`mpremote`](https://docs.micropython.org/en/latest/reference/mpremote.html):

```bash
mpremote mip install github:catuta0/tmac_ds
```

Ou diretamente na placa (com Wi-Fi ligado):

```python
import mip
mip.install("github:catuta0/tmac_ds")
```

## Uso

```python
from tmac_ds import TmacDS

tmac = TmacDS()                       # SPI0, SCK=2, MOSI=3, MISO=4, CS=5
tmac.config(channel=0, level=3, out_trigger=1, reset=1)
print(tmac.read_counters())
```

Pinos diferentes:

```python
tmac = TmacDS(spi_id=1, sck=10, mosi=11, miso=12, cs=13)
```

Ou com um objeto `SPI` já criado:

```python
from machine import SPI, Pin
spi = SPI(0, baudrate=1_000_000, sck=Pin(2), mosi=Pin(3), miso=Pin(4))
tmac = TmacDS(spi=spi, cs=Pin(5, Pin.OUT))
```

## API

| Método | Descrição |
|---|---|
| `config(channel, level, out_trigger=MAJORITY, reset=NO_RESET)` | Envia a palavra de configuração (canal 0–15, nível 0–7) |
| `reset(channel=0, level=0, out_trigger=MAJORITY)` | Zera os contadores e a base de tempo, deixando esta configuração |
| `read_counters()` | Lê o frame de 18 bytes e devolve um `dict` |
| `read_raw()` | Devolve os 18 bytes crus |
| `start_aquisition(level, out_trigger=MAJORITY, freq_aquisition=1, filename="tmac.csv")` | Conveniência para testes. Reset, depois varrimentos dos 12 canais gravados em CSV na flash da placa. Para com Ctrl-C |

Campos devolvidos por `read_counters()`: `trigger1`, `trigger2`, `channel`, `ms`,
`ovf_t1`, `ovf_t2`, `ovf_ch` (12 bits, um por canal), `ovf_time`, `valid`.

Constantes do módulo, para não passar `0` e `1` à mão:

| Constante | Parâmetro | Significado |
|---|---|---|
| `MAJORITY` | `out_trigger` | o pino out segue o trigger de maioria |
| `ADJACENCY` | `out_trigger` | o pino out segue o trigger de adjacência |
| `NO_RESET` | `reset` | deixa os contadores a correr |
| `RESET` | `reset` | zera os 12 contadores de canal, os 2 de trigger e a base de tempo |

Os dois contadores de trigger contam sempre, qualquer que seja o `out_trigger`:
essa escolha só decide qual das duas condições chega ao pino físico. E como o
valor das constantes é o próprio bit, código antigo que passasse `0` e `1`
continua a funcionar.

A biblioteca não guarda a configuração: a palavra leva canal, level e out de uma
vez e a trama de leitura não a devolve, logo cada `config` declara os três
campos. E o `start_aquisition` é só para bring-up, grava num ficheiro e só para
com Ctrl-C.

---

# Arduino IDE

A raiz do repositório é a biblioteca Arduino, com as fontes em `src/`.

## Instalação

Clonar diretamente para a pasta de bibliotecas do Arduino:

```bash
cd ~/Documents/Arduino/libraries
git clone https://github.com/catuta0/tmac_ds
```

Ou, sem git: descarregar o zip do GitHub e usar **Sketch → Include Library →
Add .ZIP Library**.

Reinicia o IDE e o exemplo aparece em **File → Examples → tmac_ds →
tmac_basic**.

## Uso

```cpp
#include <tmac_ds.h>

TmacDS tmac;

void setup()
{
    Serial.begin(115200);
    tmac.begin(5, 18, 19, 23);          // CS, SCK, MISO, MOSI (ESP32)
    tmac.config(0, 3, TMAC_MAJORITY, TMAC_RESET);
}

void loop()
{
    TmacCounters c;
    tmac.readCounters(c);
    if (c.valid) Serial.println(c.trigger1);
    delay(1000);
}
```

Em placas cujo core não deixa escolher os pinos de SPI (AVR, por exemplo), usa
só `tmac.begin(cs)` e liga aos pinos de SPI por omissão da placa.

## API

| Método | Descrição |
|---|---|
| `begin(cs, hz = 1000000)` | Pinos de SPI por omissão da placa |
| `begin(cs, sck, miso, mosi, hz = 1000000)` | Pinos escolhidos. Só em ESP32, ESP8266 e RP2040 |
| `config(channel, level, out = TMAC_MAJORITY, reset = TMAC_NO_RESET)` | Envia a palavra de configuração |
| `reset(channel = 0, level = 0, out = TMAC_MAJORITY)` | Zera os contadores e a base de tempo, deixando esta configuração |
| `readRaw(rx)` | Os 18 bytes crus para `rx` |
| `readCounters(c)` | Preenche um `TmacCounters` |
| `startAquisition(level, out, freqHz)` | Reset, depois varrimentos dos 12 canais em CSV para o `Serial`. Não termina |

Campos de `TmacCounters`: `trigger1`, `trigger2`, `channel`, `ms`, `ovfCh`
(12 bits, um por canal), `ovfT1`, `ovfT2`, `ovfTime`, `valid`.

As constantes são `TMAC_MAJORITY` e `TMAC_ADJACENCY` para o `out`, e
`TMAC_NO_RESET` e `TMAC_RESET` para o `reset`, com o mesmo significado da
versão C. Aqui são `enum` com tipo, logo o IDE propõe só os valores válidos de
cada parâmetro.

Não há variante em software do bus: a biblioteca `SPI` do Arduino já o resolve,
e nas placas onde interessa os pinos são escolhidos no `begin`.

Três coisas a saber: uma leitura bloqueia uns 150 µs a 1 MHz e não deve ser
chamada de dentro de uma interrupção; a biblioteca não guarda a configuração,
porque a palavra leva os três campos de uma vez e a trama não a devolve; e o
`startAquisition` é só para bring-up, não retorna e imprime no `Serial`.

---

# C, Pico SDK

Não há gestor de pacotes para o Pico SDK: a instalação é CMake.

## Instalação

Como submódulo do teu projecto:

```bash
cd o_teu_projecto
git submodule add https://github.com/catuta0/tmac_ds external/tmac_ds
```

No `CMakeLists.txt`:

```cmake
add_subdirectory(external/tmac_ds/c tmac_ds)

target_link_libraries(o_teu_alvo
        pico_stdlib
        tmac_ds)
```

E no teu `.c`:

```c
#include "tmac_ds.h"
```

O `add_subdirectory` já traz os `include` directories, não precisas de caminhos
relativos no `#include`.

## Uso

```c
#include <stdio.h>
#include "pico/stdlib.h"
#include "tmac_ds.h"

int main(void)
{
    tmac_counters_t c;

    stdio_init_all();
    tmac_init(spi0, 2, 3, 4, 5, 1000000);   // SCK, MOSI, MISO, CS, 1 MHz

    tmac_reset(0, 3, TMAC_MAJORITY);        // canal 0, level 3, zera tudo

    while (true) {
        tmac_read_counters(&c);
        if (c.valid)
            printf("%lu %lu %lu %lu\n", (unsigned long)c.ms,
                   (unsigned long)c.trigger1, (unsigned long)c.trigger2,
                   (unsigned long)c.channel);
        sleep_ms(1000);
    }
}
```

## API

Escolhes o bus **uma vez**, no init, e tudo o resto segue essa escolha. Não há
duas famílias de funções.

| Função | Descrição |
|---|---|
| `tmac_init(spi, sck, mosi, miso, cs, baudrate)` | SPI de hardware |
| `tmac_init_bb(sck, mosi, miso, cs, baudrate)` | SPI em software, para quando os pinos livres não formam um SPI de hardware. O baudrate é um tecto, não exacto: alternar os GPIO leva tempo e o relógio real sai mais lento |
| `tmac_config(channel, level, out, reset)` | Uma palavra com os três campos |
| `tmac_reset(channel, level, out)` | Zera os contadores e a base de tempo, deixando esta configuração |
| `tmac_read_raw(rx)` | Os 18 bytes crus, para empurrar para um buffer teu |
| `tmac_read_counters(&c)` | A mesma trama, descodificada |
| `tmac_start_aquisition(level, out, freqHz)` | Só para bring-up, ver abaixo |

Constantes: `TMAC_MAJORITY` e `TMAC_ADJACENCY` para o `out`, `TMAC_NO_RESET` e
`TMAC_RESET` para o `reset`. São `enum` com tipo, logo o editor propõe só os
valores válidos de cada parâmetro e trocar a ordem dos dois últimos dá erro de
tipo.

Campos de `tmac_counters_t`: `trigger1`, `trigger2`, `channel`, `ms`, `ovf_ch`
(12 bits, um por canal), `ovf_t1`, `ovf_t2`, `ovf_time`, `valid`.

## Três coisas a saber antes de a meter num programa existente

**Custo.** Uma leitura são 18 bytes e bloqueia cerca de 150 µs a 1 MHz; uma
configuração são 2 bytes. **Nunca chames nada disto de dentro de uma
interrupção**: numa placa que também serve uma IRQ rápida perderias eventos. O
sítio certo é o loop principal ou um interpretador de comandos. Ser interrompido
a meio de uma transacção é inofensivo, porque o CS fica em baixo e a FPGA só
avança nos flancos de SCK.

**A biblioteca não guarda estado.** A palavra leva canal, level e out de uma
vez, e a trama de leitura não a devolve, logo não há como reler a configuração
da FPGA. Cada `tmac_config` declara os três campos, e lembrar-se deles entre
chamadas é trabalho de quem chama.

**O `tmac_start_aquisition` é só para bring-up.** Não retorna, e faz `printf` de
texto CSV para o `stdout`. Não a chames de um programa que mande binário pelo
mesmo `stdout`, porque o texto desalinha quem estiver a ler do outro lado. Para
algo a sério usa o `tmac_config` e o `tmac_read_counters` e escreve o teu loop.

## Integrar num programa de aquisição já existente

Exemplo com o `picodaq`, que tem um interpretador de comandos de uma letra
(`run_cmd`). Acrescentar letras integra a FPGA sem tocar em nada do que já
funciona.

Os comandos que já lá estão (`D`, `R`, `S`, `T`) seguem três regras: são verbos
que actuam, não imprimem nada, e aplicam o parâmetro ao hardware na hora. Os
dados saem por outro caminho, o ring buffer em binário pelo core 1. Convém que
os comandos novos sigam o mesmo.

No `CMakeLists.txt`, juntar ao `target_link_libraries` existente:

```cmake
add_subdirectory(external/tmac_ds/c tmac_ds)

target_link_libraries(picodaq
        pico_stdlib pico_multicore
        tmac_ds)
```

No `picodaq.c`:

```c
#include "tmac_ds.h"

// estado da configuracao, porque a palavra leva os tres campos de uma vez
int tmac_chan  = 0;
int tmac_level = 0;

// no main(), depois do init_trg()
tmac_init_bb(22, 0, 1, 26, 1000000);    // SCK, MOSI, MISO, CS, 1 MHz

// em run_cmd(), antes do case 'T' ou depois de lhe acrescentar o break que
// lhe falta, senao o T cai dentro do primeiro caso novo
case 'M':                           // M<n>: majority level, zera os contadores
    tmac_level = atoi(buf+1);
    tmac_reset(tmac_chan, tmac_level, TMAC_MAJORITY);
    break;

case 'C':                           // C<n>: canal que a leitura reporta
    tmac_chan = atoi(buf+1);
    tmac_config(tmac_chan, tmac_level, TMAC_MAJORITY, TMAC_NO_RESET);
    break;
```

Faltam os dados. A forma que respeita a arquitectura é empurrar uma trama de
contadores para o **mesmo ring buffer**, com um terminador diferente para o
host as distinguir. As contas encaixam sem mudar nada: o `EVTLEN` são 5 words,
e a trama do TMAC são 16 bytes de contadores mais 16 bits de flags.

```
word 0:  trigger1
word 1:  trigger2
word 2:  channel
word 3:  ms
word 4:  0x55AB0000 | flags      <- terminador mais as 16 flags
```

Os 16 bits de baixo do word 4 estão livres no formato dele e as flags são
exactamente 16 bits. O host separa pelo terminador: `0x55AA` para o ADC,
`0x55AB` para o TMAC.

Duas regras ao integrar, as duas já no header mas vale a pena repetir:

**Não ponhas SPI dentro de uma IRQ.** No `picodaq` o `gpio_callback` corre no
flanco do `/EOLC` do ADC e tem de ser curto; uma leitura leva uns 150 µs e
perderias eventos.

**Não ponhas SPI no core 1**, que é um `while(1)` apertado a esvaziar o ring
buffer.

Se quiseres o tempo da FPGA no stream de eventos do ADC, a maneira barata é o
core 0 guardar o `c.ms` numa `volatile` e a IRQ copiá-la para o word 0 do
frame, que no `picodaq` está comentado como `placeholder for the timestamp` e
vai a zeros. Um load, sem SPI.

### Orçamento de pinos

São precisos 4 GPIO. Lendo os `#define` do `picodaq.c`, estão ocupados o 2
(`DS_DATA`), 3 a 14 (dados do MAX1304), 15 a 19 (controlo do MAX), 20 e 21
(`REG_HIZ` e `TRG`), 27 (`DS_CLK`) e 28 (`DS_LE`). O `CMakeLists.txt` tem
`pico_enable_stdio_uart(picodaq 1)`, e a UART0 por omissão usa o **GPIO 0 e 1**.

Sobram portanto **só o 22 e o 26**, o que não chega.

Além disso, com esses pinos **não se forma um SPI de hardware**: no RP2040 o
MOSI (SPI TX) só existe nos GPIO 3, 7, 11, 15, 19, 23 e 27, e nenhum está
livre. Daí as funções `_bb`.

Para ter os 4 pinos, a saída mais limpa é passar o stdio para USB e libertar o
GPIO 0 e 1:

```cmake
pico_enable_stdio_uart(picodaq 0)
pico_enable_stdio_usb(picodaq 1)
```

Ficam livres o 0, 1, 22 e 26, que é a atribuição usada no exemplo acima. Em
alternativa, libertar um dos pinos do MAX ou do DS1023S. Confirma sempre contra
a tua ligação antes de assumir.
