/*
  This file is part of the ArduinoECCX08 library.
  Copyright (c) 2018 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <Arduino.h>

#include "ECCX08.h"

const uint32_t ECCX08Class::_wakeupFrequency = 100000u;  // 100 kHz
#ifdef __AVR__
const uint32_t ECCX08Class::_normalFrequency = 400000u;  // 400 kHz
#else
const uint32_t ECCX08Class::_normalFrequency = 1000000u; // 1 MHz
#endif

ECCX08Class::ECCX08Class(TwoWire& wire, uint8_t address) :
  _wire(&wire),
  _address(address)
{
}

ECCX08Class::~ECCX08Class()
{
}

int ECCX08Class::begin(uint8_t i2cAddress)
{
  _address = i2cAddress;
  return begin();
}

int ECCX08Class::begin()
{
  _wire->begin();

  wakeup();
  idle();
  
  long ver = version() & 0x0F00000;

  if (ver != 0x0500000 && ver != 0x0600000) {
    return 0;
  }

  return 1;
}

void ECCX08Class::end()
{
  // First wake up the device otherwise the chip didn't react to a sleep command
  wakeup();
  sleep();
#ifdef WIRE_HAS_END
  _wire->end();
#endif
}

int ECCX08Class::serialNumber(byte sn[])
{
  if (!read(0, 0, &sn[0], 4)) {
    return 0;
  }

  if (!read(0, 2, &sn[4], 4)) {
    return 0;
  }

  if (!read(0, 3, &sn[8], 4)) {
    return 0;
  }

  return 1;
}

String ECCX08Class::serialNumber()
{
  String result = (char*)NULL;
  byte sn[12];

  if (!serialNumber(sn)) {
    return result;
  }

  result.reserve(18);

  for (int i = 0; i < 9; i++) {
    byte b = sn[i];

    if (b < 16) {
      result += "0";
    }
    result += String(b, HEX);
  }

  result.toUpperCase();

  return result;
}

long ECCX08Class::random(long max)
{
  return random(0, max);
}

long ECCX08Class::random(long min, long max)
{
  if (min >= max)
  {
    return min;
  }

  long diff = max - min;

  long r;
  random((byte*)&r, sizeof(r));

  if (r < 0) {
    r = -r;
  }

  r = (r % diff);

  return (r + min);
}

int ECCX08Class::random(byte data[], size_t length)
{
  if (!wakeup()) {
    return 0;
  }

  while (length) {
    if (!sendCommand(0x1b, 0x00, 0x0000)) {
      return 0;
    }

    delay(23);

    byte response[32];

    if (!receiveResponse(response, sizeof(response))) {
      return 0;
    }

    int copyLength = min(32, (int)length);
    memcpy(data, response, copyLength);

    length -= copyLength;
    data += copyLength;
  }

  delay(1);

  idle();

  return 1;
}

int ECCX08Class::generatePrivateKey(int slot, byte publicKey[])
{
  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x40, 0x04, slot)) {
    return 0;
  }

  delay(115);

  if (!receiveResponse(publicKey, 64)) {
    return 0;
  }

  delay(1);

  idle();

  return 1;
}

int ECCX08Class::generatePublicKey(int slot, byte publicKey[])
{
  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x40, 0x00, slot)) {
    return 0;
  }

  delay(115);

  if (!receiveResponse(publicKey, 64)) {
    return 0;
  }

  delay(1);

  idle();

  return 1;
}

int ECCX08Class::ecdsaVerify(const byte message[], const byte signature[], const byte pubkey[])
{
  if (!challenge(message)) {
    return 0;
  }

  if (!verify(signature, pubkey)) {
    return 0;
  }

  return 1;
}

int ECCX08Class::ecSign(int slot, const byte message[], byte signature[])
{
  byte rand[32];

  if (!random(rand, sizeof(rand))) {
    return 0;
  }

  if (!challenge(message)) {
    return 0;
  }

  if (!sign(slot, signature)) {
    return 0;
  }

  return 1;
}

int ECCX08Class::beginSHA256()
{
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x47, 0x00, 0x0000)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::updateSHA256(const byte data[])
{
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x47, 0x01, 64, data, 64)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::endSHA256(byte result[])
{
  return endSHA256(NULL, 0, result);
}

int ECCX08Class::endSHA256(const byte data[], int length, byte result[])
{
  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x47, 0x02, length, data, length)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(result, 32)) {
    return 0;
  }

  delay(1);
  idle();

  return 1;
}

int ECCX08Class::ecdh(int slot, byte mode, const byte pubKeyXandY[], byte output[])
{
  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x43, mode, slot, pubKeyXandY, 64)) {
    return 0;
  }

  delay(55);

  if (mode == ECDH_MODE_OUTPUT) {
    if (!receiveResponse(output, 32)) {
      return 0;
    }
  } else if (mode == ECDH_MODE_TEMPKEY) {
    if (!receiveResponse(output, 1)) {
      return 0;
    }
  }

  delay(1);
  idle();

  return 1;
}

/** \brief AES_GCM encryption function, see
 *   NIST Special Publication 800-38D
 *   7.1, using TempKey.
 *
 * \param[out] IV                 Initialization vector
 *                                (12 bytes)
 * \param[in] ad                  Associated data
 * \param[in] pt                  Plaintext
 * \param[out] ct                 Ciphertext
 * \param[out] tag                Authentication tag
 *                                (16 bytes)
 * \param[in] adLength            The length of ad
 * \param[in] ptLength            The length of pt
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::AESEncrypt(byte IV[], byte ad[], byte pt[], byte ct[], byte tag[], const uint64_t adLength, const uint64_t ptLength)
{
  byte H[16] = {0x00};
  if (!AESBlockEncrypt(H)){
    Serial.println("AESEncrypt: failed to compute H.");
    return 0;
  }

  byte J0[16] = {0x00};
  if (!AESGenIV(IV)){
    Serial.println("AESEncrypt: failed to generate IV.");
    return 0;
  }
  memcpy(J0, IV, 12);
  J0[15] = 0x01;

  byte counterBlock[16];
  memcpy(counterBlock, J0, 16);
  if (!AESIncrementBlock(counterBlock)){
    Serial.println("AESEncrypt: failed to increment counter block.");
    return 0;
  }

  if (!AESGCTR(counterBlock, pt, ct, ptLength)){
    Serial.println("AESEncrypt: failed to encrypt.");
    return 0;
  }

  int adPad = (-adLength) % 16;
  int ctPad = (-ptLength) % 16;

  byte S[16];
  uint64_t inputLength = adLength+adPad+ptLength+ctPad+16;
  byte input[inputLength];
  memcpy(input, ad, adLength);
  memset(input+adLength, 0, adPad);
  memcpy(input+adLength+adPad, ct, ptLength);
  memset(input+adLength+adPad+ptLength, 0, ctPad);
  // Device is little endian.
  // GCM specification requires big endian representation
  // of bit length.
  // Hence we multiply by 8 and
  // reverse the byte order of adLength and ptLength.
  for (int i=0; i<8; i++){
    input[adLength+adPad+ptLength+ctPad+i] = (adLength*8 >> (56-8*i)) & 0xFF;
    input[adLength+adPad+ptLength+ctPad+8+i] = (ptLength*8 >> (56-8*i)) & 0xFF;
  }

  if (!AESGHASH(H, input, S, inputLength)){
    Serial.println("AESEncrypt: failed to compute GHASH.");
    return 0;
  }

  if (!AESGCTR(J0, S, tag, 16)){
    Serial.println("AESEncrypt: failed to compute tag.");
    return 0;
  }

  return 1;
}

/** \brief AES_GCM decryption function, see
 *   NIST Special Publication 800-38D
 *   7.2, using TempKey.
 *
 * \param[in] IV                 Initialization vector
 *                               (12 bytes)
 * \param[in] ad                 Associated data
 * \param[out] pt                Plaintext
 * \param[in] ct                 Ciphertext
 * \param[in] tag                Authentication tag
 *                               (16 bytes)
 * \param[in] adLength           The length of ad
 * \param[in] ctLength           The length of ct
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::AESDecrypt(byte IV[], byte ad[], byte pt[], byte ct[], byte tag[], const uint64_t adLength, const uint64_t ctLength)
{
  uint64_t maxLength = 1ull << 36;
  if (adLength >= maxLength || ctLength >= maxLength){
    return 0;
  }

  byte H[16] = {0x00};
  if (!AESBlockEncrypt(H)){
    return 0;
  }

  byte J0[16] = {0x00};
  memcpy(J0, IV, 12);
  J0[15] = 0x01;

  int adPad = (-adLength) % 16;
  int ctPad = (-ctLength) % 16;

  byte S[16];
  uint64_t inputLength = adLength+adPad+ctLength+ctPad+16;
  byte input[inputLength];
  memcpy(input, ad, adLength);
  memset(input+adLength, 0, adPad);
  memcpy(input+adLength+adPad, ct, ctLength);
  memset(input+adLength+adPad+ctLength, 0, ctPad);
  // Device is little endian.
  // GCM specification requires big endian representation
  // of bit length.
  // Hence we multiply by 8 and
  // reverse the byte order of adLength and ptLength.
  for (int i=0; i<8; i++){
    input[adLength+adPad+ctLength+ctPad+i] = (adLength*8 >> (56-8*i)) & 0xFF;
    input[adLength+adPad+ctLength+ctPad+8+i] = (ctLength*8 >> (56-8*i)) & 0xFF;
  }

  if (!AESGHASH(H, input, S, inputLength)){
    return 0;
  }

  byte tagComputed[16];
  if (!AESGCTR(J0, S, tagComputed, 16)){
    return 0;
  }

  uint8_t equalBytes=0;
  for (int i=0; i<16; i++){
    equalBytes += (tag[i]==tagComputed[i]);
  }
  if (equalBytes!=16){
    // tag mismatch
    return 0;
  }

  byte counterBlock[16];
  memcpy(counterBlock, J0, 16);
  if (!AESIncrementBlock(counterBlock)){
    return 0;
  }

  if (!AESGCTR(counterBlock, ct, pt, ctLength)){
    return 0;
  }

  return 1;
}


/** \brief GCTR function, see
 *   NIST Special Publication 800-38D
 *   6.5
 *
 * \param[in,out] counterBlock    The initial counter block
 *                                (16 bytes).
 * \param[in] input               The input bit string
 * \param[out] output             The output bit string
 * \param[in] inputLength         The length of the input
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::AESGCTR(byte counterBlock[], byte input[], byte output[], const uint64_t inputLength)
{
  if(inputLength == 0){
    return 1;
  }
  int remainder = inputLength % 16;
  int n = inputLength / 16 + (remainder != 0);

  int i;
  for (i=0; i<n-1; i++){
    byte temp[16];
    memcpy(temp, counterBlock, 16);

    if (!AESBlockEncrypt(temp)){
      Serial.println("AESGCTR: failed to encrypt counter block.");
      return 0;
    }

    for (int j=0; j<16; j++){
      output[16*i+j] = input[16*i+j]^temp[j];
    }

    if (!AESIncrementBlock(counterBlock)){
      Serial.println("AESGCTR: failed to increment counter block.");
      return 0;
    }
  }
  byte temp[16];
  memcpy(temp, counterBlock, 16);

  if (!AESBlockEncrypt(temp)){
    Serial.println("AESGCTR: failed to encrypt counter block.");
    return 0;
  }

  remainder = (remainder == 0) ? 16 : remainder;
  for (int j=0; j<remainder; j++){
    output[16*i+j] = input[16*i+j]^temp[j];
  }

  return 1;
}

/** \brief GHASH function, see
 *   NIST Special Publication 800-38D
 *   6.4
 *
 * \param[in] H                   The hash subkey H
 *                                (16 bytes).
 * \param[in] input               The input bit string
 * \param[out] output             The output block
 *                                (16 bytes)
 * \param[in] inputLength         The length of the input
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::AESGHASH(byte H[], byte input[], byte output[], const uint64_t inputLength)
{
  if (inputLength % 16 != 0){
    Serial.println("GHASH is only defined for multiples of 16 bytes.");
    return 0;
  }

  memset(output, 0, 16);
  for (int i=0; i< inputLength/16; i++){
    for (int j=0; j<16; j++){
      output[j] ^= input[16*i+j];
    }
    if (!AESBlockMultiplication(H, output)){
      return 0;
    }
  }
  return 1;
}

/** \brief Increments the right-most 32 bits of the block
 * regarded as an integer mod 2^32
 *
 * \param[in,out] counterBlock  The block to be incremented
 *                              (16 bytes). See
 *                              NIST Special Publication 800-38D
 *                              6.2
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::AESIncrementBlock(byte counterBlock[])
{
// Increment the big-endian counter value
  int i;
  for (i = 0; i < 4; i++) {
    if (++(counterBlock[15 - i]) != 0) {
        break;
    }
  }
  if (i >= 4) {
    Serial.println("AESIncrementBlock: counter overflowed.");
    return 0;
  }
  return 1;
}

/** \brief AES encrypts a block using TempKey
 *
 * \param[in,out] block         The block to be encrypted
 *                              (16 bytes).
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::AESBlockEncrypt(byte block[])
{
  if (!wakeup()) {
    return 0;
  }
  if (!sendCommand(0x51, 0x00, 0xFFFF, block, 16)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(block, 16)) {
    return 0;
  }

  delay(1);
  idle();

  return 1;
}

/** \brief AES block multiplication with hash key H
 *
 * \param[in]     H             The hash subkey
 *                              (16 bytes)
 * \param[in,out] block         The block to be multiplied
 *                              (16 bytes).
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::AESBlockMultiplication(byte H[], byte block[])
{
  if (!wakeup()) {
    return 0;
  }
  byte data[32];
  memcpy(data, H, 16);
  memcpy(data+16, block, 16);
  if (!sendCommand(0x51, 0x03, 0xFFFF, data, 32)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(block, 16)) {
    return 0;
  }

  delay(1);
  idle();

  return 1;
}

/** \brief Generates AES GCM initialization vector.
 *
 * \param[out] IV               Initialization vector to be generated
 *                              (12 bytes). See
 *                              NIST Special Publication 800-38D
 *                              8.2.1 Deterministic Construction
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::AESGenIV(byte IV[])
{
  // The device ID is determined by the public key in slot 0
  byte pubKey[64];
  if (!generatePublicKey(0, pubKey)){
    Serial.println("AESGenIV: failed to obtain device ID");
    return 0;
  }
  // XOR the 64 public key bytes to get 4 bytes
  byte deviceID[4] = {0x00};
  for (int i=0; i<64; i++){
    deviceID[i%4] ^= pubKey[i];
  }
  // First 4 bytes of IV are device ID
  for (int i=0; i<4; i++){
    IV[i] = deviceID[i];
  }

  // Device only has two 4 byte counters
  // instead of 8 byte counter.
  // We increment one counter and read the other
  // This should be enough for the lifetime of the device
  byte counter0[4];
  if (!incrementCounter(0, counter0)){
    Serial.println("AESGenIV: failed to increment counter");
    return 0;
  }
  byte counter1[4];
  if (!readCounter(1, counter1)){
    Serial.println("AESGenIV: failed to read counter");
    return 0;
  }
  // Last 8 bytes of IV are counter
  for (int i=0; i<4; i++){
    // chip counter is little endian
    IV[11-i] = counter0[i];
    IV[7-i] = counter1[i];
  }

  return 1;
}

/** \brief Reads the counter on the device.
 *
 * \param[in] slot           counter slot to read from
 * \param[out] counter       counter value (4 bytes)
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::readCounter(int slot, byte counter[])
{
  if (!wakeup()) {
    return 0;
  }
  if (!sendCommand(0x24, 0x00, slot)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(counter, 4)) {
    return 0;
  }

  delay(1);
  idle();

  return 1;
}

/** \brief Increments the counter on the device.
 *
 * \param[in] slot           counter slot to increment
 * \param[out] counter       counter value (4 bytes)
 *
 * \return 1 on success, otherwise 0.
 */
int ECCX08Class::incrementCounter(int slot, byte counter[])
{
  if (!wakeup()) {
    return 0;
  }
  if (!sendCommand(0x24, 0x01, slot)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(counter, 4)) {
    return 0;
  }

  delay(1);
  idle();

  return 1;
}

int ECCX08Class::readSlot(int slot, byte data[], int length)
{
  if (slot < 0 || slot > 15) {
    return -1;
  }

  if (length % 4 != 0) {
    return 0;
  }

  int chunkSize = 32;

  for (int i = 0; i < length; i += chunkSize) {
    if ((length - i) < 32) {
      chunkSize = 4;
    }

    if (!read(2, addressForSlotOffset(slot, i), &data[i], chunkSize)) {
      return 0;
    }
  }

  return 1;
}

int ECCX08Class::writeSlot(int slot, const byte data[], int length)
{
  if (slot < 0 || slot > 15) {
    return -1;
  }

  if (length % 4 != 0) {
    return 0;
  }

  int chunkSize = 32;

  for (int i = 0; i < length; i += chunkSize) {
    if ((length - i) < 32) {
      chunkSize = 4;
    }

    if (!write(2, addressForSlotOffset(slot, i), &data[i], chunkSize)) {
      return 0;
    }
  }

  return 1;
}

int ECCX08Class::locked()
{
  byte config[4];

  if (!read(0, 0x15, config, sizeof(config))) {
    return 0;
  }

  if (config[2] == 0x00 && config[3] == 0x00) {
    return 1; // locked
  }

  return 0;
}

int ECCX08Class::writeConfiguration(const byte data[])
{
  // skip first 16 bytes, they are not writable
  for (int i = 16; i < 128; i += 4) {
    if (i == 84) {
      // not writable
      continue;
    }

    if (!write(0, i / 4, &data[i], 4)) {
      return 0;
    }
  }

  return 1;
}

int ECCX08Class::readConfiguration(byte data[])
{
  for (int i = 0; i < 128; i += 32) {
    if (!read(0, i / 4, &data[i], 32)) {
      return 0;
    }
  }

  return 1;
}

int ECCX08Class::lock()
{
  // lock config
  if (!lock(0)) {
    return 0;
  }

  // lock data and OTP
  if (!lock(1)) {
    return 0;
  }

  return 1;
}


int ECCX08Class::beginHMAC(uint16_t keySlot)
{
  // HMAC implementation is only for ATECC608
  uint8_t status;
  long ecc608ver = 0x0600000;
  long eccCurrVer = version() & 0x0F00000;
  
  if (eccCurrVer != ecc608ver) {
    return 0;
  }

  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x47, 0x04, keySlot)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::updateHMAC(const byte data[], int length) {
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  // Processing message
  int currLength = 0;
  while (length) {
    data += currLength;

    if (length > 64) {
      currLength = 64;
    } else {
      currLength = length;
    }
    length -= currLength;
    
    if (!sendCommand(0x47, 0x01, currLength, data, currLength)) {
      return 0;
    }

    delay(9);

    if (!receiveResponse(&status, sizeof(status))) {
      return 0;
    }

    delay(1);
  }
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::endHMAC(byte result[])
{
  return endHMAC(NULL, 0, result);
}

int ECCX08Class::endHMAC(const byte data[], int length, byte result[])
{
  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x47, 0x02, length, data, length)) {
    return 0;
  }

  delay(9);

  if (!receiveResponse(result, 32)) {
    return 0;
  }

  delay(1);
  idle();

  return 1;
}

int ECCX08Class::nonce(const byte data[])
{
  return challenge(data);
}

int ECCX08Class::wakeup()
{
  _wire->setClock(_wakeupFrequency);
  _wire->beginTransmission(0x00);
  _wire->endTransmission();

  delayMicroseconds(1500);

  byte response;

  if (!receiveResponse(&response, sizeof(response)) || response != 0x11) {
    return 0;
  }

  _wire->setClock(_normalFrequency);

  return 1;
}

int ECCX08Class::sleep()
{
  _wire->beginTransmission(_address);
  _wire->write(0x01);

  if (_wire->endTransmission() != 0) {
    return 0;
  }

  delay(1);

  return 1;
}

int ECCX08Class::idle()
{
  _wire->beginTransmission(_address);
  _wire->write(0x02);

  if (_wire->endTransmission() != 0) {
    return 0;
  }

  delay(1);

  return 1;
}

long ECCX08Class::version()
{
  uint32_t version = 0;

  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x30, 0x00, 0x0000)) {
    return 0;
  }

  delay(2);

  if (!receiveResponse(&version, sizeof(version))) {
    return 0;
  }

  delay(1);
  idle();

  return version;
}

int ECCX08Class::challenge(const byte message[])
{
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  // Nonce, pass through
  if (!sendCommand(0x16, 0x03, 0x0000, message, 32)) {
    return 0;
  }

  delay(29);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::verify(const byte signature[], const byte pubkey[])
{
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  byte data[128];
  memcpy(&data[0], signature, 64);
  memcpy(&data[64], pubkey, 64);

  // Verify, external, P256
  if (!sendCommand(0x45, 0x02, 0x0004, data, sizeof(data))) {
    return 0;
  }

  delay(72);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::sign(int slot, byte signature[])
{
  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x41, 0x80, slot)) {
    return 0;
  }

  delay(70);

  if (!receiveResponse(signature, 64)) {
    return 0;
  }

  delay(1);
  idle();

  return 1;
}

int ECCX08Class::read(int zone, int address, byte buffer[], int length)
{
  if (!wakeup()) {
    return 0;
  }

  if (length != 4 && length != 32) {
    return 0;
  }

  if (length == 32) {
    zone |= 0x80;
  }

  if (!sendCommand(0x02, zone, address)) {
    return 0;
  }

  delay(5);

  if (!receiveResponse(buffer, length)) {
    return 0;
  }

  delay(1);
  idle();

  return length;
}

int ECCX08Class::write(int zone, int address, const byte buffer[], int length)
{
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  if (length != 4 && length != 32) {
    return 0;
  }

  if (length == 32) {
    zone |= 0x80;
  }

  if (!sendCommand(0x12, zone, address, buffer, length)) {
    return 0;
  }

  delay(26);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::lock(int zone)
{
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  if (!sendCommand(0x17, 0x80 | zone, 0x0000)) {
    return 0;
  }

  delay(32);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::addressForSlotOffset(int slot, int offset)
{
  int block = offset / 32;
  offset = (offset % 32) / 4;  

  return (slot << 3) | (block << 8) | (offset);
}

int ECCX08Class::sendCommand(uint8_t opcode, uint8_t param1, uint16_t param2, const byte data[], size_t dataLength)
{
  int commandLength = 8 + dataLength; // 1 for type, 1 for length, 1 for opcode, 1 for param1, 2 for param2, 2 for CRC
  byte command[commandLength]; 
  
  command[0] = 0x03;
  command[1] = sizeof(command) - 1;
  command[2] = opcode;
  command[3] = param1;
  memcpy(&command[4], &param2, sizeof(param2));
  memcpy(&command[6], data, dataLength);

  uint16_t crc = crc16(&command[1], 8 - 3 + dataLength);
  memcpy(&command[6 + dataLength], &crc, sizeof(crc));

  _wire->beginTransmission(_address);
  _wire->write(command, commandLength);
  if (_wire->endTransmission() != 0) {
    return 0;
  }

  return 1;
}

int ECCX08Class::receiveResponse(void* response, size_t length)
{
  int retries = 20;
  size_t responseSize = length + 3; // 1 for length header, 2 for CRC
  byte responseBuffer[responseSize];

  while (_wire->requestFrom((uint8_t)_address, (size_t)responseSize, (bool)true) != responseSize && retries--);

  responseBuffer[0] = _wire->read();

  // make sure length matches
  if (responseBuffer[0] != responseSize) {
    return 0;
  }

  for (size_t i = 1; _wire->available(); i++) {
    responseBuffer[i] = _wire->read();
  }

  // verify CRC
  uint16_t responseCrc = responseBuffer[length + 1] | (responseBuffer[length + 2] << 8);
  if (responseCrc != crc16(responseBuffer, responseSize - 2)) {
    return 0;
  }
  
  memcpy(response, &responseBuffer[1], length);

  return 1;
}

uint16_t ECCX08Class::crc16(const byte data[], size_t length)
{
  if (data == NULL || length == 0) {
    return 0;
  }

  uint16_t crc = 0;

  while (length) {
    byte b = *data;

    for (uint8_t shift = 0x01; shift > 0x00; shift <<= 1) {
      uint8_t dataBit = (b & shift) ? 1 : 0;
      uint8_t crcBit = crc >> 15;

      crc <<= 1;
      
      if (dataBit != crcBit) {
        crc ^= 0x8005;
      }
    }

    length--;
    data++;
  }

  return crc;
}

#ifdef CRYPTO_WIRE
ECCX08Class ECCX08(CRYPTO_WIRE, 0x60);
#else
ECCX08Class ECCX08(Wire, 0x60);
#endif