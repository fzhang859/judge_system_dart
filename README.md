# 裁判系统飞镖数据模拟器

本项目模拟 RoboMaster 裁判系统中与飞镖相关的比赛状态、飞镖发射口状态、飞镖倒计时和目标选择数据，并通过串口周期性发送裁判系统协议数据。

## 编译和运行

```bash
cd build
cmake ..
make
./build/app [options]
```

默认串口设备为 `/dev/ttyUSB0`，串口参数为：

| 项目 | 当前配置 |
| --- | --- |
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验位 | 无 |
| 停止位 | 1 |
| 硬件流控 | 关闭 |
| 软件流控 | 关闭 |
| 模式 | raw mode |

## 命令行参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `-h`, `--help` | 无 | 显示帮助信息后退出 |
| `-p`, `--port DEV` | `/dev/ttyUSB0` | 指定串口设备 |
| `-i`, `--interval N` | `100` | 主循环更新和状态打印间隔，单位 ms；小于 10 时会被改为 10 |
| `-o`, `--once` | `false` | 只模拟一轮比赛，结算结束后退出 |
| `-d`, `--dart-target N` | `0` | 设置飞镖目标；允许值为 0-3，非法值会回退到 0 |

`--dart-target` 取值：

| 值 | 含义 |
| --- | --- |
| 0 | 前哨站，默认或未切换目标 |
| 1 | 基地固定目标 |
| 2 | 基地随机目标 |
| 3 | 基地随机移动目标 |

## 当前模拟时间线

代码中的比赛时间是压缩模拟时间，不是标准整场比赛时长。

| 阶段 | `game_progress` | 持续时间 | 说明 |
| --- | --- | --- | --- |
| 准备阶段 | 1 | 3 s | 程序开始后从 `Standby` 进入该阶段 |
| 裁判系统自检阶段 | 2 | 15 s | 剩余 10 s 时飞镖口状态设为已开启；剩余 3 s 时设为正在开启或关闭 |
| 五秒倒计时 | 3 | 5 s | 剩余 3 s 时飞镖口状态设为关闭 |
| 比赛中 | 4 | 100 s | 模拟飞镖目标切换和两次飞镖事件 |
| 比赛结算中 | 5 | 15 s | 结束后回到 `Standby`；未使用 `--once` 时 3 s 后自动开始下一轮 |

一轮完整模拟约为 `3 + 15 + 5 + 100 + 15 = 138 s`，如果不是 `--once` 模式，轮次之间额外等待 3 s。

## 飞镖事件时间线

飞镖系统本身是一个独立状态机：

| 飞镖状态 | 持续时间 | `dart_launch_opening_status` | 说明 |
| --- | --- | --- | --- |
| `Closed` | 直到事件触发 | 1 | 飞镖发射口关闭 |
| `Opening` | 7 s | 2 | 飞镖发射口正在开启 |
| `Opened` | 一个更新周期 | 0 | 飞镖发射口已开启，随后进入发射倒计时 |
| `Countdown` | 20 s | 0 | `dart_remaining_time` 从 20 s 倒计时到 0 |
| `Closing` | 7 s | 2 | 飞镖发射口正在关闭 |
| `Closed` | 直到下一次事件 | 1 | 飞镖发射口关闭 |

当前代码在比赛中触发两次飞镖事件：

| 触发条件 | 行为 |
| --- | --- |
| 比赛中经过时间 `>= 5 s` | 触发第一次飞镖事件，舱门开始打开 |
| 比赛中经过时间 `>= 45 s` | 触发第二次飞镖事件，舱门开始打开 |

如果 `--dart-target` 不是 0，比赛中经过时间等于 3 s 时会设置：

| 字段 | 值 |
| --- | --- |
| `selected_target` | `--dart-target` 的值 |
| `target_change_time` | 当前比赛剩余时间，单位 s |

注意：当前实现使用 `total_game_seconds == 3` 判断目标切换，且没有额外的“一次性触发”标志。因此在这一整秒内，每次主循环更新都会重复设置目标并打印“前哨站被击毁，切换飞镖目标!”。默认 `--interval 100` 时，这条日志大约会打印 10 次。

## 发送的数据

主循环每次达到 `--interval` 后更新模拟器，并按各自周期发送三类裁判系统数据。

| 数据 | `cmd_id` | 数据体大小 | 整包大小 | 默认频率 | 发送函数 |
| --- | --- | ---: | ---: | --- | --- |
| 比赛状态 | `0x0001` | 11 B | 20 B | 每 1 s 一次 | `send_game_status` |
| 飞镖发射状态 | `0x0105` | 3 B | 12 B | 每 1 s 一次 | `send_dart_info` |
| 飞镖机器人客户端指令 | `0x020a` | 6 B | 15 B | 每 333 ms 一次 | `send_dart_client_cmd` |

如果 `--interval` 大于某个发送周期，实际发送只能在主循环更新时发生。例如 `--interval 1000` 时，333 ms 的客户端指令也最多约 1 s 发送一次。

每次发送都会先经过 1% 随机丢包模拟。丢包时不会写串口，会打印：

```text
[模拟丢包] 本次数据未发送 (1%)
```

### 通用帧格式

每个裁判系统数据包格式如下：

| 偏移 | 字段 | 大小 | 当前实现 |
| ---: | --- | ---: | --- |
| 0 | SOF | 1 B | 固定 `0xA5` |
| 1 | data_length | 2 B | 数据体长度 |
| 3 | seq | 1 B | 全局递增序列号，超过 255 后回到 0 |
| 4 | CRC8 | 1 B | 帧头 CRC8 |
| 5 | cmd_id | 2 B | 命令 ID |
| 7 | data | N B | 对应命令的数据体 |
| 7 + N | CRC16 | 2 B | 整包 CRC16 |

整包大小为：

```text
5 字节帧头 + 2 字节 cmd_id + 数据体长度 + 2 字节 CRC16
```

当前实现通过 `memcpy` 直接写入多字节字段，因此字节序依赖运行平台。常见 x86/Linux 环境下实际为小端序。

### `0x0001` 比赛状态数据

发送周期：每 1 s 一次。

数据体结构：`game_status_t`，大小 11 B。

| 字段 | 大小 | 当前内容 |
| --- | ---: | --- |
| `game_type` | 4 bit | 固定为 `1`，表示 RoboMaster 机甲大师超级对抗赛 |
| `game_progress` | 4 bit | 当前比赛阶段：0 未开始，1 准备，2 自检，3 五秒倒计时，4 比赛中，5 结算 |
| `stage_remain_time` | 2 B | 当前阶段剩余时间，单位 s |
| `SyncTimeStamp` | 8 B | 当前系统时间戳，单位 us |

字段来源：

| 字段 | 来源 |
| --- | --- |
| `game_type` | `kDefaultGameType = 1` |
| `game_progress` | `DartSimulatorSnapshot::game_progress` |
| `stage_remain_time` | `DartSimulatorSnapshot::game_time_remaining` |
| `SyncTimeStamp` | `std::chrono::high_resolution_clock::now()` 转为 us |

### `0x0105` 飞镖发射状态数据

发送周期：每 1 s 一次。

数据体结构：`dart_info_t`，大小 3 B。

| 字段 | 大小 | 当前内容 |
| --- | ---: | --- |
| `dart_remaining_time` | 1 B | 己方飞镖发射剩余时间，单位 s；只有飞镖发射倒计时时大于 0 |
| `dart_info` | 2 B | 由最近命中目标、对方被击中计数、当前选定目标打包而成 |

`dart_info` 位分布：

| 位 | 内容 | 当前来源 |
| --- | --- | --- |
| bit 0-2 | 最近一次己方飞镖击中的目标 | `last_hit_target`，当前模拟中一直为 0 |
| bit 3-5 | 对方最近被击中目标累计被击中计数 | `opponent_hit_count`，当前模拟中一直为 0 |
| bit 6-8 | 飞镖此时选定的击打目标 | `selected_target & 0x07` |
| bit 9-15 | 保留 | 0 |


### `0x020a` 飞镖机器人客户端指令数据

发送周期：每 333 ms 一次。

数据体结构：`dart_client_cmd_t`，大小 6 B。

| 字段 | 大小 | 当前内容 |
| --- | ---: | --- |
| `dart_launch_opening_status` | 1 B | 当前飞镖发射口状态：0 已开启，1 关闭，2 正在开启或关闭 |
| `reserved` | 1 B | 固定为 0 |
| `target_change_time` | 2 B | 切换目标时的比赛剩余时间，单位 s；未切换时为 0 |
| `latest_launch_cmd_time` | 2 B | 最后一次发射指令确认时的比赛剩余时间，单位 s；初始为 0 |

`latest_launch_cmd_time` 在每次飞镖发射倒计时刚开始后记录一次，值为当时的 `game_time_remaining`。

## 控制台打印内容

程序会在以下场景打印信息。

### 启动和参数相关

串口打开成功：

```text
UART initialized successfully on <port>
开始模拟裁判系统飞镖数据发送...
按 Ctrl+C 退出程序
```

串口打开或配置失败：

```text
Error opening serial port: <device>
Failed to initialize UART on <device>
Error configuring serial port
```

发送间隔小于 10 ms：

```text
Interval too small. Using 10ms.
```

飞镖目标参数非法：

```text
Invalid dart target. Using default (0: 前哨站).
```

### 比赛阶段变化

```text
比赛开始，进入准备阶段!
准备阶段结束，进入裁判系统自检阶段!
裁判系统自检完成，进入五秒倒计时!
比赛正式开始!
前哨站被击毁，切换飞镖目标!
比赛结束，进入结算阶段!
3秒后重新开始比赛...
```

其中“前哨站被击毁，切换飞镖目标!”只会在 `--dart-target` 不为 0 时出现，并且当前实现会在比赛中第 3 秒内重复打印。

### 飞镖事件变化

```text
飞镖事件触发!
飞镖舱门已完全打开!
飞镖发射倒计时开始: 20秒
飞镖发射倒计时结束，舱门开始关闭
飞镖舱门已完全关闭!
```

### 周期性状态打印

每次主循环更新都会打印一次比赛状态和飞镖状态。默认 `--interval 100` 时约每 100 ms 打印一次。

比赛状态格式：

```text
====================================
当前比赛状态: <状态名> | 剩余时间: <remain_time>秒
====================================
```

状态名可能为：

| `game_progress` | 打印文本 |
| --- | --- |
| 0 | 未开始比赛 |
| 1 | 准备阶段 |
| 2 | 裁判系统自检阶段 |
| 3 | 五秒倒计时 |
| 4 | 比赛中 |
| 5 | 比赛结算中 |
| 其他 | 未知状态 |

飞镖状态格式：

```text
飞镖发射口状态: <状态名> | 倒计时: <remaining_time>秒
最新发射指令时间: <latest_launch_cmd_time>秒
最近一次己方飞镖击中目标: <last_hit_target>
对方最近被击中目标累计被击中计数: <opponent_hit_count>
飞镖选定的击打目标: <selected_target>
```

当 `remaining_time` 为 0 时，第一行不会打印倒计时。

飞镖发射口状态名：

| `dart_launch_opening_status` | 打印文本 |
| --- | --- |
| 0 | 已经开启 |
| 1 | 关闭 |
| 2 | 正在开启或者关闭中 |
| 其他 | 未知状态 |

### 发送相关打印

模拟丢包：

```text
[模拟丢包] 本次数据未发送 (1%)
```

串口写入字节数不符合预期：

```text
Error sending data: <bytes_written> bytes sent, expected <packet_size>
```

异常捕获：

```text
Error: <exception message>
```

代码里保留了十六进制打印已发送数据包的调试代码，但当前被注释，不会输出 `Sent packet`。

## 当前模拟数据初始值和重置行为

`DartSimulatorSnapshot` 初始值：

| 字段 | 初始值 | 说明 |
| --- | ---: | --- |
| `game_progress` | 0 | 未开始比赛 |
| `game_time_remaining` | 0 | 当前阶段剩余时间 |
| `dart_opening_status` | 1 | 飞镖发射口关闭 |
| `dart_remaining_time` | 0 | 飞镖发射倒计时 |
| `target_change_time` | 0 | 未切换目标 |
| `latest_launch_cmd_time` | 0 | 未发送发射指令 |
| `last_hit_target` | 0 | 当前模拟不更新命中目标 |
| `opponent_hit_count` | 0 | 当前模拟不更新被击中计数 |
| `selected_target` | 0 | 默认前哨站或未选定 |

进入比赛中时会重置：

| 字段/状态 | 重置值 |
| --- | --- |
| `game_progress` | 4 |
| `game_time_remaining` | 100 |
| `last_hit_target` | 0 |
| `opponent_hit_count` | 0 |
| `selected_target` | 0 |
| `in_game_elapsed_ms_` | 0 |
| `dart_phase_elapsed_ms_` | 0 |
| 第一次飞镖事件标志 | false |
| 第二次飞镖事件标志 | false |

进入结算时会设置：

| 字段/状态 | 值 |
| --- | --- |
| `game_progress` | 5 |
| `game_time_remaining` | 15 |
| `dart_opening_status` | 1 |
| `dart_remaining_time` | 0 |
| `latest_launch_cmd_time` | 0 |
| `selected_target` | 0 |

## 实现注意点

- 所有发送函数最终都会调用 `send_judge_system_data()`，该函数内有 1% 概率模拟丢包。
- `send_judge_system_data()` 当前在成功写入串口后没有显式 `return true;`，虽然调用方目前没有使用返回值，但这是一个实现细节风险。
- 目标切换打印当前会在比赛中第 3 秒内重复出现；如果只希望打印一次，可以增加类似 `target_change_triggered_` 的标志位。
