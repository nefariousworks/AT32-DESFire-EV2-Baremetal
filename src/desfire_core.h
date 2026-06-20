/**
 * @file desfire_core.h
 * @brief Core Cryptographic and APDU definitions for MIFARE DESFire EV2.
 *
 * @note AES Implementation: This project utilizes the aes_128 library 
 * (https://github.com/openluopworld/aes_128). The original library 
 * operates in CTR mode. Custom wrappers (aes128_cbc_encrypt / aes128_cbc_decrypt) 
 * were implemented here to support the CBC mode required by DESFire EV2 
 * Mutual Authentication.
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
#define CMD_READ_DATA          0xBD

// --- Function Prototypes ---
/**
 * @brief Custom CBC Decryption wrapper for the openluopworld AES library.
 */
void aes128_cbc_decrypt(uint8_t *data, uint8_t length, uint8_t *key, uint8_t *iv);

/**
 * @brief Custom CBC Encryption wrapper for the openluopworld AES library.
 */
void aes128_cbc_encrypt(uint8_t *data, uint8_t length, uint8_t *key, uint8_t *iv);

/**
 * @brief Computes the CMAC for authenticated commands.
 */
void generate_cmac(uint8_t *data, uint8_t length, uint8_t *key, uint8_t *mac);

#endif // DESFIRE_CORE_H
