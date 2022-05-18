#include <sif.hpp>
#include <stdio.h>
#include <Bus.hpp>

constexpr const char* REGS[] =
{
	"SIF_MSCOM", "SIF_SMCOM", "SIF_MSFLG",
	"SIF_SMFLG", "SIF_CTRL", "", "SIF_BD6"
};

constexpr const char* COMP[] =
{
	"IOP", "EE"
};

SIF::SIF(Bus* bus)
: bus(bus)
{}

uint32_t SIF::read(uint32_t addr)
{
    auto comp = (addr >> 9) & 0x1;
    uint16_t offset = (addr >> 4) & 0xF;
    auto ptr = (uint32_t*)&regs + offset;

    //printf("[SIF][%s]: Read 0x%08X from %s\n", COMP[comp], *ptr, REGS[offset]);

    return *ptr;
}

void SIF::write(uint32_t addr, uint32_t data)
{
    auto comp = (addr >> 9) & 0x1;
    uint16_t offset = (addr >> 4) & 0xF;
    auto ptr = (uint32_t*)&regs + offset;

    printf("[SIF][%s]: Writing 0x%08X to %s\n", COMP[comp], data, REGS[offset]);

    if (offset == 4)
    {
        auto& control = regs.ctrl;
        if (comp == 0)
        {
            uint8_t temp = data & 0xF0;
            if (data & 0xA0)
            {
                control &= ~0xF000;
                control |= 0x2000;
            }

            if (control & temp)
                control &= ~temp;
            else
                control |= temp;
        }
        else
        {
            if (!(data & 0x100))
                control &= ~0x100;
            else
                control |= 0x100;
        }
    }
    else
    {
        *ptr = data;
    }
}