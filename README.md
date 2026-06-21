# Baremetal MIFARE DESFire EV2 Driver for AT32UC3B0512

## Overview
This repository contains a baremetal C implementation of a **MIFARE DESFire EV2** authentication and communication stack, driven by an **MFRC522** RFID/NFC module and an **AT32UC3B0512** microcontroller.

Stepping completely outside of higher-level Arduino wrappers, this project interacts directly with the AVR32 SPI hardware registers and handles the strict timing, bit-framing, and cryptographic requirements of the DESFire EV2 protocol. 

## Repository Structure
The project is organized into modular core libraries and progressive examples to demonstrate the DESFire EV2 authentication flow step-by-step:

* **`/src`**: Contains the foundational libraries. 
  * `mfrc522_driver`: Baremetal AVR32 SPI configuration, register handling, and ISO14443A anti-collision loops.
  * `desfire_core`: ISO 14443-4 block transmission, APDU wrapping, and custom AES-CBC cryptographic wrappers.
* **`/examples`**: Contains standalone applications demonstrating project milestones.
  * `01_Select_Application`: Wakes the card, handles RATS (Request for Answer To Select), and selects a specific DESFire Application ID (AID).
  * `02_Mutual_Authentication`: Executes the cryptographic challenge-response protocol (AES) to establish session keys.
  * `03_Read_File_Data`: Uses the established secure session to execute a ReadData command and extract payloads.

## Hardware Setup
This driver requires a 12 MHz external clock configuration (`F_CPU 12000000UL`). The SPI interface is manually routed via GPIO function selection to ensure deterministic control over the Chip Select (CS) line during cryptographic processing delays.

### Pin Mapping
| MFRC522 Pin | AT32UC3B0512 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **SCK** | `PA17` | SPI Clock | GPIO Function C |
| **MISO** | `PA28` | SPI MISO | GPIO Function C |
| **MOSI** | `PA29` | SPI MOSI | GPIO Function C |
| **SDA / CS** | `PA24` | Chip Select | Manual GPIO control (Active Low) |

## Dependencies & Acknowledgements
To compile and run this code, your environment must include:
1. **Atmel Studio Framework (ASF):** The examples rely on ASF for USART serial communication (`usart_write_line()`) to output status messages and debug data. You must initialize USART for your specific board prior to running the main execution loops.
2. **AES Cryptography:** The cryptographic core utilizes the byte-oriented AES-128 library by [openluopworld](https://github.com/openluopworld/aes_128). 
   * *Note:* The original openluopworld library does not natively support CBC (Cipher Block Chaining) mode. Custom wrappers (`DESFire_CBC_Decrypt` and `DESFire_CBC_Encrypt`) were implemented within `src/desfire_core.c` to support the strict **CBC (Cipher Block Chaining)** mode required by the DESFire EV2 Mutual Authentication protocol.

## Usage & Compilation
To test a specific milestone:
1. Include the core files (`mfrc522_driver.c`, `desfire_core.c`, and your AES library) in your compiler's build path.
2. Link them against the `main.c` file found within the specific `examples/` folder you wish to test.
3. Flash the AT32 MCU and open your serial monitor. When a DESFire EV2 card is brought into the RF field, the terminal will walk you through the transaction cycle.
