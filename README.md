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

Data exchange between the PIO and the microcontroller's RAM is handled via multiple DMA channels. Two DMA channels are allocated for each line (SDIN, SDOUT, and SYNC) to support double buffering (often referred to as "ping-pong" buffering), resulting in a total of six DMA channels.

Audio data received from the ADC via the SDIN line enters the `HDA_din_buff` double buffer; it is then unpacked in software into a set of 32-bit samples stored in the `g_SamplesInBuff32` double buffer. Upon successful unpacking, the global pointer `g_SamplesInBuff32_ready` is set to a non-zero value indicating the start of the data chunk—a value that must be monitored within the main loop. Once this value is detected, the data received from the ADC can be processed, and the pointer itself is reset to zero.

To handle audio data for DAC output, the state of the `g_SamplesOutBuff32_empty` pointer must be monitored; this pointer is either NULL or points to a section of the `g_SamplesOutBuff32` buffer. When it takes on a non-zero value, the 32-bit DAC samples must be written to that address, and the pointer reset to zero. In the subsequent frame, the data is packed in software into the `HDA_dout_buff` double buffer and transmitted externally.

All 32-bit sample values ​​are signed (int32_t) and left-aligned. The least significant 8 or 16 bits (depending on the codec mode) are either discarded (for DAC output) or padded with zeros (for ADC input).

Two separate buffers—`HDA_sync_buff_actual` and `HDA_sync_buff_empty`—are used for SYNC output. Their contents are updated during initialization and remain static during normal operation. The `HDA_sync_buff_actual` buffer holds information about audio streams (their IDs and start times) for the codec. The `HDA_sync_buff_empty` buffer contains an "empty" signal (devoid of audio stream data) and is used to insert empty frames when operating at sample rates that are multiples of 44.1 kHz.

Software-based DMA interrupt processing—including the packing and unpacking of audio data—is handled by core #1, leaving the resources of the primary core (#0) available for signal processing tasks.

## Usage

First, determine the number of DAC and ADC channels and the sample rate, then set the appropriate values ​​in **config.h**:
```c
#define HDA_DAC_NUM    5      // number of stereo DACs
#define HDA_ADC_NUM    3      // number of stereo ADCs
#define HDA_SAMPLERATE 192000 // working sample rate
```
To output signals to the DAC: periodically poll the `g_SamplesOutBuff32_empty` variable within the main loop; if it is not NULL, prepare the next batch (frame) of audio data:

```c
if(g_SamplesOutBuff32_empty)
{
	// DAC buffer is empty – preparing the next batch of samples
	int32_t *dst = (int32_t*)g_SamplesOutBuff32_empty;
	g_SamplesOutBuff32_empty = NULL;

	//  generate output
	make_next_sine(dst); 
}
```
To receive the signal from the ADC: poll the variable `g_SamplesInBuff32_ready`, and as soon as it becomes non-zero, process the received data.

```c
if(g_SamplesInBuff32_ready)
{
	// Process the data received from the ADC here
	int32_t *src = (int32_t*)g_SamplesInBuff32_ready;
	g_SamplesInBuff32_ready = NULL;
}
```

Clone and build:

```bash
git clone https://github.com/VictorAgarkov/rp2040-alc889-sinegen-01
cd rp2040-alc889-sinegen-01
cmake . -B build
make -j 8 -C build
```
The firmware files (e.g., *rp2040-alc889-sinegen-01.uf2*) will be generated in the `build` directory.