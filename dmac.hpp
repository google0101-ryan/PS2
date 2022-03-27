#pragma once

#include <int128.h>

class Bus;
class EmotionEngine;

constexpr uint32_t DMATAG_END = 0x7;

union TagAddr
{
    uint32_t value;
    struct
    {
        uint32_t address : 30;
        uint32_t mem_select : 1;
    };
};

union DMATag
{
    uint128_t value;
    struct
    {
        uint128_t qwords : 16;
        uint128_t : 10;
        uint128_t priority : 2;
        uint128_t id : 3;
        uint128_t irq : 1;
        uint128_t address : 31;
        uint128_t mem_select : 1;
        uint128_t data : 64;
    };
};

union DCHCR
{
    uint32_t value;
    struct
    {
        uint32_t direction : 1;
        uint32_t : 1;
        uint32_t mode : 2;
        uint32_t stack_ptr : 2;
        uint32_t transfer_tag : 1;
        uint32_t enable_irq_bit : 1;
        uint32_t running : 1;
        uint32_t : 7;
        uint32_t tag : 16;
    };
};

struct DMACChannel
{
    DCHCR control;
    uint32_t address;
    uint32_t qword_count;
    TagAddr tag_address;
    TagAddr saved_tag_address[2];
    uint32_t padding[2];
    uint32_t scratchpad_address;
    bool end_transfer = false;
};

union DSTAT
{
    uint32_t value;
    struct
    {
        uint32_t channel_irq : 10;
        uint32_t : 3;
        uint32_t dma_stall : 1;
        uint32_t mfifo_empty : 1;
        uint32_t bus_error : 1;
        uint32_t channel_irq_mask : 10;
        uint32_t : 3;
        uint32_t stall_irq_mask : 1;
        uint32_t mfifo_irq_mask : 1;
        uint32_t : 1;
    };
    struct
    {
        uint32_t clear : 16;
        uint32_t reverse : 16;
    };
};

struct DMACGlobals
{
    uint32_t d_ctrl;
    DSTAT d_stat;
    uint32_t d_pcr;
    uint32_t d_sqwc;
    uint32_t d_rbsr;
    uint32_t d_rbor;
    uint32_t d_stadr;
    uint32_t d_enable = 0x1201;
};

enum DMAChannels : uint32_t
{
    VIF0,
    VIF1,
    GIFC,
    IPU_FROM,
    IPU_TO,
    SIF0,
    SIF1,
    SIF2,
    SPR_FROM,
    SPR_TO
};

enum DMASourceID : uint32_t
{
    REFE,
    CNT,
    NEXT,
    REF,
    REFS,
    CALL,
    RET,
    END
};

class DMAController
{
public:
    DMAController(Bus* parent, EmotionEngine* _cpu);
    ~DMAController() = default;

    uint32_t read_channel(uint32_t addr);
    uint32_t read_global(uint32_t addr);
    uint32_t read_enabler(uint32_t addr);

    void write_channel(uint32_t addr, uint32_t data);
    void write_global(uint32_t addr, uint32_t data);
    void write_enabler(uint32_t addr, uint32_t data);

    void tick(uint32_t cycles);
private:
    void fetch_tag(uint32_t id);
private:
    Bus* bus;
    EmotionEngine* cpu;

    DMACChannel channels[10] = {};
    DMACGlobals globals = {};
};