# OPEN-SOURCE ESP-32 UDA1334A MUSIC PLAYER

## 1. Project Overview

The Open-Source ESP-32 UDA1334A Music Player is a standalone digital audio player based on the ESP32 microcontroller platform.

The system is designed to read MP3 audio files from a microSD card, decode the audio using the ESP32, and transmit the resulting digital audio stream through an I²S interface to a UDA1334A digital-to-analog converter (DAC).

A 128×64 SSD1306 OLED display provides playback information, while four physical push buttons provide playback control. A potentiometer is used as the hardware volume control.

The project is being released as an open-source implementation intended to provide a reproducible foundation for developing custom ESP32-based music players and related embedded audio systems.

This repository currently represents a **Test Alpha build**. The implementation is functional, but further testing, optimization, and hardware validation are required before it can be considered stable or production-ready.

---

## 2. Project Objectives

The primary objectives of this project are:

* To implement a standalone MP3 player using an ESP32.
* To use an external I²S DAC for digital-to-analog audio conversion.
* To provide local audio storage through a microSD card.
* To provide a simple hardware-based user interface.
* To provide playback information through an OLED display.
* To maintain an openly accessible implementation that can be modified and extended.
* To provide a reference implementation for developers interested in ESP32-based digital audio systems.

The project is intentionally open source so that developers can use the implementation as a starting point for their own hardware and firmware designs.

---

## 3. Current Development Status

**Release Status: Test Alpha**

The current implementation has been assembled and tested as a functional MP3 player.

The firmware currently implements:

* MP3 file detection and loading from microSD
* Playlist generation
* Alphabetical playlist sorting
* MP3 playback
* Play/pause control
* Previous and next track selection
* Loop mode
* Hardware volume control
* Playback progress tracking
* OLED status display
* I²S audio output
* FreeRTOS-based audio task execution
* Mutex-based protection of audio operations

The system remains under active development.

The current Alpha release may contain issues related to audio stability, memory utilization, buffering, SD card behavior, and peripheral compatibility.

---

## 4. System Architecture

The system consists of five primary functional subsystems:

1. ESP32 controller
2. microSD storage subsystem
3. OLED user-interface subsystem
4. UDA1334A I²S DAC subsystem
5. Physical control subsystem

The general signal flow is:

```text
microSD Card
     |
     | SPI
     v
+-----------+
|   ESP32   |
|           |
| MP3 Decode|
| Playback  |
+-----+-----+
      |
      | I²S
      v
+-------------+
|  UDA1334A   |
|     DAC     |
+------+------+ 
       |
       | Analog Audio
       v
 Audio Amplifier /
 Headphone System
```

The ESP32 also communicates with the OLED through I²C and receives user input through the push buttons and potentiometer.

---

## 5. Hardware Requirements

### 5.1 Main Components

| Component                               |    Quantity | Function                     |
| --------------------------------------- | ----------: | ---------------------------- |
| ESP32 development board                 |           1 | Main microcontroller         |
| UDA1334A I²S DAC                        |           1 | Digital-to-analog conversion |
| MicroSD card module                     |           1 | Storage interface            |
| MicroSD card                            |           1 | MP3 storage                  |
| SSD1306 128×64 OLED                     |           1 | User interface               |
| Momentary push buttons                  |           4 | Playback controls            |
| 10 kΩ potentiometer                     |           1 | Volume control               |
| Audio amplifier or powered audio output |           1 | Audio reproduction           |
| Connecting wires                        | As required | Electrical connections       |
| Breadboard or custom PCB                |    Optional | Hardware assembly            |

### 5.2 Optional Components

Depending on the intended implementation, the following components may be added:

* Headphone amplifier
* Speaker amplifier
* Speakers
* Rotary encoder
* Battery
* Battery management circuit
* Power regulation circuitry
* Custom PCB
* Enclosure

The current firmware does not require these optional components.

---

## 6. ESP32 Pin Configuration

The following GPIO configuration corresponds to the current Alpha firmware.

### 6.1 SSD1306 OLED

| OLED Signal | ESP32 GPIO | Interface |
| ----------- | ---------: | --------- |
| SDA         |    GPIO 21 | I²C       |
| SCL         |    GPIO 22 | I²C       |

The firmware initializes the I²C interface using:

```cpp
Wire.begin(21, 22);
```

The configured OLED address is:

```text
0x3C
```

---

### 6.2 MicroSD Card

The microSD card is connected through the ESP32 SPI interface.

| SD Signal | ESP32 GPIO | Interface |
| --------- | ---------: | --------- |
| CS        |     GPIO 5 | SPI       |
| MOSI      |    GPIO 23 | SPI       |
| MISO      |    GPIO 19 | SPI       |
| SCK       |    GPIO 18 | SPI       |

The firmware initializes the SPI interface using:

```cpp
SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
```

The SD chip-select pin is:

```cpp
#define SD_CS 5
```

---

### 6.3 UDA1334A I²S DAC

| DAC Signal | ESP32 GPIO | Function         |
| ---------- | ---------: | ---------------- |
| DIN        |    GPIO 25 | I²S data         |
| LRC / WS   |    GPIO 26 | Left/right clock |
| BCLK       |    GPIO 27 | Bit clock        |

The firmware configures the DAC interface using:

```cpp
audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
```

The upstream ESP32-audioI2S project also documents UDA1334A compatibility and uses the same DOUT, BCLK, and LRC signal arrangement in its examples.

---

### 6.4 Push Buttons

All buttons use the ESP32 internal pull-up resistors.

| Function   | ESP32 GPIO |
| ---------- | ---------: |
| Previous   |    GPIO 32 |
| Next       |    GPIO 33 |
| Play/Pause |    GPIO 12 |
| Loop       |    GPIO 14 |

Each button is connected between its corresponding GPIO and ground.

The firmware configures the buttons using:

```cpp
pinMode(BTN_PREV, INPUT_PULLUP);
pinMode(BTN_NEXT, INPUT_PULLUP);
pinMode(BTN_PLAY, INPUT_PULLUP);
pinMode(BTN_LOOP, INPUT_PULLUP);
```

Therefore:

```text
Released = HIGH
Pressed  = LOW
```

---

### 6.5 Volume Potentiometer

The potentiometer wiper is connected to:

```text
GPIO 34
```

The firmware configures the ESP32 ADC with 12-bit resolution:

```cpp
analogReadResolution(12);
```

The resulting ADC range is:

```text
0–4095
```

The firmware maps this range to a software volume range of:

```text
0–100 %
```

---

## 7. Software Architecture

The firmware is written in C++ using the Arduino ESP32 framework.

The application is divided into several functional areas:

### Audio Processing

The `Audio` object is responsible for MP3 decoding and I²S audio output through the ESP32-audioI2S library.

### Storage

The ESP32 `SD` and `FS` interfaces are used to access MP3 files stored on the microSD card.

### User Interface

The SSD1306 OLED is controlled using:

* Adafruit GFX
* Adafruit SSD1306
* Wire

### Input Handling

The four physical buttons use software debouncing and the ESP32 internal pull-up resistors.

The potentiometer is sampled periodically and averaged using multiple ADC readings to reduce fluctuations.

### Task Management

The firmware creates a dedicated FreeRTOS task for continuous audio processing.

Audio access is protected using a recursive mutex to prevent concurrent access from the main application loop and the audio task.

---

## 8. Software Requirements

The following versions define the intended software environment for the current Alpha build.

| Dependency           |           Version | Requirement                                 |
| -------------------- | ----------------: | ------------------------------------------- |
| Arduino ESP32 Core   |        **2.0.17** | Required for the tested Alpha configuration |
| ESP32-audioI2S       |         **2.0.0** | Required for the tested Alpha configuration |
| Adafruit GFX Library | Latest compatible | Recommended                                 |
| Adafruit SSD1306     | Latest compatible | Recommended                                 |

The ESP32 Arduino project has subsequently introduced the 3.x series and provides a migration guide from 2.x to 3.x. The Alpha build therefore intentionally remains on the tested 2.0.17 environment rather than automatically adopting a newer core.

---

## 9. ESP32-audioI2S Dependency

This project uses:

**ESP32-audioI2S version 2.0.0**

Repository:

[ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S?utm_source=chatgpt.com)

The dependency is intentionally pinned to version 2.0.0 for the Alpha build.

The upstream library provides the audio decoding and I²S output functionality required by this project and documents support for the UDA1334A DAC.

Newer versions of the library should not be assumed to be binary or API compatible with this implementation. Updating the dependency should therefore be treated as a separate development task requiring regression testing.

---

## 10. Arduino ESP32 Dependency

This project uses:

**Arduino-ESP32 version 2.0.17**

Repository:

[Arduino-ESP32](https://github.com/espressif/arduino-esp32?utm_source=chatgpt.com)

Version 2.0.17 is the tested Arduino ESP32 environment for the current Alpha firmware.

The project should not be considered validated against the newer Arduino ESP32 3.x API without additional testing and possible source modifications.

---

## 11. Installation

### 11.1 Arduino IDE

Install a compatible version of the Arduino IDE.

Install the ESP32 board package through the Arduino Board Manager.

Select:

```text
ESP32 Arduino Core 2.0.17
```

Select the appropriate ESP32 development board corresponding to the hardware being used.

---

### 11.2 Required Libraries

Install the following libraries:

```text
ESP32-audioI2S 2.0.0
Adafruit GFX Library
Adafruit SSD1306
```

The ESP32 core itself provides the following interfaces used by the application:

```cpp
Arduino.h
FS.h
SD.h
SPI.h
Wire.h
```

---

## 12. microSD Card Configuration

The firmware scans the root directory of the microSD card for MP3 files.

Example:

```text
/
├── TRACK01.mp3
├── TRACK02.mp3
├── TRACK03.mp3
└── TRACK04.mp3
```

Only files with the `.mp3` extension are added to the playlist.

File extension detection is performed without case sensitivity.

The current firmware supports a maximum of:

```text
50 tracks
```

The playlist entries are sorted alphabetically before playback begins.

---

## 13. User Controls

| Control       | Operation                   |
| ------------- | --------------------------- |
| Previous      | Select previous track       |
| Next          | Select next track           |
| Play/Pause    | Toggle playback state       |
| Loop          | Enable or disable loop mode |
| Potentiometer | Adjust playback volume      |

When loop mode is disabled, playback automatically advances to the next track after the current track ends.

When loop mode is enabled, the current track is restarted after reaching the end of the file.

---

## 14. OLED Interface

The OLED displays the following information:

* Current track
* Playback time
* Total track duration
* Playback progress
* Current volume
* Playback state
* Loop state
* Playlist position

The display is updated periodically rather than continuously in order to reduce unnecessary processing and I²C traffic.

---

## 15. Audio Processing

The audio subsystem uses the ESP32-audioI2S library to process MP3 files and provide I²S output.

The current implementation uses:

```text
I²S BCLK = GPIO 27
I²S LRC  = GPIO 26
I²S DOUT = GPIO 25
```

The UDA1334A receives the resulting I²S stream and performs digital-to-analog conversion.

The analog output from the DAC must subsequently be connected to a suitable audio amplifier, headphone amplifier, or other compatible audio output stage.

The UDA1334A itself is a DAC and should not be treated as a speaker power amplifier.

---

## 16. FreeRTOS Audio Task

The firmware uses a dedicated FreeRTOS task to continuously execute:

```cpp
audio.loop();
```

This task is pinned to a processor core and operates independently from the primary application loop.

Audio access is protected using a recursive mutex:

```cpp
SemaphoreHandle_t audioMutex;
```

This prevents simultaneous access to the audio object from multiple execution contexts.

This architecture was introduced as part of the development and debugging process to improve audio task handling and reduce conflicts between playback operations and the user-interface logic.

---

## 17. Memory and Buffer Considerations

The ESP32 is a resource-constrained embedded platform, and audio decoding requires continuous memory and processing resources.

The current Alpha implementation has not been exhaustively tested against all combinations of:

* MP3 bitrate
* MP3 sample rate
* MP3 file size
* SD card performance
* Playlist size
* Continuous playback duration
* ESP32 board variants
* Different memory conditions

Potential problems may therefore include:

* Audio buffer underruns
* Audio interruptions
* Memory fragmentation
* Insufficient available memory
* Playback termination
* Watchdog resets
* SD card read delays
* Unexpected audio output behavior

These limitations are particularly relevant to the current Alpha status and should be considered when modifying the audio pipeline.

---

## 18. Known Limitations

The current implementation has the following known limitations:

* Maximum playlist size is 50 tracks.
* MP3 files are currently scanned from the SD card root directory.
* Folder-based music organization is not currently implemented.
* Metadata presentation is limited.
* Album artwork is not implemented.
* Shuffle playback is not implemented.
* Resume playback is not implemented.
* The firmware has not been exhaustively tested with all ESP32 variants.
* Audio memory and buffer behavior may require further optimization.
* The current dependency versions are intentionally pinned.
* Newer ESP32 Arduino Core and ESP32-audioI2S releases have not been validated for this Alpha implementation.

---

## 19. Troubleshooting

### No OLED output

Check:

```text
SDA → GPIO 21
SCL → GPIO 22
```

Also verify that the display uses the configured I²C address:

```text
0x3C
```

---

### SD card initialization failure

Check:

```text
CS   → GPIO 5
MOSI → GPIO 23
MISO → GPIO 19
SCK  → GPIO 18
```

Verify the SD card filesystem and ensure the card can be read correctly.

---

### No audio output

Check the I²S connections:

```text
UDA1334A DIN  → GPIO 25
UDA1334A LRC  → GPIO 26
UDA1334A BCLK → GPIO 27
```

Also verify the downstream audio amplifier or headphone stage.

The UDA1334A DAC provides analog audio output but does not provide the power required to directly drive conventional passive speakers.

---

### Audio interruptions

Possible causes include:

* SD card read latency
* Insufficient audio buffering
* Memory pressure
* Unsupported or problematic MP3 encoding
* CPU contention
* Incorrect dependency versions

First verify that the system is running the specified Alpha dependency versions.

---

## 20. Development and Testing

This project has undergone iterative development involving hardware integration, software debugging, audio pipeline testing, SD card handling, display integration, input handling, task management, and memory-related debugging.

The Alpha release represents the current functional state of that development process rather than a finalized product.

Testing contributions are particularly useful for identifying problems that cannot be reproduced on the original development hardware.

When reporting a problem, include:

```text
ESP32 board:
Arduino ESP32 Core version:
ESP32-audioI2S version:
SD card capacity:
SD card filesystem:
MP3 bitrate:
MP3 sample rate:
MP3 file size:
Problem description:
Serial Monitor output:
```

---

## 21. Development Roadmap

The following areas are candidates for future development:

* Improved audio buffering
* Memory optimization
* Improved SD card error handling
* Improved playback recovery
* Directory and folder support
* ID3 metadata handling
* Artist and album information
* Album artwork
* Shuffle playback
* Resume playback
* Additional audio formats
* Improved OLED interface
* Rotary encoder support
* Battery-powered operation
* Power-management implementation
* Custom PCB design
* Enclosure design
* Broader hardware compatibility testing
* Stable release

The roadmap is subject to change as testing progresses.

---

## 22. Open-Source Development

The firmware is being released publicly so that the implementation can be studied, modified, extended, and reused according to the applicable project and dependency licenses.

The project is intended to serve as a practical starting point for developers building their own ESP32-based audio hardware.

Contributions may include:

* Bug reports
* Hardware testing
* Firmware improvements
* Memory optimization
* Audio stability improvements
* Documentation improvements
* Additional hardware support
* User-interface improvements
* Feature implementations

Pull requests should document significant changes to the audio subsystem, hardware interfaces, task scheduling, or dependency requirements.

---

## 23. Developer

**Developed by:** Shubhayu Kundu

**GitHub:** [Shubhayu15](https://github.com/Shubhayu15?utm_source=chatgpt.com)

The project was developed as an independent hardware and software implementation with emphasis on experimentation, debugging, and open access to the resulting firmware.

---

## 24. Third-Party Dependencies

This project uses third-party open-source software.

### ESP32-audioI2S

Repository:

[ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S?utm_source=chatgpt.com)

Tested version:

```text
2.0.0
```

The upstream repository is licensed under GPL-3.0. Refer to the upstream repository for the complete license and associated terms.

### Arduino-ESP32

Repository:

[Arduino-ESP32](https://github.com/espressif/arduino-esp32?utm_source=chatgpt.com)

Tested version:

```text
2.0.17
```

The Arduino-ESP32 project is distributed under LGPL-2.1. Refer to the upstream repository for the complete license and associated terms.

### Adafruit Libraries

The project also uses:

* Adafruit GFX Library
* Adafruit SSD1306

Their respective licenses and terms apply to the versions distributed through the Arduino ecosystem.

---

## 25. Project Status Summary

| Area                           | Status            |
| ------------------------------ | ----------------- |
| ESP32 control                  | Functional        |
| microSD storage                | Functional        |
| MP3 file scanning              | Functional        |
| Playlist generation            | Functional        |
| MP3 playback                   | Functional        |
| I²S output                     | Functional        |
| UDA1334A integration           | Functional        |
| OLED interface                 | Functional        |
| Button controls                | Functional        |
| Volume control                 | Functional        |
| Loop mode                      | Functional        |
| FreeRTOS audio task            | Implemented       |
| Memory optimization            | Under development |
| Audio stability testing        | Under development |
| Hardware compatibility testing | Under development |
| Production release             | Not yet available |

---

## 26. License

The license for this project should be defined in the repository separately from the licenses of its third-party dependencies.

If a `LICENSE` file is present in the repository, it should be treated as the authoritative license for the project's original source code.

Third-party libraries remain subject to their respective licenses and terms.

---

## 27. Disclaimer

This project is provided as an experimental Alpha implementation.

It is intended for development, educational, and experimental purposes. Hardware and firmware modifications should be performed with appropriate care.

The current implementation has not undergone the level of validation required for a production audio device.

Users building the project should verify electrical compatibility between the ESP32, DAC, SD module, display, power supply, amplifier, and other connected hardware before operation.

---

## 28. Current Alpha Release

**Project:** Open-Source ESP-32 UDA1334A Music Player

**Release:** Test Alpha

**Primary Platform:** ESP32

**Audio Interface:** I²S

**DAC:** UDA1334A

**Storage:** microSD

**Display:** SSD1306 128×64 OLED

**Firmware Framework:** Arduino ESP32

**Tested Arduino ESP32 Core:** 2.0.17

**Tested ESP32-audioI2S:** 2.0.0

**Development Status:** Active Development
