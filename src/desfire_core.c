#include "desfire_core.h"
// Include your byte-oriented AES library here (e.g., kokke/tiny-AES-c)
// #include "aes.h" 

// --- Global Cryptographic State ---
uint8_t session_key[16] = {0};
uint8_t current_iv[16]  = {0}; 

// --- Strict Memory-Isolated CBC Wrappers ---
// Prevents memory aliasing issues on Big-Endian AVR32 architecture
void DESFire_CBC_Decrypt(uint8_t *key, uint8_t *iv, uint8_t *ciphertext, uint8_t *plaintext) {
    uint8_t roundkeys[176];
    uint8_t temp_cipher[16];
    uint8_t temp_plain[16];

    aes_key_schedule_128(key, roundkeys); // Dependent on your specific AES lib

    // Copy ciphertext to temp to avoid aliasing and save for NEXT IV
    for (int i=0; i<16; i++) temp_cipher[i] = ciphertext[i];
    
    // ECB Decrypt using strict separate buffers
    aes_decrypt_128(roundkeys, temp_cipher, temp_plain);
    
    // XOR with IV to complete CBC, and update IV
    for (int i=0; i<16; i++) {
        plaintext[i] = temp_plain[i] ^ iv[i];
        iv[i] = temp_cipher[i];
    }
}

void DESFire_CBC_Encrypt(uint8_t *key, uint8_t *iv, uint8_t *plaintext, uint8_t *ciphertext) {
    uint8_t roundkeys[176];
    uint8_t temp_plain[16];
    uint8_t temp_cipher[16];

    aes_key_schedule_128(key, roundkeys); // Dependent on your specific AES lib

    // XOR plaintext with current IV into a temp buffer
    for (int i=0; i<16; i++) {
        temp_plain[i] = plaintext[i] ^ iv[i];
    }

    // ECB Encrypt using strict separate buffers
    aes_encrypt_128(roundkeys, temp_plain, temp_cipher);
    
    // Output ciphertext and update IV
    for (int i=0; i<16; i++) {
        ciphertext[i] = temp_cipher[i];
        iv[i] = temp_cipher[i];
    }
}
