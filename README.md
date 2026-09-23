[Русская версия](./README_RU.md)

# RP2040 HDA-Link demo project

## About
This project demonstrates the feasibility of using HDA-Link audio codecs—such as the ALC889—in conjunction with the RP2040 microcontroller and its PIO coprocessors.
Four state machines (SMs), numbered 0 through 3, are used to implement the HDA-Link interface. 

The SMs are utilized as follows:
| SM | assignment |
|----|------------|
| 0  | **DOUT** output|
| 1  | **SYNC** output|
| 2  | **DIN** input, and **BCLK** and "start-of-frame" (SoF) outputs|
| 3  | detection of the codec's address request and address assignment|

After the codec address request and assignment phase completes (upon return from the `hdal_codec_reset()` function), **SM3** can be stopped and used for other purposes.

**RP2040 pin assignments:**
| RP2040 Pin | Signal      | ALC889 Pin | Note  |
|------------|-------------|------------|-------|
|     4      | SoF         |     -      | Debug |
|     5      | BCLK        |     6      |       |
|     6      | DOUT        |     5      |       |
|     7      | SYNC        |    10      |       |
|     8      | SDI         |     8      |       |
|     9      | RST         |    11      |       |
|    13      | DBGPIN1     |            | Debug |
|    14      | DBGPIN2     |            | Debug |
|    15      | DBGPIN3     |            | Debug |
|    16      | DBGPIN4     |            | Debug |
|    17      | DBGPIN5     |            | Debug |
|    20      | UART_TX_PIN |            |       |
|    21      | UART_RX_PIN |            |       |

The project supports all sample rates compatible with the codec: 44.1, 48, 88.2, 96, 176.4, and 192 kHz. 

The maximum rate — 192 kHz (48000 * 4) — is used by default.
It defined in main.c:
```c
sbm = hdac_find_samplerate_base_mul(48000 * 4);
```
It may be change with other valid argument.
 
As a demonstration, the project utilizes all 10 audio outputs to generate independent sine waves. Frequencies are defined in the `gen_freq[10]` array. For the 192 kHz setting, channels 0 through 7 follow a geometric progression with a multiplier of 1.5, ranging from 880 Hz (0.004583333333 * 192000) to 15035.6 Hz; channels 8 and 9 are set to 63936 Hz and 76800 Hz, respectively.

## Build from source
Building from source follows the same procedure as for most RP2040 projects.

Clone and build:

```bash
git clone https://github.com/VictorAgarkov/rp2040-alc889-sinegen-01
cd rp2040-alc889-sinegen-01
cmake . -B build
make -j 8 -C build
```
The firmware files (e.g., *rp2040-alc889-sinegen-01.uf2*) will be generated in the `build` directory.