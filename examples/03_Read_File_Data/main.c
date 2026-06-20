#include <avr32/io.h>
#include <stdio.h>
#include <string.h>
#include "../../src/mfrc522_driver.h"
#include "../../src/desfire_core.h"

// --- Main Program Routine ---
int main(void) {
    uint8_t buffer[128]; // Increased buffer for reading larger file chunks
    uint8_t len;
    
    SPI_Init();
    // (USART Initialization goes here)
    MFRC522_Init();
    
    while(1) {
        // --- FAILSAFE STATE RESETS ---
        MFRC522_SetCRC(0);
        MFRC522_WriteRegister(BitFramingReg, 0x07); 
        
        // --- Step 1: Wake Up (REQA) ---
        buffer[0] = PICC_REQIDL;
        len = 128; 
        
        if (MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 1, buffer, &len)) {
            
            MFRC522_WriteRegister(BitFramingReg, 0x00); 
            
            // --- Step 2: Anti-collision Cascade Level 1 ---
            buffer[0] = 0x93;
            buffer[1] = 0x20; 
            len = 128;
            if (!MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &len)) continue;
            
            // --- Step 3: Select Cascade Level 1 ---
            uint8_t selCmd1[7] = {0x93, 0x70, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
            MFRC522_SetCRC(1); 
            len = 128;
            if (!MFRC522_ToCard(PCD_TRANSCEIVE, selCmd1, 7, buffer, &len)) continue;
            
            if (buffer[0] & 0x04) {
                
                // --- Step 4: Anti-collision Cascade Level 2 ---
                MFRC522_SetCRC(0);
                buffer[0] = 0x95; 
                buffer[1] = 0x20; 
                len = 128;
                if (!MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &len)) continue;
                
                // --- Step 5: Select Cascade Level 2 ---
                uint8_t selCmd2[7] = {0x95, 0x70, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
                MFRC522_SetCRC(1); 
                len = 128;
                if (!MFRC522_ToCard(PCD_TRANSCEIVE, selCmd2, 7, buffer, &len)) continue;
                
                if (buffer[0] == 0x20) {
                    
                    // --- Step 6: RATS (Request for Answer To Select) ---
                    uint8_t ratsCmd[2] = {0xE0, 0x50};
                    len = 128;
                    if (MFRC522_ToCard(PCD_TRANSCEIVE, ratsCmd, 2, buffer, &len)) {
                        
                        _delay_ms(20); // SFGT Delay
                        
                        for(uint8_t i = 0; i < 128; i++) buffer[i] = 0;
                        
                        // --- Phase 1: Select Application ---
                        uint8_t appCmd[10] = {0x02, 0x90, 0x5A, 0x00, 0x00, 0x03, 0x56, 0x34, 0x12, 0x00};
                        len = 128;
                        
                        if (MFRC522_ToCard(PCD_TRANSCEIVE, appCmd, 10, buffer, &len)) {
                            
                            if (buffer[0] == 0xF2) {
                                uint8_t wtxAck[2] = {0xF2, buffer[1]};
                                len = 128;
                                if (!MFRC522_ToCard(PCD_TRANSCEIVE, wtxAck, 2, buffer, &len)) continue; 
                            }

                            if (buffer[1] == 0x91 && buffer[2] == 0x00) {

                                // --- Phase 2: Mutual Authentication AES ---
                                uint8_t default_aes_key[16] = {0}; 
                                for(int i=0; i<16; i++) current_iv[i] = 0x00; 

                                // Auth Step 1: Send 0xAA
                                uint8_t authCmd1[8] = {0x03, 0x90, 0xAA, 0x00, 0x00, 0x01, 0x00, 0x00};
                                len = 128;

                                if (MFRC522_ToCard(PCD_TRANSCEIVE, authCmd1, 8, buffer, &len)) {
                                    
                                    if (buffer[0] == 0xF2) {
                                        uint8_t wtxAck[2] = {0xF2, buffer[1]};
                                        len = 128;
                                        MFRC522_ToCard(PCD_TRANSCEIVE, wtxAck, 2, buffer, &len);
                                    }

                                    uint8_t sw1 = buffer[len - 2];
                                    uint8_t sw2 = buffer[len - 1];

                                    if (sw1 == 0x91 && sw2 == 0xAF) {
                                        
                                        uint8_t enc_RndB[16];
                                        for(int i=0; i<16; i++) enc_RndB[i] = buffer[i+1];

                                        uint8_t RndB[16];
                                        DESFire_CBC_Decrypt(default_aes_key, current_iv, enc_RndB, RndB);

                                        uint8_t RndB_prime[16];
                                        for (int i=0; i<15; i++) RndB_prime[i] = RndB[i+1];
                                        RndB_prime[15] = RndB[0];
                                        
                                        uint8_t RndA[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 
                                                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
                                                            
                                        uint8_t enc_RndA[16];
                                        uint8_t enc_RndB_prime[16];
                                        DESFire_CBC_Encrypt(default_aes_key, current_iv, RndA, enc_RndA);
                                        DESFire_CBC_Encrypt(default_aes_key, current_iv, RndB_prime, enc_RndB_prime);
                                        
                                        // Auth Step 2: Send 0xAF
                                        uint8_t authCmd2[39];
                                        authCmd2[0] = 0x02; 
                                        authCmd2[1] = 0x90; 
                                        authCmd2[2] = 0xAF; 
                                        authCmd2[3] = 0x00; 
                                        authCmd2[4] = 0x00; 
                                        authCmd2[5] = 0x20; 
                                        
                                        for(int i=0; i<16; i++) {
                                            authCmd2[6+i] = enc_RndA[i];
                                            authCmd2[22+i] = enc_RndB_prime[i];
                                        }
                                        authCmd2[38] = 0x00; 

                                        _delay_ms(10); 
                                        
                                        len = 128;
                                        if (MFRC522_ToCard(PCD_TRANSCEIVE, authCmd2, 39, buffer, &len)) {
                                            
                                            if (buffer[0] == 0xF2) {
                                                uint8_t wtxAck[2] = {0xF2, buffer[1]};
                                                len = 128;
                                                MFRC522_ToCard(PCD_TRANSCEIVE, wtxAck, 2, buffer, &len);
                                            }

                                            sw1 = buffer[len - 2];
                                            sw2 = buffer[len - 1];

                                            if (sw1 == 0x91 && sw2 == 0x00) {
                                                usart_write_line(usart, "SUCCESS: Mutual Authentication Complete!\r\n");
                                                
                                                // --- Phase 3: Session Key Creation ---
                                                for(int i = 0; i < 4; i++) {
                                                    session_key[i]      = RndA[i];      
                                                    session_key[i + 4]  = RndB[i];      
                                                    session_key[i + 8]  = RndA[i + 12]; 
                                                    session_key[i + 12] = RndB[i + 12]; 
                                                }

                                                for(int i = 0; i < 16; i++) current_iv[i] = 0x00;

                                                // =====================================
                                                // --- Phase 4: Read File Data ---
                                                // =====================================
                                                usart_write_line(usart, "Requesting File Data (0xBD)...\r\n");

                                                // ISO 7816-4 Wrapped ReadData Command (0xBD)
                                                // Read File ID: 0x01, Offset: 0x000000, Length: 0x000000 (Read All)
                                                // PCB toggles to 0x03 (Block 1) since the last command was 0x02
                                                uint8_t readCmd[15] = {
                                                    0x03, 0x90, 0xBD, 0x00, 0x00, 0x07, 
                                                    0x01,       // FileNo
                                                    0x00, 0x00, 0x00, // Offset (3 bytes, LSB first)
                                                    0x00, 0x00, 0x00, // Length (3 bytes, LSB first -> 0 = All)
                                                    0x00        // Le expected
                                                };

                                                len = 128;
                                                if (MFRC522_ToCard(PCD_TRANSCEIVE, readCmd, 15, buffer, &len)) {
                                                    
                                                    if (buffer[0] == 0xF2) {
                                                        uint8_t wtxAck[2] = {0xF2, buffer[1]};
                                                        len = 128;
                                                        MFRC522_ToCard(PCD_TRANSCEIVE, wtxAck, 2, buffer, &len);
                                                    }

                                                    sw1 = buffer[len - 2];
                                                    sw2 = buffer[len - 1];

                                                    if (sw1 == 0x91 && sw2 == 0x00) {
                                                        usart_write_line(usart, "Read Successful. Data Extracted:\r\n");
                                                        
                                                        // Extract Payload (Skip PCB and Status Words)
                                                        uint8_t payload_len = len - 3; 
                                                        uint8_t file_data[64]; 
                                                        
                                                        char hex_str[10];
                                                        for (int i = 0; i < payload_len; i++) {
                                                            file_data[i] = buffer[i + 1];
                                                            sprintf(hex_str, "%02X ", file_data[i]);
                                                            usart_write_line(usart, hex_str);
                                                        }
                                                        usart_write_line(usart, "\r\n");
                                                        
                                                        // Halt Execution - Transaction Cycle Complete
                                                        while(1);
                                                    } else {
                                                        char debug_str[64];
                                                        sprintf(debug_str, "Read Failed: SW1=0x%02X SW2=0x%02X\r\n", sw1, sw2);
                                                        usart_write_line(usart, debug_str);
                                                    }
                                                }
                                            } 
                                        }
                                    } 
                                }
                            } 
                        } 
                    }
                }
            }
            _delay_ms(1000); 
        }
        _delay_ms(100);
    }
}
