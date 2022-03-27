#include <intc.hpp>
#include <EE.hpp>

static const char* REGS[2] =
{
    "INTC_STAT",
    "INTC_MASK"
};

INTC::INTC(EmotionEngine* ee)
: cpu(ee)
{
    regs = {};
}

uint64_t INTC::read(uint32_t addr)
{
    uint8_t offset = (addr >> 4) & 0xf;
    uint32_t* ptr = (uint32_t*)&regs + offset;

    return *ptr;
}

void INTC::write(uint32_t addr, uint64_t data)
{
    uint8_t offset = (addr >> 4) & 0xf;
    uint32_t* ptr = (uint32_t*)&regs + offset;

    *ptr = (offset ? *ptr ^ data : *ptr & ~data);

    cpu->cop0.cause.ip0_pending = (regs.intc_mask & regs.intc_stat);

    printf("[INTC]: Writing 0x%08lX to %s\n", data, REGS[offset]);
}

void INTC::trigger(uint32_t intr)
{
    //printf("[INTC]: Triggering interrupt %d\n", intr);
    regs.intc_stat |= (1 << intr);

    cpu->cop0.cause.ip0_pending = (regs.intc_mask & regs.intc_stat);
}

bool INTC::int_pending()
{
    auto& cop0 = cpu->cop0;

    bool int_enabled = cop0.status.eie && cop0.status.ie && !cop0.status.erl && !cop0.status.exl;

    bool pending = (cop0.cause.ip0_pending && cop0.status.im0) ||
            (cop0.cause.ip1_pending && cop0.status.im1) ||
            (cop0.cause.timer_ip_pending && cop0.status.im7);
    
    return int_enabled && pending;
}