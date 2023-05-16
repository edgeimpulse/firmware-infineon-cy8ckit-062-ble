# EdgeImpulse Firmware for Infineon PSoC63 (CY8CKIT-062-BLE)

Edge Impulse enables developers to create the next generation of intelligent device solutions with embedded machine learning. This repository contains the Edge Impulse firmware for Infineon PSoC62 BLE Pioneer Kit with E-ink Display kit. This device supports all of Edge Impulse's device features, including ingestion, remote management and inferencing.

## Introduction

This project supports:
* Data ingestion using the [EdgeImpulse Studio](https://studio.edgeimpulse.com)
  * Movement sampling from the Bosch BMI160 Inertial sensor on the EPD hat
  * Audio sampling using the PDM Microphone on the EPD hat
  * Temperature sampling using the Environmental sensor on the EPD hat
* Live inference with all sensors
  * Sensor Fusion using Inertial and Environmental sensor together
* Storing samples on the external NOR Flash using the QSPI inteface

Project dependencies:
* PSoC6 io_retarget library for UART interface
* PSoC6 serial-flash library for QSPI interface
* PSoC6 I2C library for hardware-driven Master I2C interface
* BMI160 driver

## Requirements

### Software
- Install ModusToolbox SDK and IDE
- Toolchain in the SDK is GNU Arm® embedded compiler v9.3.1

### Hardware

- Development board: [PSoC 62 BLE pioneer kit](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062-ble/) (`CY8CKIT-062-BLE`)
- Evaluation board: [Infineon E-ink Display kit](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-028-epd/) (`CY8CKIT-028-EPD`)

## Building

### ModusToolbox IDE

### Comammand Line

1. Install [ModusToolbox](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/)
1. Clone this repository.
1. Open terminal and go to the directory with cloned project
1. Run the following commands

    ```
    make getlibs
    make build
    ```

### Docker

1. Build docker image

    ```
    docker build -t edge-impulse-infineon .
    ```

1. Build firmware

    ```
    docker run --rm -v $PWD:/app edge-impulse-infineon
    ```

## Flashing

### ModusToolbox IDE

Infineon provides extensive documentation, with screenshots, about how to use the ModusToolbox IDE. Topics covered include:
- Importing a project
- Building the project
- Flashing the project to the board

Please visit this link for the [Infineon ModusToolbox IDE guide](https://www.infineon.com/dgdl/Infineon-Eclipse_IDE_for_ModusToolbox_User_Guide_1-UserManual-v01_00-EN.pdf?fileId=8ac78c8c7d718a49017d99bcb86331e8)

### Command Line

1. After building the firmware (see steps above) connect the board and run

    ```
    make program
    ```

### Standalone

1. Install [CyProgrammer](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.cypressprogrammer)
1. Connect the board and run `CyProgrammer`
1. Select a probe/kit
1. Select compiled hex file
1. Connect to the board
1. Program the firmware
1. After successful flashing, you should see a LED blinking patter

    ![](docs/blink.gif)

## Troubleshooting

### Audio sampling at 8kHz

The current firmware version can not sample at 8kHz. We have put request to the engineering team at Infineon for clarification of the PDM setings.

Audio sampling at 16 and 32kHz works and the audio quality is high.

### No other known issues
