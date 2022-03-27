#include <dmac.hpp>
#include <Bus.hpp>
#include <EE.hpp>
#include <sif.hpp>
#include <gs/gif.hpp>
#include <cassert>

inline uint32_t get_channel(uint32_t value)
{
    switch (value)
    {
    case 0x80: return 0;
    case 0x90: return 1;
    case 0xA0: return 2;
    case 0xB0: return 3;
    case 0xB4: return 4;
    case 0xC0: return 5;
    case 0xC4: return 6;
    case 0xC8: return 7;
    case 0xD0: return 8;
    case 0xD4: return 9;
    default:
        printf("[DMAC]: Invalid channel id %d\n", value);
        exit(1);
    }
}

constexpr const char* REGS[] =
{
	"Dn_CHCR",
	"Dn_MADR",
	"Dn_QWC",
	"Dn_TADR",
	"Dn_ASR0",
	"Dn_ASR1",
	"", "",
	"Dn_SADR"
};

constexpr const char* GLOBALS[] =
{
	"D_CTRL",
	"D_STAT",
	"D_PCR",
	"D_SQWC",
	"D_RBSR",
	"D_RBOR",
	"D_STADT",
};

DMAController::DMAController(Bus* parent, EmotionEngine* _cpu)
: bus(parent),
cpu(_cpu)
{}

uint32_t DMAController::read_channel(uint32_t addr)
{
    assert((addr & 0xff) <= 0x80);

    uint32_t id = (addr >> 8) & 0xFF;
    uint32_t channel = get_channel(id);
    uint32_t offset = (addr >> 4) & 0xF;
    auto ptr = (uint32_t*)&channels[channel] + offset;

    return *ptr;
}

void DMAController::write_channel(uint32_t addr, uint32_t data)
{
    assert((addr & 0xff) <= 0x80);

    uint32_t id = (addr >> 8) & 0xFF;
    uint32_t channel = get_channel(id);
    uint32_t offset = (addr >> 4) & 0xF;
    auto ptr = (uint32_t*)&channels[channel] + offset;

    if (offset == 1)
        data &= 0x01FFFFF0;
    *ptr = data;

    if (channels[channel].control.running)
    {
        printf("[DMAC]: Transfer for channel %d started\n", channel);
    }
}

uint32_t DMAController::read_global(uint32_t addr)
{
    assert(addr <= 0x1000E060);

    uint32_t offset = (addr >> 4) & 0xF;
    auto ptr = (uint32_t*)&globals + offset;

    return *ptr;
}

uint32_t DMAController::read_enabler(uint32_t addr)
{
    assert(addr == 0x1000F520);

    return globals.d_enable;
}

void DMAController::write_global(uint32_t addr, uint32_t data)
{
    assert(addr <= 0x1000E060);

    uint32_t offset = (addr >> 4) & 0xF;
    auto ptr = (uint32_t*)&globals + offset;

    if (offset == 1)
    {
        auto& cop0 = cpu->cop0;

        globals.d_stat.clear &= ~(data & 0xFFFF);
        globals.d_stat.reverse ^= (data >> 16);

        cop0.cause.ip1_pending = globals.d_stat.channel_irq & globals.d_stat.channel_irq_mask;
    }
    else
        *ptr = data;
}

void DMAController::write_enabler(uint32_t addr, uint32_t data)
{
    assert(addr == 0x1000F590);

    globals.d_enable = data;
}

void DMAController::tick(uint32_t cycles)
{
    if (globals.d_enable & 0x10000)
        return;
    
    for (int cycle = cycles; cycle > 0; cycle--)
    {
        for (uint32_t id = 0; id < 10; id++)
        {
            auto& channel = channels[id];
            if (channel.control.running)
            {
                if (channel.qword_count > 0)
                {
                    switch (id)
                    {
                    case DMAChannels::VIF0:
                    case DMAChannels::VIF1:
                        printf("[DMAC]: Unimplemented VIF%d channel\n", id);
                        exit(1);
                    case DMAChannels::GIFC:
                    {
                        auto& gif = bus->gif;
                        uint128_t qword = *(uint128_t*)&bus->eeRam[channel.address];

                        if (gif->write_path3(NULL, qword))
                        {
                            uint64_t upper = qword >> 64, lower = qword;
                            channel.address += 16;
                            channel.qword_count--;

                            if (!channel.qword_count && !channel.control.mode)
                                channel.end_transfer = true;
                        }
                        break;
                    }
                    case DMAChannels::SIF0:
                    {
                        auto& sif = bus->sif;
                        if (sif->sif0_fifo.size() >= 4)
                        {
                            uint32_t data[4];
                            for (int i = 0; i < 4; i++)
                            {
                                data[i] = sif->sif0_fifo.front();
                                sif->sif0_fifo.pop();
                            }
                            uint128_t qword = *(uint128_t*)data;
                            uint64_t upper = (qword >> 64), lower = qword;

                            bus->Write64(channel.address, qword, true);

                            channel.qword_count--;
                            channel.address += 16;
                        }
                        break;
                    }
                    case DMAChannels::SIF1:
                    {
                        auto& sif = bus->sif;
                        uint128_t qword = *(uint128_t*)&bus->eeRam[channel.address];
                        uint32_t* data = (uint32_t*)&qword;
                        for (int i = 0; i < 4; i++)
                        {
                            sif->sif1_fifo.push(data[i]);
                        }

                        channel.qword_count--;
                        channel.address += 16;
                        break;
                    }
                    default:
                        printf("[DMAC]: Unknown channel %d\n", id);
                        exit(1);
                    }
                }
                else if (channel.end_transfer)
                {
                    channel.end_transfer = false;
                    channel.control.running = 0;

                    globals.d_stat.channel_irq |= (1 << id);

                    if (globals.d_stat.channel_irq & globals.d_stat.channel_irq_mask)
                    {
                        cpu->cop0.cause.ip1_pending = 1;
                    }
                }
                else
                {
                    fetch_tag(id);
                }
            }
        }
    }
}

void DMAController::fetch_tag(uint32_t id)
{
    DMATag tag;
    auto& channel = channels[id];
    switch (id)
    {
    case DMAChannels::GIFC:
    {
        assert(!channel.tag_address.mem_select);

        auto& gif = bus->gif;
        auto address = channel.tag_address.address;

        tag.value = *(uint128_t*)&bus->eeRam[address];

        channel.qword_count = tag.qwords;
        channel.control.tag = (tag.value >> 16) & 0xFFFF;

        uint16_t tag_id = tag.id;
        switch (tag_id)
        {
        case DMASourceID::REFE:
            channel.address = tag.address;
            channel.tag_address.value += 16;
            channel.end_transfer = true;
            break;
        case DMASourceID::CNT:
            channel.address = channel.tag_address.address;
            channel.tag_address.value = channel.address + channel.qword_count * 16;
            break;
        case DMASourceID::END:
            channel.address = channel.tag_address.address + 16;
            channel.end_transfer = true;
            break;
        default:
            printf("[DMAC]: Unrecognized GIF DMATag id %d\n", tag_id);
            exit(1);
        }
        if (channel.control.enable_irq_bit && tag.irq)
            channel.end_transfer = true;
        break;
    }
    case DMAChannels::VIF0:
    case DMAChannels::VIF1:
        printf("[DMAC]: Unknown VIF tag id!\n");
        exit(1);
    case DMAChannels::SIF0:
    {
        auto& sif = bus->sif;
        if (sif->sif0_fifo.size() >= 2)
        {
            uint32_t data[2] = {};
            for (int i = 0; i < 2; i++)
            {
                data[i] = sif->sif0_fifo.front();
                sif->sif0_fifo.pop();
            }

            tag.value = *(uint64_t*)data;

            channel.qword_count = tag.qwords;
            channel.control.tag = (tag.value >> 16) & 0xFFFF;
            channel.address = tag.address;
            channel.tag_address.address += 16;

            if (channel.control.enable_irq_bit && tag.irq)
                channel.end_transfer = true;
        }
        break;
    }
    case DMAChannels::SIF1:
    {
        assert(channel.control.mode == 1);
        assert(!channel.tag_address.mem_select);

        auto address = channel.tag_address.address;

        tag.value = *(uint128_t*)&bus->eeRam[address];

        channel.qword_count = tag.qwords;
        channel.control.tag = (tag.value >> 16) & 0xFFFF;

        uint16_t tag_id = tag.id;
        switch (tag_id)
        {
        case DMASourceID::REFE:
        {
            channel.address = tag.address;
            channel.tag_address.value += 16;
            channel.end_transfer = true;
            break;
        }
        case DMASourceID::NEXT:
        {
            channel.address = channel.tag_address.address;
            channel.tag_address.value = tag.address;
            break;
        }
        case DMASourceID::REF:
        {
            channel.address = channel.tag_address.address;
            channel.tag_address.value = tag.address;
            break;
        }
        default:
            printf("[DMAC]: Unrecognized SIF1 DMATag id %d\n", tag_id);
            exit(1);
        }
        if (channel.control.enable_irq_bit && tag.irq)
            channel.end_transfer = true;
        break;
    }
    default:
        printf("[DMAC]: Unknown channel %d\n", id);
        exit(1);
    }
}