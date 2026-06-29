#ifndef JUDGE_PROTOCOL_HPP_
#define JUDGE_PROTOCOL_HPP_

#include <cstdint>

// cmd_id 2-byte
inline constexpr uint16_t GAME_STATUS = 0x0001;
inline constexpr uint16_t DART_INFO = 0x0105;
inline constexpr uint16_t DART_CLIENT_CMD = 0x020a;

// datasize
inline constexpr uint16_t GAME_STATUS_SIZE = 11;
inline constexpr uint16_t DART_INFO_SIZE = 3;
inline constexpr uint16_t DART_CLIENT_CMD_SIZE = 6;

inline constexpr uint16_t JUDGE_HEADER_SIZE = 5;
inline constexpr uint16_t JUDGE_CMD_ID_SIZE = 2;
inline constexpr uint16_t JUDGE_CRC16_SIZE = 2;

constexpr uint16_t JUDGE_SIZE(uint16_t data_size)
{
    return JUDGE_HEADER_SIZE + JUDGE_CMD_ID_SIZE + data_size + JUDGE_CRC16_SIZE;
}

// Offset
inline constexpr uint16_t JUDGE_SOF_OFFSET = 0;
inline constexpr uint16_t JUDGE_DATA_LENGTH_OFFSET = 1;
inline constexpr uint16_t JUDGE_SEQ_OFFSET = 3;
inline constexpr uint16_t JUDGE_CRC8_OFFSET = 4;
inline constexpr uint16_t JUDGE_CMD_ID_OFFSET = 5;
inline constexpr uint16_t JUDGE_DATA_OFFSET = 7;

constexpr uint16_t JUDGE_CRC16_OFFSET(uint16_t data_size)
{
    return JUDGE_DATA_OFFSET + data_size;
}

#pragma pack(1)

typedef struct
{
    uint8_t game_type : 4; /*
    bit 0-3: 比赛类型
        1: RoboMaster 机甲大师超级对抗赛
        2: RoboMaster 机甲大师高校单项赛
        3: ICRA RoboMaster 高校人工智能挑战赛
        4: RoboMaster 机甲大师高校联盟赛 3V3 对抗
        5: RoboMaster 机甲大师高校联盟赛步兵对抗    */
    uint8_t game_progress : 4;  /*
    bit 4-7: 当前比赛阶段
        0: 未开始比赛
        1: 准备阶段
        2: 十五秒裁判系统自检阶段
        3: 五秒倒计时
        4: 比赛中
        5: 比赛结算中   */
    uint16_t stage_remain_time; // 当前阶段剩余时间，单位：秒
    uint64_t SyncTimeStamp;     // UNIX 时间, 当机器人正确连接到裁判系统的 NTP 服务器后生效
} game_status_t;

typedef struct
{
    uint8_t dart_remaining_time; // 己方飞镖发射剩余时间
    uint16_t dart_info;          /*
    bit 0-2
        最近一次己方飞镖击中的目标，开局默认为 0， 1 为击中前哨站， 2 为击中基地固定目标， 3 为击中基地随机固定目标， 4 为击中基地随机移动目标，5 为击中基地末端移动目标
    bit 3-5
        对方最近被击中的目标累计被击中计次数，开局默认为 0，至多为 4
    bit 6-8
        飞镖此时选定的击打目标，开局默认或未选定/选定前哨站时为 0，选中基地固定目标为 1，选中基地随机固定目标为 2，选中基地随机移动目标为 3，选中基地末端移动目标为 4
    bit 9-15
        保留                      */
} dart_info_t;

typedef struct
{
    uint8_t dart_launch_opening_status;    /*
    当前飞镖发射站的状态:
        1: 关闭
        2: 正在开启或者关闭中
        0: 已经开启                         */
    uint8_t reserved;   // 保留位
    uint16_t target_change_time;    // 切换打击目标时的比赛剩余时间，单位：秒，无/未切换动作，默认为 0
    uint16_t latest_launch_cmd_time;    // 最后一次操作手确定发射指令时的比赛剩余时间，单位：秒，初始值为 0
} dart_client_cmd_t;

#pragma pack()

static_assert(sizeof(game_status_t) == GAME_STATUS_SIZE);
static_assert(sizeof(dart_info_t) == DART_INFO_SIZE);
static_assert(sizeof(dart_client_cmd_t) == DART_CLIENT_CMD_SIZE);

#endif
