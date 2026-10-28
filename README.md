# SimulIDE CE

Electronic Circuit Simulator Community Edition


SimulIDE Community Edition is a fork of SimulIDE software, a simple real time electronic circuit simulator, intended for hobbyist or students to learn and experiment with simple electronic circuits and microcontrollers, supporting PIC, AVR and Arduino. This is not an accurate simulator for circuit analysis. It aims to be fast, simple and easy to use, so this means simple and not very accurate electronic models and limited features. SimulIDE also features a code Editor and Debugger for GcBasic, Arduino, PIC asm and AVR asm. Editor/Debugger is still in its firsts stages of development, with basic functionalities, but it is possible to write, compile and perform basic debugging with breakpoints, watch registers and global variables. PIC simulation is provided by gpsim and avr simulation by simavr.


## Building SimulIDE:

Build dependencies:

 - Qt 6 dev packages (Qt >= 6.2)
 - Qt6 Core, Gui, Widgets, Xml, Concurrent, Network
 - Qt6 Svg
 - Qt6 Multimedia
 - Qt6 SerialPort
 - Qt6 Qml (QtDeclarative, used for the scripted "Function" logic component)
 - Qt6 tools (lrelease) and qmake6
 - libelf-dev
 - gcc-avr
 - avr-libc

 
On Ubuntu/Debian (24.04+):

```
sudo apt install qt6-base-dev qt6-multimedia-dev qt6-serialport-dev qt6-svg-dev \
    qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools libelf-dev
```

On Arch Linux: `pacman -S qt6-base qt6-multimedia qt6-serialport qt6-svg qt6-declarative qt6-tools libelf`

Once installed:

```
mkdir build
cd build
qmake6 ..
make
```

In the folder `build/release/SimulIDE_x.x.x` you will find the executable and all the files needed to run SimulIDE. SimulIDE executable is in bin folder. No need for installation, place SimulIDE folder wherever you want and run the executable.

## Breaking change: Arduino Mega pin layout

The pins of the Arduino Mega board were moved to sit exactly on the header holes of the realistic board art (see `changelog.txt`). Pin names and MCU mappings are unchanged, but circuits saved with an older Mega will load with the wires on the moved pins displaced; re-route those wires.

## Social:

Here on [this Discord channel](https://discord.gg/nTRbdpY) you may ask for help or discuss different SimulIDE related matters.

![Discord](https://img.shields.io/discord/697035358088462346) 

![GitHub issues](https://img.shields.io/github/issues-raw/SimulIDE/SimulIDE)
![GitHub pull requests](https://img.shields.io/github/issues-pr/SimulIDE/SimulIDE)
![GitHub forks](https://img.shields.io/github/forks/SimulIDE/SimulIDE)


![GitHub](https://img.shields.io/github/license/SimulIDE/SimulIDE)

## Disclaimer:

SimulIDE software is originally developed by [Santiago González Rodriguez](santigoro@gmail.com) and he is still the core developer of the software. This GitHub organization/repositories and the corresponding Discord channels, are community driven effort to make the software more accessible to users and developers. If you enjoy  the software please support the original developer [here on Patreon](https://www.patreon.com/simulide).
