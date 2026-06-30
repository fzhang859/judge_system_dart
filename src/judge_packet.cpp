#include "judge_packet.hpp"

#include "CRC.hpp"
#include "judge_protocol.hpp"

#include <cstring>

namespace
{
constexpr uint8_t kSofByte = 0xA5;
}

/**
 * @brief 打包裁判系统数据
 * @param cmd_id 命令ID
 * @param data 数据
 * @param data_len 数据长度
 * @param seq 序列号
 * @return 打包好的数据包
 */
std::vector<uint8_t> pack_judge_system_data(uint16_t cmd_id, const uint8_t *data, uint16_t data_len, uint8_t seq)
{
    const uint16_t total_len = JUDGE_SIZE(data_len);
    std::vector<uint8_t> packet(total_len, 0);

    // 填充帧头
    packet[JUDGE_SOF_OFFSET] = kSofByte; // SOF

    // 数据长度，注意字节序
    uint16_t len_network = data_len;
    memcpy(&packet[JUDGE_DATA_LENGTH_OFFSET], &len_network, sizeof(uint16_t));

    // 序列号
    packet[JUDGE_SEQ_OFFSET] = seq;

    // 计算帧头CRC8校验
    Append_CRC8_Check_Sum(packet.data(), JUDGE_HEADER_SIZE); // 帧头校验

    // 填充命令ID，注意字节序
    uint16_t cmd_id_network = cmd_id;
    memcpy(&packet[JUDGE_CMD_ID_OFFSET], &cmd_id_network, sizeof(uint16_t));

    // 填充数据
    if (data != nullptr && data_len > 0)
    {
        memcpy(&packet[JUDGE_DATA_OFFSET], data, data_len);
    }

    // 计算整包CRC16校验
    Append_CRC16_Check_Sum(packet.data(), total_len);

    return packet;
}

/** @brief 生成dart_info数据包
 * @param
    bit 0-2：
    最近一次己方飞镖击中的目标，开局默认为 0，1 为击中前哨站，2 为击中基地固定目标，3 为击中基地随机固定目标，4 为击中基地随机移动目标, 5 为击中基地末端移动目标
    bit 3-5：
    对方最近被击中的目标累计被击中计数，开局默认为 0，至多为 4
    bit 6-8：
    飞镖此时选定的击打目标，开局默认或未选定/选定前哨站时为 0，选中基地固定目标为 1，选中基地随机目标为 2，选中基地随机移动目标为3, 选中基地末端移动目标为 4
    bit 9-15：
    保留
 * @return 飞镖发射口状态信息
 */
uint16_t generate_dart_info(uint8_t last_hit_target, uint8_t opponent_hit_count, uint8_t selected_target)
{
    uint16_t dart_info = 0;

    // 设置最近一次己方飞镖击中的目标
    dart_info |= (last_hit_target & 0x07); // 取低3位

    // 设置对方最近被击中的目标累计被击中计数
    dart_info |= ((opponent_hit_count & 0x07) << 3); // 取低3位并左移3位

    // 设置飞镖此时选定的击打目标
    dart_info |= ((selected_target & 0x07) << 6); // 取低3位并左移6位

    return dart_info;
}
