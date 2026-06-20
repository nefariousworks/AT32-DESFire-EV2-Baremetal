#include <avr32/io.h>
#include <stdio.h>
#include "../../src/mfrc522_driver.h"

// --- Main Program Routine ---
int main(void) {
    uint8_t buffer[20];
    uint8_t len;
    
    SPI_Init();
    
    // Assuming USART is initialized here
    
    MFRC522_Init();
    
    while(1) {
        // --- FAILSAFE STATE RESETS ---
        MFRC522_SetCRC(0);
        MFRC522_WriteRegister(BitFramingReg, 0x07); // 7-bit framing for REQA
        
        // --- Step 1: Wake Up (REQA) ---
        buffer[0] = PICC_REQIDL;
        len = 20; 
        
        if (MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 1, buffer, &len)) {
            
            MFRC522_WriteRegister(BitFramingReg, 0x00); // Back to standard 8-bit
            
            // --- Step 2: Anti-collision Cascade Level 1 ---
            buffer[0] = 0x93;
            buffer[1] = 0x20; 
            len = 20;
            if (!MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &len)) continue;
            
            // --- Step 3: Select Cascade Level 1 ---
            uint8_t selCmd1[7] = {0x93, 0x70, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
            MFRC522_SetCRC(1); 
            len = 20;
            if (!MFRC522_ToCard(PCD_TRANSCEIVE, selCmd1, 7, buffer, &len)) continue;
            
            if (buffer[0] & 0x04) {
                
                // --- Step 4: Anti-collision Cascade Level 2 ---
                MFRC522_SetCRC(0);
                buffer[0] = 0x95; 
                buffer[1] = 0x20; 
                len = 20;
                if (!MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &len)) continue;
                
                // --- Step 5: Select Cascade Level 2 ---
                uint8_t selCmd2[7] = {0x95, 0x70, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
                MFRC522_SetCRC(1); 
                len = 20;
                if (!MFRC522_ToCard(PCD_TRANSCEIVE, selCmd2, 7, buffer, &len)) continue;
                
                if (buffer[0] == 0x20) {
                    usart_write_line(usart, "\r\nDESFire EV2 UID Extracted!\r\n");
                    
                    // --- Step 6: RATS (Request for Answer To Select) ---
                    uint8_t ratsCmd[2] = {0xE0, 0x50};
                    len = 20;
                    if (MFRC522_ToCard(PCD_TRANSCEIVE, ratsCmd, 2, buffer, &len)) {
                        usart_write_line(usart, "Layer 4 Active (RATS OK)\r\n");
                        
                        // --- SFGT Delay: Let the DESFire OS boot ---
                        _delay_ms(20);
                        
                        // Wipe the buffer so we don't read ghost ATS data
                        for(uint8_t i = 0; i < 20; i++) buffer[i] = 0;
                        
                        // --- Step 7: Select Application (ISO 7816-4 Wrapped) ---
                        // [0] = 0x02 (I-Block PCB)
                        // [1..9] = Wrapped APDU (CLA, INS, P1, P2, Lc, AID, Le)
                        uint8_t appCmd[10] = {0x02, 0x90, 0x5A, 0x00, 0x00, 0x03, 0x56, 0x34, 0x12, 0x00};
                        len = 20;
                        
                        if (MFRC522_ToCard(PCD_TRANSCEIVE, appCmd, 10, buffer, &len)) {
                            
                            // Check for WTX (Card needs more time for Crypto init)
                            if (buffer[0] == 0xF2) {
                                usart_write_line(usart, "Card processing... (WTX requested)\r\n");
                                uint8_t wtxAck[2] = {0xF2, buffer[1]};
                                len = 20;
                                
                                if (!MFRC522_ToCard(PCD_TRANSCEIVE, wtxAck, 2, buffer, &len)) {
                                    usart_write_line(usart, "FAILED: Card died during WTX wait.\r\n");
                                    continue; 
                                }
                            }

                            // Parse the response
                            if (buffer[1] == 0x91 && buffer[2] == 0x00) {
                                usart_write_line(usart, "SUCCESS: Application 123456 Selected!\r\n");
                            } else {
                                char debug_str[50];
                                sprintf(debug_str, "FAILED: PCB=0x%02X SW1=0x%02X SW2=0x%02X\r\n", buffer[0], buffer[1], buffer[2]);
                                usart_write_line(usart, debug_str);
                            }
                        } else {
                            usart_write_line(usart, "FAILED: Select App Timeout (No Response)\r\n");
                        }
                    }
                }
            }
            _delay_ms(1000); // Backoff to prevent spamming the card
        }
        _delay_ms(100);
    }
}
