#include <ee_timers.hpp>
#include <intc.hpp>
#include <cassert>
#include <cstdio>

static const char* REGS[] =
{
	"TN_COUNT",
	"TN_MODE",
	"TN_COMP",
	"TN_HOLD"
};

static const char* CLOCK[] =
{
	"BUSCLK",
	"BUSCLK / 16",
	"BUSCLK / 256",
	"HBLANK"
};

Timers::Timers(INTC* intc)
: intc(intc)
{}

uint32_t Timers::read(uint32_t addr)
{
    assert((addr & 0xff) <= 0x30);

    int num = (addr & 0xff00) >> 11;
    uint32_t offset = (addr & 0xf0) >> 4;
    uint32_t* ptr = (uint32_t*)&timers[num] + offset;

    printf("[EE Timer]: Read %s (0x%08X) [%s]\n", REGS[offset], *ptr, timers[num].mode.enable ? "enabled" : "disabled");

    return *ptr;
}

void Timers::write(uint32_t addr, uint32_t data)
{
    assert((addr & 0xff) <= 0x30);

    int num = (addr & 0xff00) >> 11;
    uint32_t offset = (addr & 0xf0) >> 4;
    auto ptr = (uint32_t*)&timers[num] + offset;

    if (offset == 1)
    {
        auto& timer = timers[num];

        switch (data & 0x3)
        {
        case 0:
            timer.ratio = 1;
            break;
        case 1:
            timer.ratio = 16;
            break;
        case 2:
            timer.ratio = 256;
            break;
        case 3:
            timer.ratio = BUS_CLOCK / HBLANK_NTSC;
            break;
        }

        data &= 0x3ff;
    }

    *ptr = data;
}

void Timers::tick(uint32_t cycles)
{
    for (uint32_t i = 0; i < 3; i++)
    {
        auto& timer = timers[i];

        if (!timer.mode.enable)
            continue;
        
        uint32_t old_count = timer.counter;
        timer.counter += cycles * timer.ratio;
        
        if (timer.counter >= timer.compare && old_count < timer.compare)
        {
            if (timer.mode.cmp_intr_enable && !timer.mode.cmp_flag)
            {
                intc->trigger(Interrupt::INT_TIMER0 + i);
                timer.mode.cmp_flag = 1;
            }

            if (timer.mode.clear_when_cmp)
            {
                timer.counter = 0;
            }
        }

        if (timer.counter > 0xffff)
        {
            if (timer.mode.overflow_intr_enable && !timer.mode.overflow_flag)
            {
                intc->trigger(Interrupt::INT_TIMER0 + i);
                timer.mode.overflow_flag = 1;
            }

            timer.counter -= 0xffff;
        }
    }
}