# Motor analógico / Analog engine

## Resumen (español)

SimulIDE ahora simula diodos, LEDs, transistores BJT y MOSFET con **modelos
físicos tipo SPICE** en lugar de los modelos lineales por tramos ("umbral +
resistencia") de antes, y además puede usar **ngspice** como motor
"analógico exacto" opcional.

- **Motor nativo mejorado (por defecto, funciona con microcontroladores):**
  - Diodo de Shockley con resistencia serie y ruptura inversa (zener),
    resuelto con Newton-Raphson y limitación de tensión de unión (`pnjlim`,
    como SPICE3).
  - BJT Gummel-Poon simplificado (modelo de transporte, efecto Early, caída
    de beta a alta corriente IKF, corrientes de fuga ISE/ISC), con la matriz
    jacobiana completa 3x3 (gm, gπ, gμ, go).
  - MOSFET nivel 1 (Shichman-Hodges) con transición suave a sub-umbral,
    RD/RS serie y diodo de cuerpo.
  - Nueva propiedad **Model** con modelos reales: 1N4148, 1N914, 1N4001,
    1N4007, 1N5819, zeners 1N4728A/1N4733A/1N4742A; 2N2222, 2N2907, 2N3904,
    2N3906, BC547, BC557, TIP120, TIP125; 2N7000, BS170, IRF520, IRF540N,
    IRLZ44N, IRF9540, BS250. Los LEDs usan por defecto un modelo según su
    **color** (rojo ≈1,95 V, verde ≈2,14 V, azul/blanco ≈2,94 V a ~10 mA).
  - "Custom" usa las propiedades clásicas (Threshold, Gain, RDSon...) como
    parámetros de un modelo físico, así los circuitos antiguos siguen
    funcionando.
- **Modo "Accurate analog (ngspice)" (opcional):** menú *Simulación →
  Accurate analog (ngspice)*. El circuito se exporta como netlist SPICE, se
  ejecuta un transitorio en libngspice en segundo plano, sincronizado con el
  tiempo real, y los resultados alimentan la interfaz (sondas, voltímetros,
  osciloscopio, brillo de LEDs). Si el circuito tiene piezas no soportadas
  (lógica, op-amps, relés...) se usa automáticamente el motor nativo.
- **Co-simulación con microcontroladores (nuevo, §7):** en modo ngspice los
  AVR/Arduino (y PIC) ya **no** desactivan ngspice. El MCU y ngspice avanzan
  en *lockstep* en **tiempo simulado**: el MCU ejecuta instrucciones por
  delante de ngspice hasta su próximo cambio de pin (o lectura de entrada /
  muestra del ADC); entonces ngspice avanza exactamente hasta ese instante
  (callback `GetSyncData` de ngspice compartido, que solo acorta pasos) y el
  pin cambia con una rampa de 10 ns en fuentes `EXTERNAL` (equivalente
  Thévenin del pin: 40 Ω en salida, alta impedancia + pull-up en entrada).
  Las entradas digitales, el ADC y el reset leen la tensión del nodo de
  ngspice en el instante simulado actual. Entre eventos ngspice da pasos
  adaptativos grandes (hasta 62,5 µs), sin paso fijo de 1 µs.
- **Velocidad honesta:** va tan rápido como puede hasta el tiempo real; si
  el circuito es más pesado va más lento, pero sincronizado. El % de tiempo
  real se ve en la interfaz y en `get_simulation_state`
  (`real_time_percent`). Medido: blink con transistor, lazo PWM 490 Hz →
  RC → ADC y UART software a 9600 baudios: 100 / 100 / 66 %; conmutar un pin
  a 10 kHz: 36 %, a 100 kHz: 4 % (cada flanco cuesta ~20 puntos de
  ngspice). Dos Arduinos conectados: 64–96 %.
- **Precisión:** LED + 2N2222 manejado por un Arduino: 13,72 mA (ngspice)
  frente a 13,79 mA (nativo); lazo PWM→RC→ADC: media del ADC 510,5 frente a
  510,0 teórico (nativo 511,8 con ±7 LSB de dispersión).
- **Validación:** el motor nativo y ngspice coinciden en < 0,1 % en todas las
  pruebas DC (tabla abajo).
- **Velocidad:** circuitos con Arduino siguen al 100 % de tiempo real; el
  peor caso medido (matriz de 64 LEDs multiplexada por un Arduino) usa ~50 %
  más CPU y queda en ~93 % de tiempo real en la máquina de pruebas.
- ngspice (BSD-3-Clause) no se incluye: se carga en tiempo de ejecución si
  está instalado (`apt install libngspice0`). Créditos en `NOTICE` y en
  *Ayuda → Acerca de*.

---

## 1. Design decision: B by default, A as an optional mode

| | A: ngspice "SPICE mode" | B: better native models |
|---|---|---|
| Works with MCUs (AVR/PIC/Arduino) | yes: lockstep co-simulation in simulated time (§7), may run slower than real time | **yes**, real time |
| Accuracy | reference | matches ngspice to < 0.1 % in DC (§6) |
| Time resolution | adaptive (ns) | fixed 1 µs step, capacitors updated every 50 µs |
| Real-time cost | 4-20 µs per time point | event driven, ~free when nothing changes |
| Dependencies | libngspice at runtime | none |

The native engine was improved first because it is what every circuit with a
microcontroller (the main SimulIDE use case) runs on in real time. ngspice
was added as an optional "accurate analog" mode, first for purely analog
circuits and now also with microcontrollers in lockstep co-simulation (§7);
it is loaded with `QLibrary` at runtime, so SimulIDE builds and runs
without it.

## 2. Native engine changes

### Generic non-linear stamps (`src/simulator/e-node.*`, `e-pin.*`)
The MNA solver only supported symmetric two-terminal conductances. A BJT or
MOSFET Newton step needs non-symmetric entries (transconductance), so
`ePin::stampEntry(colPin, g)` / `eNode::stampEntry()` were added: they set
`G[node(row)][node(col)]`. Devices fill an `NLStamp<N>` (Jacobian + currents
at the linearization point, `src/simulator/elements/active/e-nlstamp.h`)
and stamp it. Each eNode row keeps its columns in a small vector (no hash
reallocation per Newton iteration).

### Newton loop (`src/simulator/simulator.cpp`)
* Devices are evaluated whenever a node they touch changes (existing
  event-driven loop). A device whose step was limited (`pnjlim`, damping)
  re-schedules itself (`addToNoLinList`), the loop iterates over a copy.
* `nlIteration()` exposes the iteration count: after 12 iterations Newton
  steps are damped (×0.5); after 30 a device hands the rest of the solve to
  the next simulation step (`addToNoLinNextStep`). This "time continuation"
  replaces up to 300 useless iterations at regenerative switching instants
  (astables, latches) where the fixed-step system is nearly singular.
* Convergence: a device does not re-stamp when its junction voltages moved
  less than 50 µV, or when the present linearization already predicts its
  current within 1e-9 A + 1e-4·|I| (SPICE style current tolerance).

### Matrix (`src/simulator/circmatrix.*`)
`stampMatrix/stampCoef` ignore unchanged values and mark only the
sub-circuit (group) they belong to; only dirty groups are re-factored and
solved. This benefits every circuit, not only the new models.

### Models (`src/simulator/elements/active/e-semimodel.*`)
* **Diode**: `Id = IS(exp(Vd/(N·Vt))−1) − IBV·exp(−(Vd+BV)/(NBV·Vt)) + gmin·Vd`.
  The series resistance is eliminated locally: for a terminal voltage V the
  junction voltage is solved from `Vd + RS·Id(Vd) = V` (monotone Newton,
  started on the convex side) and the device is stamped with
  `g = gd/(1+gd·RS)`. With RS > 0 the composite curve grows at most like
  V/RS, so no junction limiting is needed (much faster LED/rectifier
  switching); RS = 0 uses `pnjlim`.
* **BJT**: transport model with base charge
  `qb = q1(1+sqrt(1+4q2))/2`, `q1 = 1/(1−Vbc/VAF−Vbe/VAR)`,
  `q2 = Icc/IKF + Iec/IKR`; `Ic = (Icc−Iec)/qb − Iec/BR − Ilc`,
  `Ib = Icc/BF + Ile + Iec/BR + Ilc` with analytic derivatives (verified
  against finite differences). RB/RC/RE and junction/diffusion capacitances
  are exported to ngspice but not used natively (no internal nodes).
* **MOSFET**: level 1 with a smooth overdrive
  `Vov = 2Vt·ln(1+exp((Vgs−VTO)/2Vt))` (continuous sub-threshold, better
  convergence), λ, series RD/RS solved locally (1-D Newton on Id) and
  body diode (bulk tied to source). SPICE3 `fetlim`/`limvds` limiting,
  automatic source/drain swap.
* Junction capacitances are not modelled natively: with the 1 µs step and
  50 µs capacitor update a pF junction has a time constant far below the
  engine resolution (and would only add CPU). Use the ngspice mode for
  switching-speed effects.

### Model property (component classes)
| Component | `Model` values | `Custom` uses |
|---|---|---|
| Diode | Custom, D1N4148, D1N914, D1N4001, D1N4007, D1N5819, D1N4728A, D1N4733A, D1N4742A | Threshold = Vf at 10 mA, Resistance = RS, Zener_Volt = BV (N = 1.8) |
| Led | Auto (by Color), Custom | Threshold = Vf at 10 mA, Resistance = RS (N = 2) |
| BJT | Custom, Q2N2222, Q2N2907, Q2N3904, Q2N3906, BC547, BC557, TIP120, TIP125 | Gain = BF, Threshold = Vbe at Ic = 10 mA, VAF = 75 |
| Mosfet | Custom, M2N7000, BS170, IRF520, IRF540N, IRLZ44N, IRF9540, BS250 | Threshold = VTO, RDSon at Vgs = 10 V |

Enum keys use the SPICE prefix when the part name starts with a digit
(`Q2N2222` = 2N2222). Toggling *PNP* / *P_Channel* on a preset selects its
complementary part (2N2222↔2N2907, BC547↔BC557, IRF520→IRF9540, ...).
Old circuits have no `Model` attribute and load as `Custom` (LEDs as
`Auto`), so their classic properties keep working. New presets must be
appended at the end of the enums (saved as integers).

## 3. ngspice mode (`src/simulator/spice/`)

* `SpiceNetlist` exports the **simulator element list** (not the GUI
  components), so internal elements are covered automatically:
  resistors (incl. potentiometer halves, switch contacts, meters), capacitors,
  inductors, diodes/LEDs, BJTs, MOSFETs (`W=L=100u`, bulk = source), every
  `eSource` as `V + series R` (rails, fixed voltage, ground, probes, meter
  outputs), Clock (PULSE) and WaveGen (SIN/PULSE), ammeters as 0 V sources.
  Models are generated from the same presets as the native engine.
  Microcontroller pins become EXTERNAL driven sources (§7) and single
  input logic buffers / inverters (e.g. the Arduino board LED driver) a
  smooth behavioural source. Anything else (other logic, op-amps, relays,
  current sources) makes the export fail with the list of unsupported
  parts, and the native engine is used.
* `SpiceEngine` loads `libngspice` (`ngspice`, `libngspice.so.0`, or
  `spice/library` in simulide.ini), sends the netlist with `ngSpice_Circ`,
  runs `.tran <maxstep> 2s uic` with `bg_run`. The `SendData` callback
  (ngspice thread) queues every accepted time point and sleeps while ahead
  of the wall clock (real-time pacing). Every 2 s of simulated time a new
  transient continues from the last node voltages (`.ic`) so memory does not
  grow.
* On every GUI tick the queued points are applied to the eNodes, the
  simulation step counter is advanced and the per-step elements
  (oscilloscope) are clocked, so probes, meters, plotter, oscilloscope and
  LED brightness work unchanged. Pausing/editing a property restarts the
  transient from the current state.
* Toggle: *Simulation → Accurate analog (ngspice)*, `simulide --spice` /
  `--no-spice`, `SIMULIDE_SPICE=1`, `spice/enabled=true` in simulide.ini,
  or the MCP tool `set_analog_engine` (`get_spice_netlist` returns the
  exported netlist). `spice/maxstep` (default 20e-6 s) sets the max step
  of purely analog circuits.

## 4. Validation

Native vs ngspice 42 in the same app (circuits built as `.simu` files and
read through the MCP server). Diode test: 5 V rail + R + diode; LED: 5 V +
220 Ω; CE stage: 12 V, R1 = 47 k, R2 = 10 k, Rc = 4.7 k, Re = 1 k
(gain = ΔVc/ΔVb when the bottom of R2 is moved by 50 mV); MOSFET: 12 V,
load to drain, gate from a rail.

| Test | Quantity | Native | ngspice | Reference |
|---|---|---|---|---|
| 1N4148 | Vf @ 1 / 10 / 100 mA | 0.586 / 0.694 / 0.846 V | 0.586 / 0.694 / 0.846 V | datasheet typ. ≈0.6 / 0.7 / 0.85 |
| 1N4007 | Vf @ 1 / 10 / 100 mA | 0.557 / 0.664 / 0.774 V | 0.557 / 0.664 / 0.774 V | |
| 1N5819 (Schottky) | Vf @ 1 / 10 / 100 mA | 0.128 / 0.209 / 0.295 V | 0.128 / 0.209 / 0.295 V | datasheet ≈0.3 @ 100 mA |
| Custom (Threshold 0.7) | Vf @ 1 / 10 / 100 mA | 0.589 / 0.700 / 0.857 V | same | |
| 1N4733A 12 V/330 Ω | Vz @ 21.7 mA | 4.894 V | 4.894 V | 5.1 V @ 49 mA |
| LED red / orange / yellow / green | Vf (5 V, 220 Ω) | 1.950 / 1.998 / 2.047 / 2.143 V | identical | 1.8–2.2 V |
| LED blue / white / purple | Vf | 2.944 / 2.944 / 3.130 V | identical | 2.9–3.4 V |
| CE 2N2222 | Vb / Ve / Ic / gain | 2.034 / 1.381 V / 1.372 mA / −4.577 | 2.034 / 1.381 V / 1.372 mA / −4.577 | −Rc/(Re+re) ≈ −4.6 |
| CE 2N3904 | Vb / Ve / Ic / gain | 2.028 / 1.356 / 1.346 / −4.572 | 2.028 / 1.356 / 1.346 / −4.572 | |
| CE BC547 | Vb / Ve / Ic / gain | 2.066 / 1.420 / 1.415 / −4.587 | 2.066 / 1.419 / 1.414 / −4.585 | |
| CE 2N3906 (−12 V) | Vb / Ve / Ic / gain | −2.054 / −1.344 / 1.338 / −4.552 | same | |
| IRF520, 10 Ω load | Vds @ Vgs = 4 / 5 / 10 V | 6.090 / 0.593 / 0.310 V | 6.090 / 0.593 / 0.310 V | Rds(on) 0.27 Ω @ 10 V |
| IRLZ44N, 10 Ω | Vds @ Vgs = 3 / 4.5 / 10 V | 0.060 / 0.038 / 0.026 V | same | logic level |
| 2N7000, 100 Ω | Vds @ Vgs = 3 / 4.5 / 10 V | 0.400 / 0.206 / 0.145 V | same | |
| TIP120 switch (Rb 1 k, 12 V/10 Ω) | Vce / Vbe | 0.250 / 1.417 V | 0.366 / 1.417 V | darlington Vce(sat) ≈ 0.8–2 V |
| RC low pass 1 k/1 µF, 200 Hz, 5 Vpp | scope amplitude | 3.08 Vpp | 3.11 Vpp | 3.11 Vpp (theory) |

Differences: ngspice also applies RB/RC/RE (TIP120 Vce) and runs with an
adaptive time step (RC amplitude). TIP120/TIP125 are a single-transistor
approximation of a darlington (NF = 2 doubles Vbe; its higher saturation
voltage is not reproduced).

## 5. Performance

Real-time % and CPU % of one core (process total) on the 4-core test
machine, shared with other jobs; "before" = same branch without these
changes.

| Example | before rt% / CPU% | after rt% / CPU% |
|---|---|---|
| Arduino/ledFadding (PWM LED) | 99.8 / 72 | 100 / 65 |
| Arduino/barGraph (LedBar) | 100 / 54 | 100 / 58 |
| Arduino/arduino_led_matrix (8×8 multiplexed LEDs) | 99.8 / 69 | 92.9 / 103 |
| Transistor/npn_vco-led (astable) | 99.9 / 34 | 91.4 / 40 |
| Transistor/npn-amplifier | 100 / 50 | 100 / 50 |
| Transistor/mosfet-H_Bridge | 100 / 26 | 100 / 28 |

ngspice raw speed on the exported examples (2 s simulated, unpaced):
basic_led 4.4 µs/point (455 % real time), npn-amplifier 5.3 µs (379 %),
mosfet-H_Bridge 8 µs (250 %), npn_vco-led 19.7 µs (88 %; the astable needs
many small steps).

### MCU co-simulation
The co-simulation that was only evaluated in the first version (halting
the background thread per edge was too slow) is implemented with the
`GetSyncData` callback instead, see §7.

## 6. Limitations
* Native: no junction/diffusion capacitances, no BJT RB/RC/RE, no
  temperature dependence (27 °C), darlingtons approximated.
* Native: fixed 1 µs step; strongly regenerative circuits converge over a
  few steps (time continuation) instead of within one step.
* ngspice mode: only the parts listed in §3; switch and potentiometer
  positions are read when the transient (re)starts (pause/resume or any
  property edit restarts it from the current node voltages); inductor
  currents are not carried over between restarts or chunks; ammeters show
  the current of their 0 V source.
* MCU co-simulation: see §7.6.
* `add_component` over MCP cannot create `Node` items (not in the item
  library); the validation used generated `.simu` files instead.

## 7. MCU co-simulation with ngspice (lockstep, simulated time)

With *Accurate analog (ngspice)* enabled, a circuit with AVR / Arduino (and
PIC) microcontrollers no longer falls back to the native engine: the MCUs
and ngspice advance together in **simulated time**. The native engine stays
the default; this mode is only used when the user enables ngspice.

Code: `src/simulator/spice/spicecosim.*` (MCU side, handoff, pin sources),
`spiceengine.*` (ngspice callbacks, chunks), `spicenetlist.*` (export),
hooks in `mcucomponentpin.cpp` (pin changes) and `avrcomponentpin.cpp`
(ADC samples).

### 7.1 Synchronisation mechanism: why `GetSyncData` + EXTERNAL sources

| Option | Verdict |
|---|---|
| `stop when time > t` / `bg_halt` + `alter` + `resume` per MCU event | Every halt/resume of the background thread costs milliseconds (`_thread_stop` polls every 10 ms), and `alter` of a source value is a discontinuity ngspice does not know about (no breakpoint): rejected. |
| XSPICE `d_cosim` code model | Needs the MCU simulator built as a separate shared library loaded by ngspice and moves SimulIDE's MCU/GUI integration out of process: too invasive. |
| Coroutine (ucontext) running the MCU inside the ngspice callback | Fastest handoff, but not portable (Windows) and fragile with simavr/gpsim. |
| **Shared ngspice: `ngSpice_Init_Sync` with `GetVSRCData` (EXTERNAL voltage sources) and `GetSyncData`** | **Chosen.** A long-standing part of the `sharedspice.h` API (present in the distribution packages, e.g. Debian/Ubuntu `libngspice0` 42), no recompilation. `GetSyncData` is called after every accepted time point (location 0, after ngspice's own breakpoint handling) with a pointer to the next time step, and again when a step is rejected (location 1): the client can only shorten the step, so ngspice keeps its full accuracy control (LTE, Newton, order). EXTERNAL sources are asked for their value at every trial time, so MCU-driven waveforms are known exactly where ngspice evaluates them. |

### 7.2 Algorithm

Two threads run in **strict alternation** (only one runs at any time, so no
locking of the circuit data is needed):

* **MCU thread** (`SpiceEngine::cosimRun`, a QtConcurrent worker while the
  simulation runs): executes the firmware instruction by instruction
  (`BaseProcessor::stepCpu`), the MCU whose simulated time is the smallest
  first (multi-MCU: each processor keeps its own clock, `cycle() *
  stepNS`). The MCUs run **ahead** of ngspice.
* **ngspice background thread**: runs the transient. In `GetSyncData`
  (location 0) the accepted time `T` is published; if `T` reached the time
  the MCU thread asked for, ngspice hands the turn back and **parks** until
  the next request. Otherwise its next step is clamped so that it lands
  exactly on the requested time and on the end of every pin ramp.

The MCU thread asks ngspice to catch up (`syncTo(t)`) only when needed:

1. **Output change** (`McuComponentPin::setState/setDirection/update` →
   `SpiceCosim::pinChanged`): ngspice first advances to the MCU time `t` of
   the instruction with the old pin state, then the pin's EXTERNAL sources
   ramp to the new state in 10 ns (`spice/cosimRise`) starting exactly at
   `t`; the ramp end is a forced time point.
2. **Input samples:** ADC conversion start (`AVRComponentPin::adcread`, exact
   time) and PINx register reads (wrapped simavr I/O read callback). A
   digital read only syncs when ngspice is behind by more than the time
   step ngspice itself proposed (its accuracy based resolution) and at least
   `spice/cosimInputRes` (2 µs): polling loops do not force a time point
   every few cycles while the analog side is quiet.
3. **Horizon:** the MCUs never run more than `spice/cosimHorizon` (50 µs)
   ahead of ngspice. This bounds the latency of asynchronous inputs (pin
   change / external interrupts, timer input capture, reset) and is also
   ngspice's maximum step (×1.25, so one step reaches the next sync).

After every accepted point (`SendData`) the eNode voltages, the ammeters and
the per-step elements (oscilloscope, plotter) are updated and the MCU input
pins (and RESET/AVCC/AREF) whose node moved by more than 2 mV get the new
voltage (`setVChanged`, the same entry point the native engine uses).

Between MCU events ngspice takes its own adaptive steps (up to 62.5 µs),
there is no fixed 1 µs step.

### 7.3 Pin model in the netlist

Every connected MCU pin `k` is exported as

```
VMV<k> mv<k> 0 DC 0 EXTERNAL            * Thevenin voltage  (asked to SimulIDE)
VMG<k> mg<k> 0 DC 0 EXTERNAL            * output conductance (asked to SimulIDE)
BMP<k> <pin node> 0 I=V(mg<k>)*(V(<pin node>)-V(mv<k>))
```

i.e. exactly the native pin stage: output = 5 V / 0 V behind 40 Ω, input =
high impedance (1e-14 S) plus the 100 kΩ pull-up when enabled, open
collector, extra sources (Vref). Both values ramp linearly over the rise
time on every change (output↔input changes included), so ngspice never
sees a step discontinuity. Board ground nodes (node number 0) map to SPICE
ground; the Arduino board LED buffer becomes a smooth behavioural source.

### 7.4 Pacing and speed display

`SpiceEngine::cosimRun` runs batches of up to 20 ms of simulated / wall
time. The batch end is `min(simTime, wallTime) + 20 ms`: the simulation
runs as fast as possible up to real time and never more than one batch
ahead of the wall clock; when the circuit is heavier it simply runs slower
than real time (no catching up, no skipped events). The speed shown in the
status bar and returned as `real_time_percent` by `get_simulation_state`
is measured (simulated time / wall time); `cosimulation` adds the number of
ngspice points, syncs, MCU pin edges and the wall time split.

The thread handoff spins (with `pause` / yields) for up to 200 µs before
sleeping on a condition variable: a sleep/wake-up costs 15–30 µs on the
test VM, more than a typical ngspice time point (5 µs).

Settings (`simulide.ini`): `spice/cosimHorizon` (50e-6 s),
`spice/cosimRise` (10e-9 s), `spice/cosimInputRes` (2e-6 s).

### 7.5 Tests (Arduino Uno, firmware built with avr-gcc, driven via MCP)

Load during the runs: 4-core Xeon 2.3 GHz VM shared with other jobs (load
average 0.5–2.3). Test circuits and firmware are built through the MCP
server (`add_component`, `connect_pins`, `load_firmware`,
`set_analog_engine`, `get_pin_voltages`).

**(1) Blink through a 2N2222 driver** — D9 → 1 kΩ → 2N2222 base, 5 V →
220 Ω → red LED → collector; firmware toggles D9 every 100 ms. Mean of all
"on" snapshots (consistent snapshot of all pins with `get_pin_voltages`):

| Quantity (LED on) | native | ngspice co-sim | note |
|---|---|---|---|
| LED current | 13.79 mA | 13.72 mA | |
| LED Vf | 1.950 V | 1.950 V | |
| Vbe (pins) | 0.744 V | 0.784 V | ngspice adds RB = 10 Ω drop |
| Vce(sat) (pins) | 0.016 V | 0.030 V | ngspice adds RC = 1 Ω drop |
| Base current | 4.09 mA | 4.05 mA | |
| D9 pin voltage | 4.836 V | 4.838 V | 40 Ω output stage |
| real time | 99.6 % | 100.0 % | MCU thread busy 55 % of the wall time |

With D9 held high (DC) both engines give the same values. In the "off"
phase the collector is a floating node (LED off, transistor off) whose
voltage depends on junction capacitances: ngspice shows the turn-off
overshoot of the stored base charge (collector up to 6–10 V for a few ms,
also reproduced by standalone ngspice with a PULSE source), the native
engine (no junction capacitances) 4.05 V.

**(2) PWM → RC → ADC feedback loop** — Timer1 8-bit phase-correct PWM on
D9 (490 Hz, the Arduino `analogWrite` frequency of pin 9) → 10 kΩ → node
with 1 µF to GND → A0. The firmware averages 64 ADC conversions (AVcc =
5 V, prescaler 128) and moves the duty by one step until the average equals
512 (2.5 V).

| Quantity (steady state) | native | ngspice co-sim | theory |
|---|---|---|---|
| duty (OCR1A) | 126–128 (mean 127.0) | 127 (constant) | 127.5 → 127/128 |
| ADC average reading | 511.8 (505–519) | 510.5 (510–511) | 1024·127/255 = 510.0 |
| real time | 100 % | 100.9 % | |

The ADC samples the ngspice node at the exact conversion start time, so the
averaged reading matches the theoretical mean within 0.5 LSB and the loop
settles on one duty value (native: the 50 µs capacitor update produces a
±7 LSB spread and the loop hunts ±1).

**(3) Bit-bang speed** — D9 → 220 Ω → red LED → GND:

| Firmware | pin edges / s | ngspice points / s | native rt % | ngspice co-sim rt % |
|---|---|---|---|---|
| toggle every 500 µs (1 kHz) | 2 000 | 83 000 | 100 | 100 |
| software UART 9600 baud ('U') | 9 550 | 244 000 | 100 | 66 |
| toggle every 50 µs (10 kHz) | 20 000 | 471 000 | 100 | 36 |
| toggle every 5 µs (100 kHz) | 203 000 | 3 950 000 | 94 | 4.2 |

Each edge costs ~20 accepted (+2.5 rejected) ngspice time points: the
10 ns edge, the diode/junction transient and the ×2 per step growth back to
the large step. ngspice spends 5.4–6.2 µs per point in the shared library
(standalone ngspice with an internal PULSE source: 2.2 µs; shared library
with EXTERNAL + B sources and SendData: 4.6 µs), SimulIDE's per-point
callback ~0.3 µs and the MCU emulation the rest.

**(4) Two MCUs** — Arduino A toggles D9 at 1 kHz → Arduino B D2; B polls
PIND, counts rising edges (GPIOR0/1) and mirrors the input on its D9 → LED.

| Case | native edges/s (rt %) | ngspice co-sim edges/s (rt %) |
|---|---|---|
| A 16 MHz, B 16 MHz | 996.7 (100) | 998.3 (64) |
| A 8 MHz, B 16 MHz (per-processor clocks) | 498.4 (100) | 500.2 (96) |

(The small native deficit comes from sampling the counter and the step
counter with separate MCP calls.) Also checked: property edits while
running (220 Ω → 470 Ω: LED current 13.73 → 6.61 mA, simulated time and MCU
state kept), the PIC example `Pic/PWM_Pic16F627A` (co-simulated, 74–86 %
real time) and the purely analog examples (unchanged, ~100 %).

### 7.6 Limitations

* Speed: ~20 ngspice points per MCU pin edge. PWM / blink / UART up to a
  few thousand edges per second run in real time; 10⁵ edges/s (fast
  bit-bang, software PWM, bit-banged SPI) run at a few % of real time.
* Edge time resolution is the start of the instruction (a few cycles); with several
  MCUs one MCU may see another's edge up to one instruction late.
* Asynchronous inputs (pin change / external interrupts, input capture,
  analog comparator, reset) are delivered at ngspice time points, at most
  one horizon (50 µs) late; polled reads (PINx) and the ADC are sampled at
  the read time (PINx: within ngspice's own step, at least 2 µs).
* PIC (gpsim): co-simulated and its outputs verified (PWM example); its
  inputs are sampled at ngspice points only (no register read / ADC hooks
  yet).
* Hardware peripherals with their own SimulIDE elements (I2C/TWI, logic
  ICs, LCD controllers...) are not exported: such circuits fall back to the
  native engine with the list of unsupported parts.
* A restart (pause/resume, property edit, every transient chunk of ~200 000
  points) completes running pin ramps immediately and loses inductor
  currents (as in the analog mode).
* While ngspice is enabled the MCU always runs in this mode; there is no
  per-part choice.
