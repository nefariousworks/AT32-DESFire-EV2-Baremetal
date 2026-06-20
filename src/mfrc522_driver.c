#include "mfrc522_driver.h"

// --- Utility Functions ---
void _delay_ms(uint32_t ms) {
    uint32_t start = __builtin_mfsr(AVR32_COUNT);
    uint32_t delay_cycles = ms * (F_CPU / 1000);
    while ((__builtin_mfsr(AVR32_COUNT) - start) < delay_cycles);
}

// --- SPI & GPIO Hardware Abstracted Layer ---
void SPI_Init(void) {
    // SCK: PA17
    AVR32_GPIO.port[0].pmr0c = (1 << 17);
    AVR32_GPIO.port[0].pmr1s = (1 << 17);
    AVR32_GPIO.port[0].gperc = (1 << 17);
    
    // MISO: PA28
    AVR32_GPIO.port[0].pmr0c = (1 << 28);
    AVR32_GPIO.port[0].pmr1s = (1 << 28);
    AVR32_GPIO.port[0].gperc = (1 << 28);

    // MOSI: PA29
    AVR32_GPIO.port[0].pmr0c = (1 << 29);
    AVR32_GPIO.port[0].pmr1s = (1 << 29);
    AVR32_GPIO.port[0].gperc = (1 << 29);
    
    // CS / NPCS0: PA24 (Manual GPIO Mode for deterministic edges)
    AVR32_GPIO.port[0].gpers = (1 << 24);
    AVR32_GPIO.port[0].oders = (1 << 24); 
    AVR32_GPIO.port[0].ovrs  = (1 << 24);
    
    // Reset SPI Subsystem
    AVR32_SPI.cr = AVR32_SPI_CR_SWRST_MASK;
    AVR32_SPI.mr = AVR32_SPI_MR_MSTR_MASK | AVR32_SPI_MR_MODFDIS_MASK;
    
    // SPI Mode 0, 8-bits, 6MHz Baud Rate
    AVR32_SPI.csr0 = AVR32_SPI_CSR0_CSAAT_MASK | 
                     AVR32_SPI_CSR0_NCPHA_MASK |
                     (0 << AVR32_SPI_CSR0_CPOL_OFFSET) |
                     (0 << AVR32_SPI_CSR0_BITS_OFFSET) | 
                     (2 << AVR32_SPI_CSR0_SCBR_OFFSET);

    AVR32_SPI.cr = AVR32_SPI_CR_SPIEN_MASK;
}

// --- MFRC522 Register Read/Write Protocol ---
void MFRC522_WriteRegister(uint8_t reg, uint8_t value) {
    uint32_t addr_cmd = ((reg << 1) & 0x7E);
    uint32_t dummy;

    AVR32_GPIO.port[0].ovrc = (1 << 24); // CS LOW

    while (!(AVR32_SPI.sr & AVR32_SPI_SR_TDRE_MASK));
    AVR32_SPI.tdr = addr_cmd;
    while (!(AVR32_SPI.sr & AVR32_SPI_SR_RDRF_MASK));
    dummy = AVR32_SPI.rdr;

    while (!(AVR32_SPI.sr & AVR32_SPI_SR_TDRE_MASK));
    AVR32_SPI.tdr = value;
    while (!(AVR32_SPI.sr & AVR32_SPI_SR_RDRF_MASK));
    dummy = AVR32_SPI.rdr;

    AVR32_GPIO.port[0].ovrs = (1 << 24); // CS HIGH
}

uint8_t MFRC522_ReadRegister(uint8_t reg) {
    uint32_t addr_cmd = (((reg << 1) & 0x7E) | 0x80);
    uint32_t dummy;

    AVR32_GPIO.port[0].ovrc = (1 << 24); // CS LOW

    while (!(AVR32_SPI.sr & AVR32_SPI_SR_TDRE_MASK));
    AVR32_SPI.tdr = addr_cmd;
    while (!(AVR32_SPI.sr & AVR32_SPI_SR_RDRF_MASK));
    dummy = AVR32_SPI.rdr; 

    while (!(AVR32_SPI.sr & AVR32_SPI_SR_TDRE_MASK));
    AVR32_SPI.tdr = 0x00;
    
    // Clock out data
    while (!(AVR32_SPI.sr & AVR32_SPI_SR_RDRF_MASK));
    
    AVR32_GPIO.port[0].ovrs = (1 << 24); // CS HIGH

    return (uint8_t)AVR32_SPI.rdr;
}

// --- Hardware Helper Utilities ---
void MFRC522_SetCRC(uint8_t enable) {
    if (enable) {
        MFRC522_WriteRegister(TxModeReg, 0x80);
        MFRC522_WriteRegister(RxModeReg, 0x80); 
    } else {
        MFRC522_WriteRegister(TxModeReg, 0x00);
        MFRC522_WriteRegister(RxModeReg, 0x00);
    }
}

void MFRC522_Init(void) {
    MFRC522_WriteRegister(CommandReg, PCD_RESETPHASE);
    _delay_ms(50);
    
    MFRC522_WriteRegister(TModeReg, 0x8D);      
    MFRC522_WriteRegister(TPrescalerReg, 0x3E);
    
    // Hardware Timer configured for 500ms to allow Crypto operations
    MFRC522_WriteRegister(TReloadRegL, 0xE8); 
    MFRC522_WriteRegister(TReloadRegH, 0x03); 
    
    MFRC522_WriteRegister(TxASKReg, 0x40);
    MFRC522_WriteRegister(ModeReg, 0x3D);       
    MFRC522_WriteRegister(0xA8, 0x00);
    MFRC522_WriteRegister(0x28, 0x83);
    MFRC522_WriteRegister(0xEE, 0x00);                      
}

// --- Baremetal Transceive Data Core ---
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen) {
    uint8_t status = 0;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;

    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    MFRC522_WriteRegister(ComIEnReg, irqEn | 0x80);
    uint8_t n = MFRC522_ReadRegister(ComIrqReg);
    MFRC522_WriteRegister(ComIrqReg, n & (~0x80));
    MFRC522_WriteRegister(FIFOLevelReg, 0x80);
    
    MFRC522_WriteRegister(CommandReg, PCD_IDLE);

    for (uint8_t i = 0; i < sendLen; i++) {
        MFRC522_WriteRegister(FIFODataReg, sendData[i]);
    }

    MFRC522_WriteRegister(CommandReg, command);    
    if (command == PCD_TRANSCEIVE) {
        uint8_t tmp = MFRC522_ReadRegister(BitFramingReg);
        MFRC522_WriteRegister(BitFramingReg, tmp | 0x80); 
    }

    // Software timeout increased to outlast 500ms hardware timer
    uint32_t timeout = 200000;
    while (timeout--) {
        n = MFRC522_ReadRegister(ComIrqReg);
        
        // Timer Expired / No Card Response Check
        if (n & 0x01) { 
            return 0;
        }

        // Clean Response Check
        if (n & waitIRq) {
            break;
        }
    }

    if (timeout == 0) return 0; // CPU overran timer

    uint8_t tmp = MFRC522_ReadRegister(BitFramingReg);
    MFRC522_WriteRegister(BitFramingReg, tmp & (~0x80));
    
    if (!(MFRC522_ReadRegister(ErrorReg) & 0x1B)) { 
        status = 1;
        if (backData && backLen) {
            uint8_t rxLen = MFRC522_ReadRegister(FIFOLevelReg);
            if (rxLen > *backLen) rxLen = *backLen;
            *backLen = rxLen;
            for (uint8_t i = 0; i < rxLen; i++) {
                backData[i] = MFRC522_ReadRegister(FIFODataReg);
            }
        }
    }
    return status;
}
