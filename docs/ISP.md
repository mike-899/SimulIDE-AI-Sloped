# Programación ISP de un AVR desde un Arduino simulado (ArduinoISP)

SimulIDE puede simular la cadena completa de la "programación en circuito" (ISP):

```
avrdude / Arduino IDE / cargador integrado
        │  STK500v1 a 19200 baudios (TCP, pty o puerto serie)
        ▼
Arduino Uno simulado ejecutando ArduinoISP
        │  D10 → RESET, D11 → MOSI, D12 ← MISO, D13 → SCK  (SPI modo 0, 125 kHz)
        ▼
AVR objetivo simulado (ATtiny85/84/13, ATmega328P...)
        → su flash se escribe de verdad por los pines y, al soltar RESET, ejecuta el nuevo firmware
```

El AVR objetivo implementa el *Serial Programming Mode* de su hoja de datos (AVR910):
mientras RESET está a nivel bajo el núcleo se detiene, se muestrea MOSI en el flanco de
subida de SCK, MISO cambia en el flanco de bajada, y se ejecutan las instrucciones de
4 bytes. No se "copia" ningún .hex por detrás: lo que llega a la flash es lo que pasa por
los cables.

## 1. Montar el circuito

1. Coloca un **Arduino Uno** (categoría *Micro → Arduino*) y un **attiny85**
   (*Micro → AVR → attiny*). Varios MCU pueden convivir en el mismo circuito.
2. Cablea (pines del ATtiny85 en encapsulado DIP-8):

   | Arduino Uno | Señal  | ATtiny85     | ATtiny84 | ATtiny13 | ATmega328P |
   |-------------|--------|--------------|----------|----------|------------|
   | D10         | RESET  | PB5 (pin 1)  | PB3      | PB5      | PC6        |
   | D11         | MOSI   | PB0 (pin 5)  | PA6      | PB0      | PB3        |
   | D12         | MISO   | PB1 (pin 6)  | PA5      | PB1      | PB4        |
   | D13         | SCK    | PB2 (pin 7)  | PA4      | PB2      | PB5        |

   La alimentación (VCC/GND) de los MCU de SimulIDE es implícita (5 V). El condensador de
   10 µF entre RESET y GND del Arduino que se usa en hardware real no hace falta: el
   Arduino simulado no se reinicia al abrir el puerto.
3. Opcional: un LED con resistencia en PB3 (pin 2) del ATtiny85 para ver el parpadeo.
4. Carga **ArduinoISP** en el Arduino: clic derecho → *Load firmware* →
   `resources/examples/Arduino/ArduinoISP_attiny85/ArduinoISP.hex`
   (en la instalación: `share/simulide/examples/Arduino/ArduinoISP_attiny85/`).
5. Deja el ATtiny85 **sin firmware**: un MCU sin programa se simula como un chip borrado
   (flash a 0xFF) que espera ser programado.

El ejemplo completo está en `resources/examples/Arduino/ArduinoISP_attiny85/ArduinoISP_attiny85.simu`.

### El pin RESET de los ATtiny / ATmega sueltos

En los encapsulados de SimulIDE el pin de RESET de los AVR sueltos (PB5 del ATtiny85, PC6
del ATmega328, PB3 del ATtiny84...) aparece como E/S normal, por compatibilidad con los
circuitos existentes. Funciona como **RESET** (con su pull-up interno) cuando:

- el MCU no tiene archivo de firmware (`Program` vacío), o bien
- la propiedad **Reset_Pin** del MCU está activada (necesario para reprogramar por ISP un
  chip al que antes le cargaste un .hex desde archivo),

y además el fusible RSTDISBL no está programado (igual que en el chip real: si lo programas
por ISP, el pin pasa a ser E/S y ya no se puede volver a programar por ISP).
Los pines `RST` dedicados (placas Arduino, ATmega32/644...) siempre son RESET.

## 2. Opción A: avrdude real por TCP (Linux, Windows, macOS)

1. Clic derecho en el Arduino → *Open Serial Port*. Se crea un componente **SerialPort**
   ligado a ese Arduino (propiedad `Mcu`).
2. Propiedades del SerialPort:
   - `Mode` = **TCP_Server**
   - `Tcp_Port` = 5333 (por defecto)
   - `BaudRate` = **19200** (la velocidad de ArduinoISP; se usa para dosificar los bytes
     que entran a la UART simulada)
   - `Mcu_Uart` = 1
3. Arranca la simulación y pulsa **Open** en el SerialPort (o activa `Auto_Open`). En la
   consola aparece `listening on 127.0.0.1:5333`, y bajo el componente `tcp 127.0.0.1:5333`.
4. Programa:

   ```sh
   avrdude -c stk500v1 -P net:127.0.0.1:5333 -b 19200 -p t85 -U flash:w:blink_t85.hex:i
   ```

   Se pueden añadir fusibles y EEPROM como con un chip real, p. ej.
   `-U lfuse:w:0xE2:m -U eeprom:w:datos.hex:i`. avrdude necesita estar compilado con
   soporte de red (`net:`); las versiones de Debian/Ubuntu (7.x) y las oficiales lo tienen.
5. Al terminar, ArduinoISP suelta RESET y el ATtiny85 arranca con el nuevo programa. En la
   consola de SimulIDE: `attiny85-1: ISP programming done, 2 flash pages written`.

El servidor TCP solo escucha en `127.0.0.1` y acepta un cliente a la vez (sin
autenticación: cualquier proceso local puede conectarse mientras esté abierto).

## 3. Opción B: pseudo-terminal (Linux / macOS)

Con `Mode` = **PTY**, al abrir el SerialPort se crea un pty y su nombre se muestra bajo el
componente y en la consola (p. ej. `/dev/pts/3`):

```sh
avrdude -c stk500v1 -P /dev/pts/3 -b 19200 -p t85 -U flash:w:blink_t85.hex:i
```

Si una herramienta solo acepta nombres tipo `/dev/ttyUSB*`, crea un enlace:
`ln -s /dev/pts/3 ~/ttySIM` (algunas herramientas necesitan permisos o una regla udev
para enlaces en `/dev`).

## 4. Opción C: Arduino IDE "Subir usando programador"

El IDE de Arduino ejecuta avrdude con el programador **Arduino as ISP** (`stk500v1`,
19200 baudios). Como el puerto que muestra el IDE es siempre un puerto serie real, hay dos
caminos:

- **PTY + enlace** (Linux): usa la opción B y abre el IDE con un puerto que apunte al pty.
  El IDE 1.8 solo lista ciertos nombres (`ttyUSB*`, `ttyACM*`, `ttyS*`); si el pty no
  aparece, usa el método siguiente.
- **Programador propio con TCP**: añade a un `programmers.txt` de tu carpeta de hardware
  (por ejemplo el del núcleo ATTinyCore, o `~/Arduino/hardware/<x>/avr/programmers.txt`):

  ```
  simulideisp.name=SimulIDE ArduinoISP (TCP 5333)
  simulideisp.communication=serial
  simulideisp.protocol=stk500v1
  simulideisp.program.protocol=stk500v1
  simulideisp.program.tool=avrdude
  simulideisp.program.tool.default=avrdude
  simulideisp.program.extra_params=-Pnet:127.0.0.1:5333 -b19200
  ```

  Elige *Herramientas → Programador → SimulIDE ArduinoISP (TCP 5333)*, la placa ATtiny85
  y *Programa → Subir usando programador*.

  Este último camino sigue el formato documentado de `programmers.txt` pero **no se ha
  probado** en este entorno (no hay IDE gráfico); el comando de avrdude equivalente sí
  está probado (sección 7).

Otra alternativa siempre válida: *Programa → Exportar binario compilado* y usar avrdude
(opción A) o el cargador integrado (sección 5) con el `.hex` generado.

## 5. Cargador integrado (sin avrdude)

Clic derecho en el Arduino que ejecuta ArduinoISP → **Program target via ISP
(STK500v1)...** → elige el `.hex` y el dispositivo destino. SimulIDE habla STK500v1 con
ArduinoISP por la UART simulada (sincronización, SET_DEVICE, ENTER_PROGMODE, lectura de
firma, borrado, LOAD_ADDRESS/PROG_PAGE por páginas, verificación con READ_PAGE y
LEAVE_PROGMODE) y muestra el registro al terminar. Si la simulación no está en marcha la
arranca.

Desde un asistente por MCP (ver `docs/MCP.md`) existe la herramienta `isp_program`:

```json
{"name":"isp_program","arguments":{"programmer_id":"Arduino Uno-1",
  "hex_path":"/ruta/blink_t85.hex","target":"attiny85"}}
```

`get_component_state` de un AVR devuelve además `avr.blank`, `avr.in_reset`,
`avr.prog_enabled`, `avr.flash_head` (primeros 16 bytes de flash) y `avr.fuses`.

## 6. Qué se simula

Instrucciones del modo de programación serie: Programming Enable (`AC 53` con eco de 0x53
en el tercer byte), Chip Erase (respeta EESAVE), Load Program Memory Page (byte bajo/alto),
Load Extended Address, Write Program Memory Page (las celdas solo pasan de 1 a 0, como la
flash real: hay que borrar antes, avrdude lo hace por defecto), Read Program Memory,
Read/Write EEPROM (byte y página), Read Signature, Read Calibration, Read/Write fuses
(bajo, alto, extendido), Read/Write lock bits (LB1 bloquea la escritura, LB1+LB2 la
lectura) y Poll RDY/BSY.

Dispositivos con firma y tamaños de página: ATtiny13, 24/44/84, 25/45/85, 2313/4313,
ATmega8, 48/88/168/328(P), 16/32, 164/324/644/1284, 1280/2560.

Temporización: ArduinoISP usa SPI hardware a 125 kHz (periodo de 8 µs). SimulIDE avanza el
circuito en pasos de 1 µs (velocidad por defecto) y el AVR objetivo ve cada flanco de SCK
desde la red del circuito, así que funciona con margen; el límite práctico es un SCK con
semiperiodo de al menos 2 pasos de simulación (≈ 250 kHz a 1 paso/µs). Para que esto
funcione se corrigió un error de simavr que calculaba mal el divisor de reloj del SPI
(`SPR1:SPR0`), que hacía que el SPI fuera 8 veces más rápido de lo programado.

La UART del Arduino recibe los bytes del host dosificados a la velocidad configurada
(un byte por tiempo de trama, en tiempo simulado) para no desbordar su FIFO.

## 7. Prueba de referencia

```sh
# Firmware del ATtiny85 de ejemplo
avr-gcc -mmcu=attiny85 -DF_CPU=8000000UL -Os -o blink_t85.elf blink_t85.c
avr-objcopy -O ihex blink_t85.elf blink_t85.hex
# Con el circuito de ejemplo en marcha y el SerialPort abierto:
avrdude -c stk500v1 -P net:127.0.0.1:5333 -b 19200 -p t85 -U flash:w:blink_t85.hex:i
```

Resultado esperado: `device signature = 0x1e930b (probably t85)`, `82 bytes of flash
verified`, y el pin PB3 del ATtiny85 conmutando cada 10 ms.

## 8. Limitaciones

- La flash/EEPROM/fusibles escritos por ISP viven en memoria: se mantienen al parar y
  volver a arrancar la simulación, pero **no se guardan en el .simu** (al reabrir el
  circuito el ATtiny vuelve a estar borrado). La EEPROM sí se guarda como siempre.
- Los fusibles se almacenan y se leen, pero no cambian la simulación: el reloj sigue
  siendo la propiedad `Mhz` (CKDIV8/CKSEL no tienen efecto); RSTDISBL y EESAVE sí.
- Las escrituras son instantáneas (RDY/BSY siempre "listo"); no se modela el tiempo de
  borrado/escritura de la flash.
- No hay programación de alta tensión (HVSP/paralela), ni debugWIRE, ni TPI (ATtiny10).
- El modo PTY solo existe en sistemas Unix; en Windows usa TCP (o un par de puertos
  virtuales tipo com0com con `Mode` = Serial_Port).
- avrdude mide sus tiempos de espera en tiempo real: si la simulación va mucho más lenta
  que el tiempo real puede dar *timeout*; deja la velocidad de simulación al 100 % o usa el
  cargador integrado, que cuenta el tiempo simulado.
- Arduino IDE: solo se ha probado el comando de avrdude equivalente, no el IDE gráfico.

## 9. Licencias

El modelo ISP (`src/simulator/elements/processors/avrisp.*`) y el cliente STK500v1
(`stk500client.*`) son código propio GPLv3+ escrito a partir de las hojas de datos de los
AVR, la nota de aplicación AVR910 y AVR061 (protocolo STK500); no contienen código de
avrdude. `ArduinoISP.ino` es de Randall Bohn (licencia BSD) y el `.hex` incluye el núcleo
AVR de Arduino (LGPL 2.1+); ver `resources/examples/Arduino/ArduinoISP_attiny85/readme`.
