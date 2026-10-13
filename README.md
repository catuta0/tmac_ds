# tmac_ds

Biblioteca MicroPython para ler e configurar o contador TMAC via SPI.

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
| `config(channel, level, out_trigger=0, reset=0)` | Envia a palavra de configuração (canal 0–15, nível 0–7) |
| `reset(channel=0, level=0)` | Atalho para `config(..., reset=1)` |
| `read_counters()` | Lê o frame de 18 bytes e devolve um `dict` |
| `read_raw()` | Devolve os 18 bytes crus |

Campos devolvidos por `read_counters()`: `trigger1`, `trigger2`, `channel`, `ms`,
`ovf_t1`, `ovf_t2`, `ovf_ch` (12 bits, um por canal), `ovf_time`, `valid`.
