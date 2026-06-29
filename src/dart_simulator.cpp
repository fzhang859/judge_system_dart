#include "dart_simulator.hpp"

#include <algorithm>
#include <iostream>

namespace
{
constexpr int kPreparationSeconds = 3;
constexpr int kSelfCheckSeconds = 15;
constexpr int kCountdownSeconds = 5;
constexpr int kGameSeconds = 100;
constexpr int kSettlementSeconds = 15;
constexpr int kDartOpenCloseSeconds = 7;
constexpr int kDartLaunchCountdownSeconds = 20;
}

DartSimulator::DartSimulator(int dart_target_type)
    : dart_target_type_(dart_target_type)
{
}

bool DartSimulator::update(int elapsed_ms)
{
    update_game_timer(elapsed_ms);
    update_game_phase();
    update_dart_phase(elapsed_ms);

    if (game_phase_ == GamePhase::Settlement && snapshot_.game_time_remaining == 0)
    {
        game_phase_ = GamePhase::Standby;
        game_second_accumulated_ms_ = 0;
        in_game_elapsed_ms_ = 0;
        return true;
    }

    return false;
}

const DartSimulatorSnapshot &DartSimulator::snapshot() const
{
    return snapshot_;
}

void DartSimulator::update_game_timer(int elapsed_ms)
{
    if (game_phase_ == GamePhase::Standby)
    {
        return;
    }

    game_second_accumulated_ms_ += elapsed_ms;
    while (game_second_accumulated_ms_ >= 1000 && snapshot_.game_time_remaining > 0)
    {
        --snapshot_.game_time_remaining;
        game_second_accumulated_ms_ -= 1000;
    }

    if (game_phase_ == GamePhase::InGame)
    {
        in_game_elapsed_ms_ += elapsed_ms;
    }
}

void DartSimulator::update_game_phase()
{
    const int total_game_seconds = in_game_elapsed_ms_ / 1000;

    switch (game_phase_)
    {
    case GamePhase::Standby:
        game_phase_ = GamePhase::Preparation;
        dart_phase_ = DartPhase::NotManaged;
        snapshot_.game_progress = 1;
        snapshot_.game_time_remaining = kPreparationSeconds;
        snapshot_.dart_opening_status = 1;
        std::cout << "比赛开始，进入准备阶段!" << std::endl;
        break;

    case GamePhase::Preparation:
        snapshot_.game_progress = 1;
        if (snapshot_.game_time_remaining == 0)
        {
            game_phase_ = GamePhase::SelfCheck;
            snapshot_.game_progress = 2;
            snapshot_.game_time_remaining = kSelfCheckSeconds;
            snapshot_.dart_opening_status = 2;
            std::cout << "准备阶段结束，进入裁判系统自检阶段!" << std::endl;
        }
        break;

    case GamePhase::SelfCheck:
        snapshot_.game_progress = 2;
        if (snapshot_.game_time_remaining == 10)
        {
            snapshot_.dart_opening_status = 0;
        }
        if (snapshot_.game_time_remaining == 3)
        {
            snapshot_.dart_opening_status = 2;
        }
        if (snapshot_.game_time_remaining == 0)
        {
            game_phase_ = GamePhase::Countdown;
            snapshot_.game_progress = 3;
            snapshot_.game_time_remaining = kCountdownSeconds;
            std::cout << "裁判系统自检完成，进入五秒倒计时!" << std::endl;
        }
        break;

    case GamePhase::Countdown:
        snapshot_.game_progress = 3;
        if (snapshot_.game_time_remaining == 3)
        {
            snapshot_.dart_opening_status = 1;
        }
        if (snapshot_.game_time_remaining == 0)
        {
            game_phase_ = GamePhase::InGame;
            dart_phase_ = DartPhase::Closed;
            snapshot_.game_progress = 4;
            snapshot_.game_time_remaining = kGameSeconds;
            snapshot_.last_hit_target = 0;
            snapshot_.opponent_hit_count = 0;
            snapshot_.selected_target = 0;
            in_game_elapsed_ms_ = 0;
            dart_phase_elapsed_ms_ = 0;
            first_dart_event_triggered_ = false;
            second_dart_event_triggered_ = false;
            std::cout << "比赛正式开始!" << std::endl;
        }
        break;

    case GamePhase::InGame:
        snapshot_.game_progress = 4;
        if (total_game_seconds == 3 && dart_target_type_ != 0)
        {
            snapshot_.selected_target = static_cast<uint8_t>(dart_target_type_);
            snapshot_.target_change_time = snapshot_.game_time_remaining;
            std::cout << "前哨站被击毁，切换飞镖目标!" << std::endl;
        }

        if (!first_dart_event_triggered_ && total_game_seconds >= 5)
        {
            start_dart_event();
            first_dart_event_triggered_ = true;
            std::cout << "\n飞镖事件触发! (60秒标记)" << std::endl;
        }

        if (!second_dart_event_triggered_ && total_game_seconds >= 45)
        {
            start_dart_event();
            second_dart_event_triggered_ = true;
            std::cout << "\n飞镖事件触发! (180秒标记)" << std::endl;
        }

        if (snapshot_.game_time_remaining == 0)
        {
            game_phase_ = GamePhase::Settlement;
            dart_phase_ = DartPhase::Closed;
            snapshot_.game_progress = 5;
            snapshot_.game_time_remaining = kSettlementSeconds;
            snapshot_.dart_opening_status = 1;
            snapshot_.dart_remaining_time = 0;
            snapshot_.latest_launch_cmd_time = 0;
            snapshot_.selected_target = 0;
            std::cout << "比赛结束，进入结算阶段!" << std::endl;
        }
        break;

    case GamePhase::Settlement:
        snapshot_.game_progress = 5;
        break;
    }
}

void DartSimulator::update_dart_phase(int elapsed_ms)
{
    dart_phase_elapsed_ms_ += elapsed_ms;
    const int dart_elapsed_seconds = dart_phase_elapsed_ms_ / 1000;

    switch (dart_phase_)
    {
    case DartPhase::Closed:
        snapshot_.dart_opening_status = 1;
        snapshot_.dart_remaining_time = 0;
        break;

    case DartPhase::Opening:
        snapshot_.dart_opening_status = 2;
        if (dart_elapsed_seconds >= kDartOpenCloseSeconds)
        {
            dart_phase_ = DartPhase::Opened;
            dart_phase_elapsed_ms_ = 0;
            snapshot_.dart_opening_status = 0;
            std::cout << "飞镖舱门已完全打开!" << std::endl;
        }
        break;

    case DartPhase::Opened:
        dart_phase_ = DartPhase::Countdown;
        dart_phase_elapsed_ms_ = 0;
        sent_launch_cmd_ = false;
        snapshot_.dart_opening_status = 0;
        snapshot_.dart_remaining_time = kDartLaunchCountdownSeconds;
        std::cout << "飞镖发射倒计时开始: 20秒" << std::endl;
        break;

    case DartPhase::Countdown:
        snapshot_.dart_opening_status = 0;
        snapshot_.dart_remaining_time = static_cast<uint8_t>(
            std::max(0, kDartLaunchCountdownSeconds - dart_elapsed_seconds));
        if (snapshot_.dart_remaining_time == kDartLaunchCountdownSeconds && !sent_launch_cmd_)
        {
            sent_launch_cmd_ = true;
            snapshot_.latest_launch_cmd_time = snapshot_.game_time_remaining;
        }
        if (dart_elapsed_seconds >= kDartLaunchCountdownSeconds)
        {
            dart_phase_ = DartPhase::Closing;
            dart_phase_elapsed_ms_ = 0;
            snapshot_.dart_opening_status = 2;
            std::cout << "飞镖发射倒计时结束，舱门开始关闭" << std::endl;
        }
        break;

    case DartPhase::Closing:
        snapshot_.dart_opening_status = 2;
        snapshot_.dart_remaining_time = 0;
        if (dart_elapsed_seconds >= kDartOpenCloseSeconds)
        {
            dart_phase_ = DartPhase::Closed;
            dart_phase_elapsed_ms_ = 0;
            snapshot_.dart_opening_status = 1;
            std::cout << "飞镖舱门已完全关闭!" << std::endl;
        }
        break;

    case DartPhase::NotManaged:
        break;
    }
}

void DartSimulator::start_dart_event()
{
    dart_phase_ = DartPhase::Opening;
    dart_phase_elapsed_ms_ = 0;
    snapshot_.dart_opening_status = 2;
}
