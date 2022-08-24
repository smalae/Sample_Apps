/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <zephyr/zephyr.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/console/console.h>
#include <zephyr/device.h>
#include <soc.h>
#include <stdlib.h>
#include "mbedtls/md.h"
#include "mbedtls/aes.h"
#include "aes_alt.h"
#include "mbedtls_config_autogen.h"
#include "sl_malloc.h"
#include "sl_se_manager.h"

//#define DEBUG_DIGEST1
//#define DEBUG_DIGEST2

#define MODE_ENCRYPT                   (1)
#define MODE_DECRYPT                   (2)

#define AES_BLOCK_SIZE                (16)
#define IV_SIZE                       (16)
#define TAG_SIZE                      (32)
#define MAX_MESSAGE_SIZE_ENCRYPTION (1024)
#define MAX_MESSAGE_SIZE_DECRYPTION \
  (2 * MAX_MESSAGE_SIZE_ENCRYPTION + 2 * IV_SIZE + 2 * TAG_SIZE + 1)

static char message[MAX_MESSAGE_SIZE_DECRYPTION];

static const char *key256bits = "603DEB10 15CA71BE 2B73AEF0 857D7781"
                                "1F352C07 3B6108D7 2D9810A3 0914DFF4";
                                
int hextext2bin(uint8_t *binbuf, unsigned int binbuflen, const char *hexstr);
void bin2hextext(char* hexstr, uint8_t* binbuf, unsigned int binbuflen);
void app_aescrypt_process_action(void);

#ifdef DEBUG_DIGEST1
void printDigest(unsigned char *arr)
{
  int i=0;
  printf("Digest: ");
  for(i = 0;i < 32;i++)
    {
      printf("%d ",arr[i]);
    }
  printf("\n");
}
#endif

int main(void)
{
	console_init();
	while(1)
	{
		app_aescrypt_process_action();
	}
}

void app_aescrypt_process_action(void)
{
  int ret;
  int i, n;
  int mode = 0, lastn;
  size_t keylen = 256 / 8;
  char *p;
  unsigned char IV[AES_BLOCK_SIZE];
  unsigned char key[256 / 8];
  unsigned char digest[32];
  unsigned char buffer[64];
  unsigned char diff;
  char initphrase[16];
  mbedtls_aes_context aes_ctx;
  mbedtls_md_context_t sha_ctx;

  long message_size, max_message_size, offset;

  mbedtls_aes_init(&aes_ctx);
  mbedtls_md_init(&sha_ctx);

  ret = mbedtls_md_setup(&sha_ctx,
                         mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                         1);
  if ( ret != 0 ) {
    printf("  ! mbedtls_md_setup() returned -0x%04x\n", -ret);
    goto exit;
  }

  memset(message, 0, sizeof(message));
  memset(initphrase, 0, sizeof(initphrase));
  memset(IV, 0, sizeof(IV));
  memset(key, 0, sizeof(key));
  memset(digest, 0, sizeof(digest));
  memset(buffer, 0, sizeof(buffer));
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 1\n");
#endif
  hextext2bin((uint8_t *) key, 256 / 8, key256bits);

  printf("\nWelcome to AESCRYPT.\n");
  n = 1;
  while (0 == mode) {
    if (n > 0) {
      printf("Please type 'e' to encrypt or 'd' to decrypt: \n");
    }
    n = console_getchar();
    if ('e' == n) {
      mode = MODE_ENCRYPT;
      max_message_size = MAX_MESSAGE_SIZE_ENCRYPTION;

      printf("Thanks. Please type a short phrase to be used as input to "
             "generate the initial vector of the encryption: ");
      p = initphrase;

      while ((13 != n)
             && (p - initphrase < (int)(sizeof(initphrase) - 1))) {
        n = console_getchar();
        if (n > 0) {
          // Local echo
          putchar(n);
          *p++ = (unsigned char) n;
        }
      }
      printf("\nThanks. Please send a message to be encrypted, "
             "terminated by hitting <enter>, i.e. a newline "
             "character. The ciphertext will be printed in the following "
             "format:\n");//inst 1
      // Make sure n is not 'd'
      n = 'e';
    }
    if ('d' == n) {
      mode = MODE_DECRYPT;
      max_message_size = MAX_MESSAGE_SIZE_DECRYPTION;

      printf("Thanks. Please send the message (ciphertext) to be "
             "decrypted. The format must be:\n");
    }
  }

  printf("Initial Vector(16 bytes) | Ciphertext | "
         "Message Digest Tag(32 bytes)\n");

  p = message;
  message_size = 0;

  while ( message_size < max_message_size ) {
    n = console_getchar();
    if (13 == n) {    // newline marks end of message
      break;
    }
    if (n > 0) {
      *p++ = n;
      message_size++;
      // Local echo
      putchar(n);
    }
  }

  // Newline after plain text message input, and ciphertext.
  printf("\n");

  if ( mode == MODE_ENCRYPT ) {//inst 2
    char hexbuf[2 * TAG_SIZE + 1];

    // Generate the initialization vector as:
    // IV = SHA-256( message_size || initphrase )[0..15]
    for ( i = 0; i < 8; i++ ) {
      buffer[i] = (unsigned char)(message_size >> (i << 3) );
    }//buffer[0] = 12

    p = initphrase;

    mbedtls_md_starts(&sha_ctx);//inst 3
    mbedtls_md_update(&sha_ctx, buffer, 8);
    mbedtls_md_update(&sha_ctx, (unsigned char*)p, strlen(initphrase) );
    mbedtls_md_finish(&sha_ctx, digest);
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 2\n");
#endif
    memcpy(IV, digest, sizeof(IV) );
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 3\n");
#endif
    // The last four bits in the IV are actually used
    // to store the file size modulo the AES block size.
    lastn = (int)(message_size & 0x0F);

    IV[15] = (unsigned char) ( (IV[15] & 0xF0) | lastn);

    // Append the IV at the beginning of the output.
    bin2hextext(hexbuf, IV, sizeof(IV) );//inst 4
    printf("%s",hexbuf);//hexbuf_inst 1

    // Hash the IV and the secret key together 8192 times
    // using the result to setup the AES context and HMAC.
    memset(digest, 0, sizeof(digest) );
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 4\n");
#endif
    memcpy(digest, IV, IV_SIZE);//digest 1
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 5\n");
#endif

    for ( i = 0; i < 8192; i++ ) {
      mbedtls_md_starts(&sha_ctx);
      mbedtls_md_update(&sha_ctx, digest, TAG_SIZE);
      mbedtls_md_update(&sha_ctx, key, keylen);
      mbedtls_md_finish(&sha_ctx, digest);
    }
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 6\n");
#endif

    memset(key, 0, sizeof(key) );
    mbedtls_aes_setkey_enc(&aes_ctx, digest, 256);//digest 2
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 7\n");
#endif
    mbedtls_md_hmac_starts(&sha_ctx, digest, TAG_SIZE);
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 8\n");
#endif
    // Encrypt and write the ciphertext.
    for ( p = message, offset = 0;//inst 5
          offset < message_size;
          offset += AES_BLOCK_SIZE, p += AES_BLOCK_SIZE ) {
      n = (message_size - offset > AES_BLOCK_SIZE)
          ? AES_BLOCK_SIZE : (int) (message_size - offset);

      memset(buffer, 0, AES_BLOCK_SIZE);
      memcpy(buffer, p, n);

      for ( i = 0; i < AES_BLOCK_SIZE; i++ ) {
        buffer[i] = (unsigned char)(buffer[i] ^ IV[i]);
      }

      mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, buffer, buffer);//inst 6

      mbedtls_md_hmac_update(&sha_ctx, buffer, AES_BLOCK_SIZE);//inst 7

      bin2hextext(hexbuf, buffer, AES_BLOCK_SIZE);
      printf("%s",hexbuf);//inst 8 //hexbuf_inst 2

      memcpy(IV, buffer, AES_BLOCK_SIZE);
    }

#ifdef DEBUG_DIGEST2
    printDigest(digest);
    printf("inst 1\n");
#endif
    // Finally write the HMAC.
    mbedtls_md_hmac_finish(&sha_ctx, digest);//digest 3
#ifdef DEBUG_DIGEST2
    printDigest(digest);
    printf("inst 2\n");
#endif
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 9\n");
#endif
    bin2hextext(hexbuf, digest, TAG_SIZE);//digest 4
#ifdef DEBUG_DIGEST1
    printDigest(digest);
    printf("inst 10\n");
#endif
    printf("%s\n\n", hexbuf);//hexbuf_inst 3
  }

  if ( mode == MODE_DECRYPT ) {
    unsigned char tmp[AES_BLOCK_SIZE];

    //  The encrypted file must be structured as follows:
    //
    //       00 .. 15              Initialization Vector
    //       16 .. 31              AES Encrypted Block #1
    //          ..
    //     N*16 .. (N+1)*16 - 1    AES Encrypted Block #N
    // (N+1)*16 .. (N+1)*16 + 32   HMAC-SHA-256(ciphertext)
    if ( message_size <  (2 * IV_SIZE + 2 * TAG_SIZE) ) {
      printf("File too short to be decrypted.\n");
      goto exit;
    }

    if ( (message_size & 0x0F) != 0 ) {
      printf("File size not a multiple of 16.\n");
      goto exit;
    }

    // Subtract the IV + HMAC length.
    message_size -= (2 * IV_SIZE + 2 * TAG_SIZE);

    //  Read the IV and original message_size modulo 16.
    hextext2bin(IV, IV_SIZE, message);
    lastn = IV[15] & 0x0F;

    // Hash the IV and the secret key together 8192 times
    // using the result to setup the AES context and HMAC.
    memset(digest, 0, TAG_SIZE);
    memcpy(digest, IV, IV_SIZE);

    for ( i = 0; i < 8192; i++ ) {
      mbedtls_md_starts(&sha_ctx);
      mbedtls_md_update(&sha_ctx, digest, TAG_SIZE);
      mbedtls_md_update(&sha_ctx, key, keylen);
      mbedtls_md_finish(&sha_ctx, digest);
    }

    memset(key, 0, sizeof(key) );
    mbedtls_aes_setkey_dec(&aes_ctx, digest, 256);
    mbedtls_md_hmac_starts(&sha_ctx, digest, TAG_SIZE);

    // Decrypt ciphertext and write the plaintext.
    for ( p = &message[2 * IV_SIZE], offset = 0;
          offset < message_size;
          offset += 2 * AES_BLOCK_SIZE, p += 2 * AES_BLOCK_SIZE ) {
      hextext2bin(buffer, AES_BLOCK_SIZE, p);

      memcpy(tmp, buffer, AES_BLOCK_SIZE);

      mbedtls_md_hmac_update(&sha_ctx, buffer, AES_BLOCK_SIZE);
      mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, buffer, buffer);

      for ( i = 0; i < AES_BLOCK_SIZE; i++ ) {
        buffer[i] = (unsigned char)(buffer[i] ^ IV[i]);
      }

      memcpy(IV, tmp, AES_BLOCK_SIZE);

      n = (lastn > 0 && offset == message_size - 2 * AES_BLOCK_SIZE)
          ? lastn : AES_BLOCK_SIZE;

      buffer[n] = 0;
      printf("%s", (char*) buffer);
    }

    // Newline after printing plaintext.
    printf("\n");

    // Verify the message authentication code.
    mbedtls_md_hmac_finish(&sha_ctx, digest);

    hextext2bin(buffer, TAG_SIZE, p);

    // Use constant-time buffer comparison
    diff = 0;
    for ( i = 0; i < TAG_SIZE; i++ ) {
      diff |= digest[i] ^ buffer[i];
    }

    if ( diff != 0 ) {
      printf("HMAC check failed: wrong key, "
                     "or file corrupted.\n");
      goto exit;
    } else {
      printf("Message digest tag OK.\n");
    }
  }
  ret = 0;

  exit:
  memset(buffer, 0, sizeof(buffer) );
  memset(digest, 0, sizeof(digest) );

  mbedtls_aes_free(&aes_ctx);
  mbedtls_md_free(&sha_ctx);
  while(1);
}

int hextext2bin(uint8_t *binbuf, unsigned int binbuflen, const char *hexstr)
{
  uint32_t ret = 0;
  int      i;
  uint8_t  tmp;
  uint8_t  val;

  while (ret < binbuflen) {
    val = 0;
    for (i = 1; i >= 0; i--) {
      tmp = *(hexstr++);
      // Skip spaces
      while (tmp == ' ') {
        tmp = *(hexstr++);
      }
      // Reached end of string?
      if (!tmp) {
        goto done;
      }

      if (tmp > '9') {
        // Ensure uppercase hex
        tmp &= ~0x20;

        val |= ((tmp - 'A') + 10) << (4 * i);
      } else {
        val |= (tmp - '0') << (4 * i);
      }
    }
    *(binbuf++) = val;
    ret++;
  }
  done:
  return ret;
}

/***************************************************************************//**
 * @brief Convert binary data to ascii hexadecimal text string
 ******************************************************************************/
void bin2hextext(char* hexstr, uint8_t* binbuf, unsigned int binbuflen)
{
  uint32_t i;
  uint8_t nibble;

  for (i = 0; i < binbuflen; i++) {
    nibble = (binbuf[i] >> 4) & 0xF;
    *hexstr++ = nibble > 9 ? nibble - 10 + 'A' : nibble + '0';
    nibble = (binbuf[i] >> 0) & 0xF;
    *hexstr++ = nibble > 9 ? nibble - 10 + 'A' : nibble + '0';
  }
  // Null terminate at end of string.
  *hexstr = 0;
}

