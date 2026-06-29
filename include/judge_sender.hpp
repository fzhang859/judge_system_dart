#ifndef JUDGE_SENDER_HPP_
#define JUDGE_SENDER_HPP_

#include <stdbool.h>
#include <cstdint>

bool send_judge_system_data(int fd, uint16_t cmd_id, const uint8_t *data, uint16_t data_len, uint8_t seq = 0);

bool send_game_status(int fd, uint8_t game_type, uint8_t game_progress, uint16_t stage_remain_time, uint64_t sync_time_stamp);

bool send_dart_info(int fd, uint8_t dart_remaining_time, uint8_t last_hit_target, uint8_t opponent_hit_count, uint8_t selected_target);

bool send_dart_client_cmd(int fd, uint8_t dart_launch_opening_status, uint16_t target_change_time, uint16_t latest_launch_cmd_time);

#endif
