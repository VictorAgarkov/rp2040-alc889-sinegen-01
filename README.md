[Русская версия](./README_RU.md)

# RP2040 HDA-Link demo project

## About
This project demonstrates the feasibility of using HDA-Link audio codecs — such as the **ALC889** — in conjunction with the RP2040 microcontroller and its PIO coprocessors.
Four state machines (SMs), numbered 0 through 3, are used to implement the HDA-Link interface. 

The SMs are utilized as follows:
| SM | assignment                                                     |
|----|----------------------------------------------------------------|
| 0  | **DOUT** output                                                |
| 1  | **SYNC** output                                                |
| 2  | **DIN** input, and **BCLK** and "start-of-frame" (SoF) outputs |
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

By default, the maximum possible sample rate of 192 kHz is used. This is defined in **config.h**:
```c
#define HDA_SAMPLERATE 192000
```
It can be changed to any sample rate (in Hertz) supported by the codec.

As a demonstration, the project uses all 10 audio outputs to generate independent sine waves. The frequencies are specified in the `gen_freq[10]` array. For the 192 kHz mode, the frequencies for channels 0 through 7 follow a geometric progression with a multiplier of 1.5—ranging from 880 Hz (0.004583333333 × 192,000) to 15,035.6 Hz—while the frequencies for channels 8 and 9 are 63,936 Hz and 76,800 Hz, respectively.

Data received from the ADC is not utilized.

## How it works

Data exchange between the PIO and the microcontroller's RAM occurs via several DMA channels. Two DMA channels are allocated for each line (SDIN, SDOUT, and SYNC) to implement a double-buffering scheme (often referred to as "ping-pong" buffering). A total of 6 DMA channels are used.

Audio data received from the ADC via the SDIN line is stored in the `HDA_din_buff` double buffer; it is then unpacked in software into a set of 32-bit samples within the `g_SamplesInBuff32` single buffer. Upon successful unpacking, the global variable `g_InputStreamReady` is set to a non-zero value, which should be monitored in the main loop. Once this value is detected, the data received from the ADC can be processed.

To handle audio data for DAC output, the state of the `g_SamplesOutBuff32_half_empty` variable must be monitored. Once it takes on a non-negative value, the 32-bit DAC samples must be placed into one of the halves of the `g_SamplesOutBuff32` buffer (the first half if `g_SamplesOutBuff32_half_empty` is 0, or the second if it is 1), and the `g_SamplesOutBuff32_half_empty` variable must be reset (or rather, set to -1). Subsequently, in the next frame, the data is packed via software into the `HDA_dout_buff` double buffer and transmitted externally.

Two separate buffers—`HDA_sync_buff_actual` and `HDA_sync_buff_empty`—are used for SYNC output. Their contents are updated during initialization and remain static during normal operation. The `HDA_sync_buff_actual` buffer holds information regarding the audio streams (stream IDs and start times) for the codec. The `HDA_sync_buff_empty` buffer contains an "empty" signal (devoid of audio stream information) and is used to insert empty frames when operating at sample rates that are multiples of 44.1 kHz.

Software-based handling of DMA interrupts—including the packing and unpacking of audio data—is performed on core #1, thereby freeing up resources on the primary core (#0) for signal processing tasks. 

## Usage

First, determine the number of DAC and ADC channels and the sampling rate, then set the appropriate values ​​in **config.h**:
```c
#define HDA_DAC_NUM    5      // number of stereo DACs
#define HDA_ADC_NUM    3      // number of stereo ADCs
#define HDA_SAMPLERATE 192000 // working sample rate
```
To output signals to the DAC: periodically poll the `g_SamplesOutBuff32_half_empty` variable in the main loop; if it is non-negative, prepare the next batch (frame) of audio data:

```c
if(g_SamplesOutBuff32_half_empty >= 0)
{
	// calculate which buffer half needs the fresh audio data
	int32_t *p32 = g_SamplesOutBuff32 + ARRAYSIZE(g_SamplesOutBuff32) / 2 * g_SamplesOutBuff32_half_empty; 

	// generate output
	make_next_sine(p32); 

	g_SamplesOutBuff32_half_empty = -1;
}
```
To receive signals from the ADC: poll the `g_InputStreamReady` variable, and once it becomes non-zero, process the received data:

```c
if(g_InputStreamReady)
{
	make_something_with_ADC_samples(g_SamplesInBuff32); 
	g_InputStreamReady = 0;
}
```
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