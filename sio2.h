#pragma once

#include <queue>
#include <cstdint>

class Bus;

enum class SIO2Peripheral
{
    None = 0x0,
    Controller = 0x1,
    Multitap = 0x21,
    Infrared = 0x61,
    MemCard = 0x81
};

struct SIO2Command
{
    uint32_t index;
    uint32_t size;
    bool running = false;
};

class SIO2
{
public:
    SIO2(Bus* parent);
    ~SIO2() = default;

    uint32_t read(uint32_t address);
    void write(uint32_t address, uint32_t data);

    void upload_command(uint8_t cmd);
    uint8_t read_fifo();
private:
    Bus* bus;

    uint32_t sio2_ctrl = 0;
    uint32_t send1_2[8] = {}, send3[16] = {};

    SIO2Command command = {};
    SIO2Peripheral current_device;
    std::queue<uint8_t> sio2_fifo;
};