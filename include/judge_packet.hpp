#ifndef JUDGE_PACKET_HPP_
#define JUDGE_PACKET_HPP_

#include <vector>
#include <cstdint>

std::vector<uint8_t> pack_judge_system_data(uint16_t cmd_id, const uint8_t *data, uint16_t data_len, uint8_t seq = 0);

uint16_t generate_dart_info(uint8_t last_hit_target, uint8_t opponent_hit_count, uint8_t selected_target);

#endif
