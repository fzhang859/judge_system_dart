#ifndef DART_SIMULATOR_HPP_
#define DART_SIMULATOR_HPP_

#include <cstdint>

enum class GamePhase
{
    Standby,
    Preparation,
    SelfCheck,
    Countdown,
    InGame,
    Settlement,
};

enum class DartPhase
{
    Closed,
    Opening,
    Opened,
    Countdown,
    Closing,
    NotManaged,
};

struct DartSimulatorSnapshot
{
    uint8_t game_progress = 0;
    uint16_t game_time_remaining = 0;

    uint8_t dart_opening_status = 1;
    uint8_t dart_remaining_time = 0;
    uint16_t target_change_time = 0;
    uint16_t latest_launch_cmd_time = 0;

    uint8_t last_hit_target = 0;
    uint8_t opponent_hit_count = 0;
    uint8_t selected_target = 0;
};

class DartSimulator
{
public:
    explicit DartSimulator(int dart_target_type);

    bool update(int elapsed_ms);
    const DartSimulatorSnapshot &snapshot() const;

private:
    void update_game_timer(int elapsed_ms);
    void update_game_phase();
    void update_dart_phase(int elapsed_ms);
    void start_dart_event();

    int dart_target_type_ = 0;
    GamePhase game_phase_ = GamePhase::Standby;
    DartPhase dart_phase_ = DartPhase::Closed;
    DartSimulatorSnapshot snapshot_;

    int game_second_accumulated_ms_ = 0;
    int in_game_elapsed_ms_ = 0;
    int dart_phase_elapsed_ms_ = 0;
    bool first_dart_event_triggered_ = false;
    bool second_dart_event_triggered_ = false;
    bool sent_launch_cmd_ = false;
};

#endif
