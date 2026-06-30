#include "judge_sender.hpp"
#include "judge_packet.hpp"
#include "judge_protocol.hpp"

#include <cstdio>
#include <random>
#include <vector>
#include <unistd.h>

#include <iostream>

namespace
{
    struct DartStatusJumpProbability
    {
        int opened_to_closed_percent = 1;
        int opened_to_switching_percent = 5;

        int closed_to_opened_percent = 1;
        int closed_to_switching_percent = 5;

        int switching_to_opened_percent = 20;
        int switching_to_closed_percent = 20;
    };

    uint8_t apply_dart_opening_status_jump(uint8_t actual_status, const DartStatusJumpProbability &probability)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, 100);

        const int roll = dis(gen);

        switch (actual_status)
        {
        case 0:
            if (roll <= probability.opened_to_closed_percent)
            {
                return 1;
            }
            if (roll <= probability.opened_to_closed_percent +
                            probability.opened_to_switching_percent)
            {
                return 2;
            }
            return actual_status;

        case 1:
            if (roll <= probability.closed_to_opened_percent)
            {
                return 0;
            }
            if (roll <= probability.closed_to_opened_percent + probability.closed_to_switching_percent)
            {
                return 2;
            }
            return actual_status;

        case 2:
            if (roll <= probability.switching_to_closed_percent)
            {
                return 1;
            }
            if (roll <= probability.switching_to_closed_percent +
                            probability.switching_to_opened_percent)
            {
                return 0;
            }
            return actual_status;

        default:
            return actual_status;
        }
    }
};


/**
 * @brief 发送裁判系统数据
 * @param fd 串口文件描述符
 * @param cmd_id 命令ID
 * @param data 数据
 * @param data_len 数据长度
 * @param seq 序列号
 * @return 发送是否成功
 */
bool send_judge_system_data(int fd, uint16_t cmd_id, const uint8_t *data, uint16_t data_len, uint8_t seq)
{
    // 序列号递增，过255清零
    static uint8_t seq_counter = 0;
    seq_counter++;
    if (seq_counter > 255)
    {
        seq_counter = 0;
    }
    // 1% 随机丢包
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1, 100);

    if (dis(gen) == 1)
    {
        std::cout << "[模拟丢包] 本次数据未发送 (1%)" << std::endl;
        return true;
    }

    std::vector<uint8_t> packet = pack_judge_system_data(cmd_id, data, data_len, seq_counter);

    // 发送数据
    ssize_t bytes_written = write(fd, packet.data(), packet.size());

    if (bytes_written != static_cast<ssize_t>(packet.size()))
    {
        std::cerr << "Error sending data: " << bytes_written << " bytes sent, expected "
                  << packet.size() << std::endl;
        return false;
    }

    // 打印发送的数据（调试用）
    // std::cout << "Sent packet: ";
    // for (size_t i = 0; i < packet.size(); ++i)
    // {
    //     printf("%02X ", packet[i]);
    // }
    // std::cout << std::endl;

    return true;
}

/**
 * @brief 发送比赛状态数据
 * @param fd 串口文件描述符
 * @param game_type 比赛类型
 * @param game_progress 比赛阶段
 * @param stage_remain_time 当前阶段剩余时间，单位 s
 * @param sync_time_stamp 机器人接收到该指令的精确 Unix 时间，单位 us
 * @return 发送是否成功
 */
bool send_game_status(int fd, uint8_t game_type, uint8_t game_progress, uint16_t stage_remain_time, uint64_t sync_time_stamp)
{
    // 使用 judge_protocol.hpp 中定义的game_status_t结构体
    game_status_t game_status;

    // 填充数据
    game_status.game_type = game_type;
    game_status.game_progress = game_progress;
    game_status.stage_remain_time = stage_remain_time; // 注意：在发送前会自动进行字节序转换
    game_status.SyncTimeStamp = sync_time_stamp;

    // 发送数据
    return send_judge_system_data(fd, GAME_STATUS, (const uint8_t *)&game_status, sizeof(game_status_t));
}

/**
 * @brief 发送飞镖发射口倒计时数据
 * @param fd 串口文件描述符
 * @param dart_remaining_time 飞镖发射口倒计时，单位s
 * @param last_hit_target 最近一次己方飞镖击中目标
 * @param opponent_hit_count 对方最近被击中目标累计被击中计数
 * @param selected_target 飞镖选定的击打目标
 * @return 发送是否成功
 */
bool send_dart_info(int fd, uint8_t dart_remaining_time, uint8_t last_hit_target, uint8_t opponent_hit_count, uint8_t selected_target)
{
    dart_info_t dart_status;
    dart_status.dart_remaining_time = dart_remaining_time;
    dart_status.dart_info = generate_dart_info(last_hit_target, opponent_hit_count, selected_target);

    return send_judge_system_data(fd, DART_INFO, (const uint8_t *)&dart_status, sizeof(dart_info_t));
}

/**
 * @brief 发送飞镖机器人客户端指令数据
 * @param fd 串口文件描述符
 * @param dart_launch_opening_status 当前飞镖发射口状态
 * @param target_change_time 切换目标时间
 * @param latest_launch_cmd_time 最新发射指令时间
 * @return 发送是否成功
 */
bool send_dart_client_cmd(int fd, uint8_t dart_launch_opening_status, uint16_t target_change_time, uint16_t latest_launch_cmd_time)
{
    static const DartStatusJumpProbability probability{
        1,  // opened_to_closed_percent
        5,  // opened_to_switching_percent
        1,  // closed_to_opened_percent
        5,  // closed_to_switching_percent
        20, // switching_to_opened_percent
        20, // switching_to_closed_percent
    };

    dart_client_cmd_t dart_cmd;
    dart_cmd.dart_launch_opening_status = apply_dart_opening_status_jump(dart_launch_opening_status, probability);
    dart_cmd.reserved = 0;
    dart_cmd.target_change_time = target_change_time;
    dart_cmd.latest_launch_cmd_time = latest_launch_cmd_time;

    return send_judge_system_data(fd, DART_CLIENT_CMD, (const uint8_t *)&dart_cmd, sizeof(dart_client_cmd_t));
}
