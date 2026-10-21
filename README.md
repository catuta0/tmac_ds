# tmac_ds

Lê e configura o contador TMAC via SPI, em três versões que falam o mesmo
protocolo: MicroPython (tmac_ds.py), Arduino IDE (src/) e C com o Pico SDK
(c/). Cada uma instala-se e usa-se independentemente das outras.


## Protocolo

A palavra de configuração tem 10 bits:

    [9]=1  [8]=reset  [7]=out  [6:4]=level  [3:0]=channel

O bit 9 é o de arranque e vai sempre a 1. O bit 8 é o reset, que zera os 12
contadores de canal, os dois de trigger e a base de tempo. O bit 7 escolhe o
que sai no pino, 0 maioria e 1 adjacência, e só isso: os dois contadores de
trigger correm sempre, qualquer que seja. O level vai de 0 a 7 e a maioria
dispara com level+1 ou mais canais activos. O channel diz qual dos 12
contadores de canal a leitura vai reportar.

A palavra viaja alinhada à esquerda em dois bytes, ou seja deslocada seis bits
para a esquerda, e os seis bits de enchimento têm de ir a zero, senão a FPGA lê
um deles como o bit de arranque de outra palavra.

A leitura devolve um frame de 18 bytes:

    t1 | t2 | channel | ms | flags

Os quatro primeiros campos são de 32 bits, big endian. As flags são dois bytes:

    [15]=ovf t1  [14]=ovf t2  [13:2]=ovf channels  [1]=ovf time  [0]=1

O bit 0 vem sempre a 1. Se vier a 0, perdeste o alinhamento de bytes e o resto
do frame é lixo. A flag de um canal só é limpa na leitura em que esse canal
está seleccionado; as dos triggers são limpas em todas as leituras, por isso
quem quiser contagens contínuas tem de as acumular a cada canal.

Configuração e leitura são sempre transacções separadas: a FPGA captura o frame
enquanto o CS está em alto, logo uma palavra enviada dentro de uma leitura só
governaria a seguinte. Durante a leitura o MOSI fica em baixo, porque um 1 ali
seria lido como bit de arranque.


## Instalação

MicroPython: com o mpremote ligado à placa, mpremote mip install
github:catuta0/tmac_ds. Ou copia o tmac_ds.py à mão.

Arduino IDE: clona para a pasta de bibliotecas, que no macOS é
~/Documents/Arduino/libraries, ou descarrega o zip e usa Sketch, Include
Library, Add .ZIP Library. O exemplo fica em File, Examples, tmac_ds,
tmac_basic.

C com o Pico SDK: não há gestor de pacotes, a instalação é CMake. Põe o
repositório em external/tmac_ds do teu projecto, acrescenta
add_subdirectory(external/tmac_ds/c tmac_ds) ao CMakeLists.txt e tmac_ds ao
target_link_libraries. O add_subdirectory já traz os include directories, basta
incluir o tmac_ds.h.


## Um só clone

Se também desenvolves a biblioteca, uma cópia instalada por projecto significa
editar numa e esquecer as outras. Em vez disso clona uma vez e põe links para
esse clone onde cada projecto a espera, com ln -s, tanto na pasta de
bibliotecas do Arduino como no external/ do projecto do Pico. O CMake e a
procura de bibliotecas do Arduino seguem links, por isso os dois projectos
compilam sempre o que está no clone, e um git pull ali chega aos dois. Um
submódulo não serve para isto: deixa de funcionar se mudares a pasta do
projecto de sítio, porque o seu .git aponta para um caminho dentro do
repositório pai.


## API

As três versões têm as mesmas seis funções, com os nomes na convenção de cada
linguagem. Por ordem, MicroPython, Arduino e C:

- TmacDS(...), begin, tmac_init e tmac_init_bb. Abre o barramento. Em C o _bb
  faz o SPI em software em vez do de hardware, para quando os pinos livres não
  formam um SPI, e a escolha fica feita para todas as outras chamadas.
- config, config, tmac_config. Manda uma palavra com os três campos.
- reset, reset, tmac_reset. Zera tudo. Leva a configuração a deixar em vigor,
  porque a palavra carrega os três campos juntos.
- read_raw, readRaw, tmac_read_raw. Os 18 bytes como vêm.
- read_counters, readCounters, tmac_read_counters. O mesmo frame,
  descodificado.
- start_aquisition, startAquisition, tmac_start_aquisition. Só para arranque,
  ver a secção seguinte.

As constantes do campo out e do reset chamam-se MAJORITY, ADJACENCY, NO_RESET e
RESET, com prefixo TMAC_ em C e em Arduino.


## Três coisas a saber antes de a meter num programa existente

Uma leitura são 18 bytes e bloqueia cerca de 150 us a 1 MHz; uma configuração
são 2 bytes. Nunca chames nada disto de dentro de um handler de interrupção:
num programa que também serve um IRQ rápido perderias eventos. O sítio certo é
o loop principal ou um interpretador de comandos. Ser interrompido a meio de
uma transacção é inofensivo, porque o CS fica em baixo e a FPGA só avança nas
transições do SCK.

O driver não guarda estado. A palavra carrega canal, level e out juntos, e o
frame de leitura não os devolve, por isso a FPGA não pode ser interrogada sobre
a configuração em que está: cada chamada ao config declara os três campos, e
lembrar-se deles entre chamadas é trabalho de quem chama.

O start_aquisition serve só para arranque e testes. Nunca retorna, e escreve
texto CSV, num ficheiro em MicroPython e na série ou no stdout nas outras duas.
Num programa que mande binário pelo mesmo canal, o texto corrompe o fluxo. Para
qualquer coisa a sério usa o config e o read_counters e escreve o teu loop.
