#include <ipu.h>
#include <Bus.hpp>
#include <cassert>

IPU::IPU(Bus* parent)
: bus(bus)
{}

uint64_t IPU::read(uint32_t addr)
{
    assert(addr <= 0x10002030);

    uint32_t offset = (addr & 0xF0) >> 4;

    auto ptr = (uint64_t*)&regs + offset - 1;
    auto output = (!offset ? get_command_result() : *ptr);

    return output;
}

void IPU::write(uint32_t addr, uint64_t data)
{
    uint32_t offset = (addr & 0xF0) >> 4;

    if (!offset)
    {
        IPUCommand command;
        command.value = data & 0xFFFFFFFF;
        decode_command(command);
    }
    else
    {
        auto ptr = (uint64_t*)&regs + offset - 1;
        *ptr = data;
    }
}

uint128_t IPU::read_fifo(uint32_t)
{
    return uint128_t();
}

void IPU::write_fifo(uint32_t, uint128_t){}

void IPU::decode_command(IPUCommand cmd)
{
    uint32_t opcode = cmd.code;
    switch (opcode)
    {
    default:
        printf("[IPU] Unknown command %d\n", opcode);
    }
}

uint64_t IPU::get_command_result() {return 0;}