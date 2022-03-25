#include <sif.hpp>

constexpr const char* REGS[] =
{
    "SIF_MSCOM", "SIF_SMCOM", "SIF_MSFLG",
    "SIF_SMFLG", "SIF_CTRL", "", "SIF_BD6"
};

constexpr const char* COMP[] =
{
    "IOP", "EE"
};

SIF::SIF()
{}

uint32_t SIF::read(uint32_t addr)
{
    auto comp = (addr >> 9) & 0x1;
    uint16_t offset = (addr >> 4) & 0xf;
    auto ptr = (uint32_t*)&regs + offset;

    return *ptr;
}

void SIF::write(uint32_t addr, uint32_t data)
{
    auto comp = (addr >> 9) & 0x1;
    uint16_t offset = (addr >> 4) & 0xf;
    auto ptr = (uint32_t*)&regs + offset;

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
        *ptr = data;
}