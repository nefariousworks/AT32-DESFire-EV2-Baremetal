/**
 * @file desfire_core.h
 * @brief Core Cryptographic definitions for MIFARE DESFire EV2.
 */

#ifndef DESFIRE_CORE_H
#define DESFIRE_CORE_H

#include <stdint.h>

// --- Global Cryptographic State ---
extern uint8_t session_key[16];
extern uint8_t current_iv[16];

// --- DESFire EV2 Commands ---
#define CMD_SELECT_APPLICATION 0x5A
#define CMD_AUTHENTICATE_AES   0xAA

// --- Function Prototypes ---
void DESFire_CBC_Decrypt(uint8_t *key, uint8_t *iv, uint8_t *ciphertext, uint8_t *plaintext);
void DESFire_CBC_Encrypt(uint8_t *key, uint8_t *iv, uint8_t *plaintext, uint8_t *ciphertext);

#endif // DESFIRE_CORE_H
