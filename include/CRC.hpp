#ifndef CRC_HPP_
#define CRC_HPP_

#include <cstddef>
#include <cstdint>
#include <cstdbool>

uint8_t Get_CRC8_Check_Sum(uint8_t *pchMessage, size_t dwLength, uint8_t CRC8);

bool Verify_CRC8_Check_Sum(uint8_t *pchMessage, size_t dwLength);

void Append_CRC8_Check_Sum(uint8_t *pchMessage, size_t dwLength);

uint16_t Get_CRC16_Check_Sum(uint8_t *pchMessage, size_t dwLength, uint16_t CRC16);

bool Verify_CRC16_Check_Sum(uint8_t *pchMessage, size_t dwLength);

void Append_CRC16_Check_Sum(uint8_t *pchMessage, size_t dwLength);

#endif
