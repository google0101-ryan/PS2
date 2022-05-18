#pragma once
#include <cstdint>

enum class PadButton
{
    SELECT,
    L3,
    R3,
    START,
    UP,
    RIGHT,
    DOWN,
    LEFT,
    L2,
    R2,
    L1,
    R1,
    TRIANGLE,
    CIRCLE,
    CROSS,
    SQUARE
};

enum PadCommand
{
    SET_VREF_PARAM = 0x40,
    QUERY_BUTTON_MASK = 0x41,
    READ_DATA = 0x42,
    CONFIG_MODE = 0x43,
    SET_MAIN_MODE = 0x44,
    QUERY_MODEL = 0x45,
    QUERY_ACT = 0x46,
    QUERY_COMB = 0x47,
    QUERY_MODE = 0x4C,
    SET_ACT_ALIGN = 0x4D,
    SET_BUTTON_INFO = 0x4F
};

enum class PadMode : uint8_t
{
    Digital,
    Analog
};

class Gamepad;
using Response = void(Gamepad::*)(uint8_t);

class Gamepad
{
public:
    Gamepad();

    void press_button(PadButton button);
    void release_button(PadButton button);

    uint8_t write_byte(uint8_t byte);
    uint8_t process_command(uint8_t cmd);
private:
    void set_response(uint16_t byte_id, Response resp);
    void switch_mode(uint8_t mode);
    void read_buttons(uint8_t cmd);
    void set_config(uint8_t value);
    void query(uint8_t half);
    void query_mode(uint8_t index);
public:
    Response response = nullptr;
    static uint8_t responses[16][18];
    int written = 0;
    int custom_response = -1;
    uint8_t current_response = 0;
    PadMode mode = PadMode::Digital;
    uint16_t buttons = 0xFFFF;
    uint8_t command = 0;
    bool config_mode = false;
};