#include "dart_simulator.hpp"
#include "judge_sender.hpp"
#include "serial_port.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <getopt.h>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

namespace
{
constexpr const char *kDefaultUartDevice = "/dev/ttyUSB0";
constexpr int kDefaultSendIntervalMs = 100;
constexpr uint8_t kDefaultGameType = 1;

uint64_t get_current_timestamp_us()
{
    const auto now = std::chrono::high_resolution_clock::now();
    const auto us = std::chrono::time_point_cast<std::chrono::microseconds>(now).time_since_epoch().count();
    return static_cast<uint64_t>(us);
}

void print_game_status(uint8_t game_progress, uint16_t remain_time)
{
    std::cout << "====================================" << std::endl;
    std::cout << "当前比赛状态: ";
    switch (game_progress)
    {
    case 0:
        std::cout << "未开始比赛";
        break;
    case 1:
        std::cout << "准备阶段";
        break;
    case 2:
        std::cout << "裁判系统自检阶段";
        break;
    case 3:
        std::cout << "五秒倒计时";
        break;
    case 4:
        std::cout << "比赛中";
        break;
    case 5:
        std::cout << "比赛结算中";
        break;
    default:
        std::cout << "未知状态";
        break;
    }
    std::cout << " | 剩余时间: " << remain_time << "秒" << std::endl;
    std::cout << "====================================" << std::endl;
}

void print_dart_status(uint8_t opening_status, uint8_t remaining_time, uint16_t latest_launch_cmd_time, uint8_t last_hit_target, uint8_t opponent_hit_count, uint8_t selected_target)
{
    std::cout << "飞镖发射口状态: ";
    switch (opening_status)
    {
    case 0:
        std::cout << "已经开启";
        break;
    case 1:
        std::cout << "关闭";
        break;
    case 2:
        std::cout << "正在开启或者关闭中";
        break;
    default:
        std::cout << "未知状态";
        break;
    }

    if (remaining_time > 0)
    {
        std::cout << " | 倒计时: " << static_cast<int>(remaining_time) << "秒" << std::endl;
    }
    else
    {
        std::cout << std::endl;
    }

    std::cout << "最新发射指令时间: " << latest_launch_cmd_time << "秒" << std::endl;
    std::cout << "最近一次己方飞镖击中目标: " << static_cast<int>(last_hit_target) << std::endl;
    std::cout << "对方最近被击中目标累计被击中计数: " << static_cast<int>(opponent_hit_count) << std::endl;
    std::cout << "飞镖选定的击打目标: " << static_cast<int>(selected_target) << std::endl;
}

void print_help(const char *program)
{
    std::cout << "Usage: " << program << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help              显示帮助信息" << std::endl;
    std::cout << "  -p, --port DEV          指定串口设备 (默认: /dev/ttyUSB0)" << std::endl;
    std::cout << "  -i, --interval N        设置发送间隔(ms) (默认: 100)" << std::endl;
    std::cout << "  -o, --once              模拟一轮后退出" << std::endl;
    std::cout << "  -d, --dart-target N     设置飞镖目标 (0: 前哨站, 1: 基地固定目标, 2: 基地随机目标, 3: 基地随机移动目标)" << std::endl;
}
}

int main(int argc, char **argv)
{
    std::string port_device = kDefaultUartDevice;
    int send_interval_ms = kDefaultSendIntervalMs;
    int dart_target_type = 0;
    bool once = false;

    const option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"port", required_argument, nullptr, 'p'},
        {"interval", required_argument, nullptr, 'i'},
        {"once", no_argument, nullptr, 'o'},
        {"dart-target", required_argument, nullptr, 'd'},
        {nullptr, 0, nullptr, 0},
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "hp:i:od:", long_options, nullptr)) != -1)
    {
        switch (opt)
        {
        case 'h':
            print_help(argv[0]);
            return 0;
        case 'p':
            port_device = optarg;
            break;
        case 'i':
            send_interval_ms = std::atoi(optarg);
            if (send_interval_ms < 10)
            {
                std::cerr << "Interval too small. Using 10ms." << std::endl;
                send_interval_ms = 10;
            }
            break;
        case 'o':
            once = true;
            break;
        case 'd':
            dart_target_type = std::atoi(optarg);
            if (dart_target_type < 0 || dart_target_type > 3)
            {
                std::cerr << "Invalid dart target. Using default (0: 前哨站)." << std::endl;
                dart_target_type = 0;
            }
            break;
        default:
            print_help(argv[0]);
            return 1;
        }
    }

    const int fd = uart_init(port_device.c_str());
    if (fd < 0)
    {
        std::cerr << "Failed to initialize UART on " << port_device << std::endl;
        return 1;
    }

    std::cout << "UART initialized successfully on " << port_device << std::endl;

    DartSimulator simulator(dart_target_type);
    auto last_update_time = std::chrono::high_resolution_clock::now();
    auto last_game_status_update_time = last_update_time;
    auto last_dart_info_update_time = last_update_time;
    auto last_dart_client_cmd_update_time = last_update_time;
    auto current_time = last_update_time;

    std::cout << "开始模拟裁判系统飞镖数据发送..." << std::endl;
    std::cout << "按 Ctrl+C 退出程序" << std::endl;

    try
    {
        while (true)
        {
            current_time = std::chrono::high_resolution_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update_time).count();

            if (elapsed >= send_interval_ms)
            {
                last_update_time = current_time;
                const bool round_finished = simulator.update(static_cast<int>(elapsed));
                if (round_finished)
                {
                    if (once)
                    {
                        close(fd);
                        return 0;
                    }
                    std::cout << "\n3秒后重新开始比赛..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }

                const DartSimulatorSnapshot &snapshot = simulator.snapshot();
                const auto game_send_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    current_time - last_game_status_update_time).count();
                if (game_send_elapsed >= 1)
                {
                    last_game_status_update_time = current_time;
                    send_game_status(fd, kDefaultGameType, snapshot.game_progress, snapshot.game_time_remaining, get_current_timestamp_us());
                }

                const auto dart_info_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    current_time - last_dart_info_update_time).count();
                if (dart_info_elapsed >= 1)
                {
                    last_dart_info_update_time = current_time;
                    send_dart_info(fd, snapshot.dart_remaining_time, snapshot.last_hit_target, snapshot.opponent_hit_count,
                                   snapshot.selected_target);
                }

                const auto dart_cmd_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_dart_client_cmd_update_time).count();
                if (dart_cmd_elapsed >= 333)
                {
                    last_dart_client_cmd_update_time = current_time;
                    send_dart_client_cmd(fd, snapshot.dart_opening_status, snapshot.target_change_time,
                                         snapshot.latest_launch_cmd_time);
                }

                print_game_status(snapshot.game_progress, snapshot.game_time_remaining);
                print_dart_status(snapshot.dart_opening_status, snapshot.dart_remaining_time,
                                  snapshot.latest_launch_cmd_time, snapshot.last_hit_target,
                                  snapshot.opponent_hit_count, snapshot.selected_target);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    close(fd);
    return 0;
}
