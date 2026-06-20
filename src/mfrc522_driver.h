#ifndef MFRC522_DRIVER_H
#define MFRC522_DRIVER_H

#include <avr32/io.h>
#include <stdint.h>

// --- CPU Clock Definition ---
#define F_CPU 12000000UL

// --- MFRC522 Registers ---
#define CommandReg    0x01
#define ComIEnReg     0x02
#define DivIEnReg     0x03
#define ComIrqReg     0x04
#define DivIrqReg     0x05
#define ErrorReg      0x06
#define Status2Reg    0x08
#define FIFODataReg   0x09
#define FIFOLevelReg  0x0A
#define ControlReg    0x0C
#define BitFramingReg 0x0D
#define ModeReg       0x11
#define TxModeReg     0x12
#define RxModeReg     0x13
#define TxControlReg  0x14
#define TxASKReg      0x15
#define TModeReg      0x2A
#define TPrescalerReg 0x2B
#define TReloadRegH   0x2C
#define TReloadRegL   0x2D

// --- Commands ---
#define PCD_IDLE       0x00
#define PCD_TRANSCEIVE 0x0C
#define PCD_RESETPHASE 0x0F
#define PICC_REQIDL    0x26

// --- Function Prototypes ---
void _delay_ms(uint32_t ms);
void SPI_Init(void);
void MFRC522_WriteRegister(uint8_t reg, uint8_t value);
uint8_t MFRC522_ReadRegister(uint8_t reg);
void MFRC522_SetCRC(uint8_t enable);
void MFRC522_Init(void);
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen);

#endif // MFRC522_DRIVER_H
