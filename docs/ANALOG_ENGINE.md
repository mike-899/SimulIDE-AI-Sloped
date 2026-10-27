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
  osciloscopio, brillo de LEDs). Si el circuito tiene un microcontrolador (u
  otras piezas no soportadas) se usa automáticamente el motor nativo.
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
| Works with MCUs (AVR/PIC/Arduino) | no (see §5) | **yes** |
| Accuracy | reference | matches ngspice to < 0.1 % in DC (§6) |
| Time resolution | adaptive (ns) | fixed 1 µs step, capacitors updated every 50 µs |
| Real-time cost | 4-20 µs per time point | event driven, ~free when nothing changes |
| Dependencies | libngspice at runtime | none |

The native engine was improved first because it is what every circuit with a
microcontroller (the main SimulIDE use case) runs on. ngspice was added as an
optional "accurate analog" mode for purely analog circuits; it is loaded with
`QLibrary` at runtime, so SimulIDE builds and runs without it.

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
  Anything else (logic, op-amps, MCUs, relays, current sources) makes the
  export fail with the list of unsupported parts, and the native engine is
  used.
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
  exported netlist). `spice/maxstep` (default 20e-6 s) sets the max step.

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

### MCU co-simulation evaluation (not implemented)
ngspice offers `ngSpice_Init_Sync` with `GetVSRCData` (EXTERNAL voltage
sources whose value is asked at every time point) and `GetSyncData` (the
caller can cut the time step), which would let MCU pins drive ngspice in
lockstep. With 4–20 µs per accepted time point, a circuit needs fewer than
~50 000–200 000 points per simulated second to stay real time. Every MCU pin
edge forces a time point (plus the ones around it for the edge), and the
MCU must be advanced inside the ngspice callback. PWM/LED circuits
(kHz edges) would be fine, but bit-banged buses, UART at 115 kbaud or
software PWM give 10⁵–10⁶ edges/s and fall to 5–50 % real time. Halting and
resuming the background thread per edge (`bg_halt`/`bg_resume`) costs
milliseconds and is not usable. Therefore SPICE mode is disabled when a MCU
is present; the improved native engine covers that case.

## 6. Limitations
* Native: no junction/diffusion capacitances, no BJT RB/RC/RE, no
  temperature dependence (27 °C), darlingtons approximated.
* Native: fixed 1 µs step; strongly regenerative circuits converge over a
  few steps (time continuation) instead of within one step.
* ngspice mode: only the parts listed in §3; switch and potentiometer
  positions are read when the transient (re)starts (pause/resume or any
  property edit restarts it from the current node voltages); inductor
  currents are not carried over between restarts or 2 s chunks; no MCUs; ammeters show the
  current of their 0 V source.
* `add_component` over MCP cannot create `Node` items (not in the item
  library); the validation used generated `.simu` files instead.
