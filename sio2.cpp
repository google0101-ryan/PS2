#include <sio2.h>
#include <gamepad.h>
#include <Bus.hpp>

Gamepad gamepad;

SIO2::SIO2(Bus* parent)
: bus(parent)
{
    current_device = SIO2Peripheral::None;
}

uint32_t SIO2::read(uint32_t address)
{
    switch (address & 0xFF)
    {
    case 0x64: return read_fifo();
    case 0x68: return sio2_ctrl;
    case 0x6C: return 0x0D102;
    case 0x70: return 0xF;
    case 0x74: return 0;
    default:
        printf("[SIO2]: Read from unknown address 0x%08X\n", address);
        exit(1);
    }
}

void SIO2::write(uint32_t address, uint32_t data)
{
    switch (address & 0xFF)
    {
    case 0x00 ... 0x3F:
    {
        int offset = (address & 0x3F) / 4;
        send3[offset] = data;
        break;
    }
    case 0x40 ... 0x5F:
    {
        bool send2 = address & 0x4;
        int offset = (address & 0x1F) / 8;
        send1_2[offset + send2 * 4] = data;
        break;
    }
    case 0x60:
    {
        upload_command(data);
        break;
    }
    case 0x68:
    {
        sio2_ctrl = data;
        if (data & 0x1)
        {
            printf("[SIO2]: Need to enable intr\n");
            sio2_ctrl &= ~0x1;
        }
        if (data & 0xC)
        {
            command = {};
            current_device = SIO2Peripheral::None;
        }
        break;
    }
    }
}

void SIO2::upload_command(uint8_t cmd)
{
    bool just_started = false;
    if (!command.size)
    {
        auto params = send3[command.index];
        if (!params)
        {
            printf("[SIO2]: SEND3 parameter empty!\n");
            exit(1);
        }

        command.size = (params >> 8) & 0x1FF;
        command.index++;

        current_device = (SIO2Peripheral)cmd;
        just_started = true;
    }

    command.size--;

    switch (current_device)
    {
    case SIO2Peripheral::Controller:
    {
        if (just_started) gamepad.written = 0;
        auto reply = gamepad.write_byte(cmd);

        sio2_fifo.push(reply);
        break;
    }
    case SIO2Peripheral::MemCard:
    {
        sio2_fifo.push(0xFF);
        break;
    }
    }
}

uint8_t SIO2::read_fifo()
{
    auto data = sio2_fifo.front();
    sio2_fifo.pop();
    return data;
}