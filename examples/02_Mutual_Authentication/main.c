#include <avr32/io.h>
#include <stdio.h>
#include "../../src/mfrc522_driver.h"
#include "../../src/desfire_core.h"

// --- Main Program Routine ---
int main(void) {
    // Increased buffer size to handle full 32-byte cryptograms + framing
    uint8_t buffer[64];
    uint8_t len;
    
    SPI_Init();
    // (USART Initialization goes here)
    MFRC522_Init();
    
    while(1) {
        // --- FAILSAFE STATE RESETS ---
        MFRC522_SetCRC(0);
        MFRC522_WriteRegister(BitFramingReg, 0x07); // 7-bit framing for REQA
        
        // --- Step 1: Wake Up (REQA) ---
        buffer[0] = PICC_REQIDL;
        len = 64; 
        
        if (MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 1, buffer, &len)) {
            
            MFRC522_WriteRegister(BitFramingReg, 0x00); // Back to standard 8-bit
            
            // --- Step 2: Anti-collision Cascade Level 1 ---
            buffer[0] = 0x93;
            buffer[1] = 0x20; 
            len = 64;
            if (!MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &len)) continue;
            
            // --- Step 3: Select Cascade Level 1 ---
            uint8_t selCmd1[7] = {0x93, 0x70, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
            MFRC522_SetCRC(1); 
            len = 64;
            if (!MFRC522_ToCard(PCD_TRANSCEIVE, selCmd1, 7, buffer, &len)) continue;
            
            if (buffer[0] & 0x04) {
                
                // --- Step 4: Anti-collision Cascade Level 2 ---
                MFRC522_SetCRC(0);
                buffer[0] = 0x95; 
                buffer[1] = 0x20; 
                len = 64;
                if (!MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &len)) continue;
                
                // --- Step 5: Select Cascade Level 2 ---
                uint8_t selCmd2[7] = {0x95, 0x70, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
                MFRC522_SetCRC(1); 
                len = 64;
                if (!MFRC522_ToCard(PCD_TRANSCEIVE, selCmd2, 7, buffer, &len)) continue;
                
                if (buffer[0] == 0x20) {
                    usart_write_line(usart, "\r\nDESFire EV2 UID Extracted!\r\n");
                    
                    // --- Step 6: RATS (Request for Answer To Select) ---
                    uint8_t ratsCmd[2] = {0xE0, 0x50};
                    len = 64;
                    if (MFRC522_ToCard(PCD_TRANSCEIVE, ratsCmd, 2, buffer, &len)) {
                        usart_write_line(usart, "Layer 4 Active (RATS OK)\r\n");
                        
                        // --- SFGT Delay: Let the DESFire OS boot ---
                        _delay_ms(20);
                        
                        // Wipe the buffer so we don't read ghost ATS data
                        for(uint8_t i = 0; i < 64; i++) buffer[i] = 0;
                        
                        // --- Step 7: Select Application (ISO 7816-4 Wrapped) ---
                        // [0] = 0x02 (I-Block PCB, Block 0)
                        uint8_t appCmd[10] = {0x02, 0x90, 0x5A, 0x00, 0x00, 0x03, 0x56, 0x34, 0x12, 0x00};
                        len = 64;
                        
                        if (MFRC522_ToCard(PCD_TRANSCEIVE, appCmd, 10, buffer, &len)) {
                            
                            // Check for WTX (Card needs more time for Crypto init)
                            if (buffer[0] == 0xF2) {
                                usart_write_line(usart, "Card processing... (WTX requested)\r\n");
                                uint8_t wtxAck[2] = {0xF2, buffer[1]};
                                len = 64;
                                
                                if (!MFRC522_ToCard(PCD_TRANSCEIVE, wtxAck, 2, buffer, &len)) {
                                    usart_write_line(usart, "FAILED: Card died during WTX wait.\r\n");
                                    continue; 
                                }
                            }

                            if (buffer[1] == 0x91 && buffer[2] == 0x00) {
                                usart_write_line(usart, "SUCCESS: Application 123456 Selected!\r\n");

                                // ==========================================
                                // --- Phase 2: Mutual Authentication AES ---
                                // ==========================================
                                usart_write_line(usart, "Starting AES Authentication...\r\n");

                                uint8_t default_aes_key[16] = {0}; // 16 bytes of 0x00
                                for(int i=0; i<16; i++) current_iv[i] = 0x00; // IV zeroed

                                // Auth Step 1: Send 0xAA for Key 0x00
                                // PCB toggles to 0x03 (Block 1)
                                uint8_t authCmd1[8] = {0x03, 0x90, 0xAA, 0x00, 0x00, 0x01, 0x00, 0x00};
                                len = 64;

                                if (MFRC522_ToCard(PCD_TRANSCEIVE, authCmd1, 8, buffer, &len)) {
                                    
                                    if (buffer[0] == 0xF2) {
                                        uint8_t wtxAck[2] = {0xF2, buffer[1]};
                                        len = 64;
                                        MFRC522_ToCard(PCD_TRANSCEIVE, wtxAck, 2, buffer, &len);
                                    }

                                    // Dynamic Status Word checking
                                    uint8_t sw1 = buffer[len - 2];
                                    uint8_t sw2 = buffer[len - 1];

                                    if (sw1 == 0x91 && sw2 == 0xAF) {
                                        usart_write_line(usart, "Auth Step 1 OK. RndB received.\r\n");
                                        
                                        // Extract Enc(RndB)
                                        uint8_t enc_RndB[16];
                                        for(int i=0; i<16; i++) enc_RndB[i] = buffer[i+1];

                                        // Decrypt RndB
                                        uint8_t RndB[16];
                                        DESFire_CBC_Decrypt(default_aes_key, current_iv, enc_RndB, RndB);

                                        // Rotate RndB left by 1 byte -> RndB'
                                        uint8_t RndB_prime[16];
                                        for (int i=0; i<15; i++) RndB_prime[i] = RndB[i+1];
                                        RndB_prime[15] = RndB[0];
                                        
                                        // Generate RndA (Mock RNG for testing)
                                        uint8_t RndA[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 
                                                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
                                                            
                                        // Encrypt RndA and RndB'
                                        uint8_t enc_RndA[16];
                                        uint8_t enc_RndB_prime[16];
                                        DESFire_CBC_Encrypt(default_aes_key, current_iv, RndA, enc_RndA);
                                        DESFire_CBC_Encrypt(default_aes_key, current_iv, RndB_prime, enc_RndB_prime);
                                        
                                        // Auth Step 2: Send 0xAF
                                        // PCB toggles back to 0x02 (Block 0).
                                        // Restored Le byte at the end (authCmd2[38] = 0x00) for strict ISO7816-4 parsing
                                        uint8_t authCmd2[39];
                                        authCmd2[0] = 0x02; // PCB
                                        authCmd2[1] = 0x90; // CLA
                                        authCmd2[2] = 0xAF; // INS
                                        authCmd2[3] = 0x00; // P1
                                        authCmd2[4] = 0x00; // P2
                                        authCmd2[5] = 0x20; // Lc (32 bytes)
                                        
                                        for(int i=0; i<16; i++) {
                                            authCmd2[6+i] = enc_RndA[i];
                                            authCmd2[22+i] = enc_RndB_prime[i];
                                        }
                                        authCmd2[38] = 0x00; // Expected Length (Le)

                                        _delay_ms(10); // Let the card breathe
                                        
                                        len = 64;
                                        if (MFRC522_ToCard(PCD_TRANSCEIVE, authCmd2, 39, buffer, &len)) {
                                            
                                            if (buffer[0] == 0xF2) {
                                                uint8_t wtxAck[2] = {0xF2, buffer[1]};
                                                len = 64;
                                                MFRC522_ToCard(PCD_TRANSCEIVE, wtxAck, 2, buffer, &len);
                                            }

                                            sw1 = buffer[len - 2];
                                            sw2 = buffer[len - 1];

                                            if (sw1 == 0x91 && sw2 == 0x00) {
                                                usart_write_line(usart, "SUCCESS: Mutual Authentication Complete!\r\n");
                                                
                                                // =====================================
                                                // --- Phase 3: Session Key Creation ---
                                                // =====================================
                                                for(int i = 0; i < 4; i++) {
                                                    session_key[i]      = RndA[i];      // Bytes 0-3
                                                    session_key[i + 4]  = RndB[i];      // Bytes 4-7
                                                    session_key[i + 8]  = RndA[i + 12]; // Bytes 8-11
                                                    session_key[i + 12] = RndB[i + 12]; // Bytes 12-15
                                                }

                                                // Reset the IV to all zeros for the start of Secure Messaging
                                                for(int i = 0; i < 16; i++) current_iv[i] = 0x00;
                                                
                                                usart_write_line(usart, "Session Key Generated. Ready for Secure Messaging.\r\n");

                                                // STOP EXECUTION - Success Achieved
                                                while(1);
                                                
                                            } else {
                                                char debug_str[64];
                                                sprintf(debug_str, "Auth Failed: SW1=0x%02X SW2=0x%02X (Len: %d)\r\n", sw1, sw2, len);
                                                usart_write_line(usart, debug_str);
                                            }
                                        }
                                    } else {
                                        char debug_str[64];
                                        sprintf(debug_str, "Auth Step 1 Failed. SW1=0x%02X SW2=0x%02X\r\n", sw1, sw2);
                                        usart_write_line(usart, debug_str);
                                    }
                                }
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
            _delay_ms(1000); 
        }
        _delay_ms(100);
    }
}
