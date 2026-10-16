# tmac_ds

Biblioteca para ler e configurar o contador TMAC via SPI, em duas versões que
falam o mesmo protocolo: **MicroPython** (`tmac_ds.py`) e **C com o Pico SDK**
(`c/`).

Protocolo, igual nas duas:

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
| `reset(channel=0, level=0)` | Atalho para `config(..., reset=RESET)` |
| `read_counters()` | Lê o frame de 18 bytes e devolve um `dict` |
| `read_raw()` | Devolve os 18 bytes crus |
| `start_aquisition(level, out_trigger=MAJORITY, freq_aquisition=1, filename="tmac.csv")` | Reset, depois varrimentos dos 12 canais gravados em CSV na flash da placa. Para com Ctrl-C |

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

    tmac_config(0, 3, TMAC_MAJORITY, TMAC_RESET);   // canal 0, level 3

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

| Função | Descrição |
|---|---|
| `tmac_init(spi, sck, mosi, miso, cs, baudrate)` | SPI de hardware. O tecto da FPGA é 25 MHz |
| `tmac_config(channel, level, out_trigger, reset)` | Envia a palavra de configuração |
| `tmac_reset(channel, level)` | Atalho para `tmac_config(..., TMAC_RESET)` |
| `tmac_read_raw(rx)` | Os 18 bytes crus para `rx` |
| `tmac_read_counters(&c)` | Preenche um `tmac_counters_t` |
| `tmac_start_aquisition(level, out_trigger, freq_aquisition)` | Reset, depois varrimentos dos 12 canais em CSV para o `stdout`. Não termina |

Os dois últimos parâmetros do `tmac_config` são `enum`, não inteiros:

| Constante | Tipo | Significado |
|---|---|---|
| `TMAC_MAJORITY` | `tmac_out_t` | o pino out segue o trigger de maioria |
| `TMAC_ADJACENCY` | `tmac_out_t` | o pino out segue o trigger de adjacência |
| `TMAC_NO_RESET` | `tmac_reset_t` | deixa os contadores a correr |
| `TMAC_RESET` | `tmac_reset_t` | zera os 12 contadores de canal, os 2 de trigger e a base de tempo |

Os dois contadores de trigger contam sempre, qualquer que seja o `out_trigger`:
essa escolha só decide qual das duas condições chega ao pino físico. O valor de
cada constante é o próprio bit da palavra, logo código que passasse `0` e `1`
continua a compilar.

Cada função que toca no bus tem um gémeo com sufixo **`_bb`** que faz o SPI em
software: `tmac_init_bb(sck, mosi, miso, cs, half_period_us)`,
`tmac_config_bb`, `tmac_reset_bb`, `tmac_read_raw_bb`,
`tmac_read_counters_bb`, `tmac_start_aquisition_bb`. Servem para quando os
pinos livres não formam um SPI de hardware. **Não se misturam**: inicializas
com `tmac_init` ou com `tmac_init_bb` e daí para a frente usas só o conjunto
correspondente.

Diferenças em relação à versão MicroPython, as duas por limitação do ambiente:
um programa do Pico SDK não tem sistema de ficheiros, logo a aquisição escreve
no `stdout` em vez de num ficheiro e o CSV recolhe-se do lado do PC; e não há
Ctrl-C, logo a aquisição não termina e para-se com um reset da placa.

## Integrar num programa de aquisição já existente

Exemplo com o `picodaq`, que tem um interpretador de comandos de uma letra
(`run_cmd`). Acrescentar uma letra integra a FPGA sem tocar em nada do que já
funciona.

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

// no main(), depois do stdio_init_all()
tmac_init_bb(22, 0, 1, 26, 1);      // SCK, MOSI, MISO, CS, meio periodo em us

// em run_cmd(), um comando novo: C<canal> le os contadores desse canal
case 'C': {
    tmac_counters_t c;
    tmac_config_bb(atoi(buf+1), 3, TMAC_MAJORITY, TMAC_NO_RESET);
    tmac_read_counters_bb(&c);
    printf("%lu %lu %lu %lu\n", (unsigned long)c.ms,
           (unsigned long)c.trigger1, (unsigned long)c.trigger2,
           (unsigned long)c.channel);
    break;
}
```

Três regras ao integrar:

**Não ponhas SPI dentro de uma IRQ.** No `picodaq` o `gpio_callback` corre no
flanco do `/EOLC` do ADC e tem de ser curto; uma leitura de 18 bytes a 1 MHz
leva uns 150 µs e perderias eventos. O sítio certo é o core 0, no interpretador
de comandos ou no loop principal.

**Não ponhas SPI no core 1**, que no `picodaq` é um `while(1)` apertado a
esvaziar o ring buffer.

**Ser interrompido a meio de uma transacção não faz mal.** O CS fica em baixo e
a FPGA só avança a máscara do MISO nos flancos de SCK, logo uma pausa entre
bytes não desalinha nada.

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
