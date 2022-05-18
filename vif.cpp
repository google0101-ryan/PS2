#include <vif.h>
#include <Bus.hpp>
#include <vu.hpp>
#include <cassert>
#include <algorithm>

constexpr const char* VIF_REGS[] =
{
    "VIF_STAT", "VIF_FBRST",
    "VIF_ERR", "VIF_MARK",
    "VIF_CYCLE", "VIF_MODE",
    "VIF_NUM", "VIF_MASK"
};

constexpr const char* UNPACK_FORMATS[4][4] =
{
    {"S-32", "V2-32", "V3-32", "V4-32"},
    {"S-16", "V2-16", "V3-16", "V4-16"},
    {"S-8",  "V2-8",  "V3-8",  "V4-8"},
    {"V4-5"}
};

VIF::VIF(Bus* parent, int n)
: bus(parent), id(n)
{}

void VIF::tick(uint32_t cycles)
{
    word_cycles = cycles * 4;
    while (!fifo.empty() && word_cycles--)
    {
        if (!subpacket_count)
            process_command();
        else
            execute_command();
    }
}

void VIF::reset()
{
    status = {};
    fbrst = {};
    cycle = {};
    err = mark = mode = num = 0;
    mask = code = itops = 0;
    base = ofst = tops = itop = top = 0;
    rn = {};
    cn = {};
    fifo = {};
    command = {};
    subpacket_count = address = qwords_written = word_cycles = 0;
    write_mode = WriteMode::Skipping;
}

uint32_t VIF::read(uint32_t address)
{
    auto offset = (address >> 4) & 0xF;
    uint32_t data = 0;
    switch (offset)
    {
    case 0:
        status.fifo_count = fifo.size<uint128_t>();
        data = status.value;
        break;
    default:
        printf("[VIF%d]: Read from unknown reg %s\n", id, VIF_REGS[offset]);
        exit(1);
    }
    return data;
}

void VIF::write(uint32_t address, uint32_t data)
{
    auto offset = (address >> 4) & 0xF;
    switch (offset)
    {
    case 0:
        status.fifo_detection = data & 0x800000;
        break;
    case 1:
        fbrst.value = data;

        if (fbrst.reset)
            reset();
        break;
    case 2:
        err = data;
        break;
    case 3:
        mark = data;
        status.mark = 0;
        break;
    default:
        printf("[VFU%d]: Writing to unknown register %s\n", id, VIF_REGS[offset]);
        exit(1);
    }
}

void VIF::process_command()
{
    if (fifo.read(&command.value))
    {
        auto immediate = command.immediate;
        switch (command.command)
        {
        case VIFCommands::VNOP:
            break;
        case VIFCommands::STCYCL:
            cycle.value = immediate;
            break;
        case VIFCommands::OFFSET:
            ofst = immediate & 0x3FF;
            status.double_buffer_flag = 0;
            base = tops;
            break;
        case VIFCommands::BASE:
            base = immediate & 0x3FF;
            break;
        case VIFCommands::ITOP:
            itop = immediate & 0x3FF;
            break;
        case VIFCommands::STMOD:
            mode = immediate & 0x3;
            break;
        case VIFCommands::MSKPATH3:
            break;
        case VIFCommands::MARK:
            mark = immediate;
            break;
        case VIFCommands::FLUSHE:
            break;
        case VIFCommands::STMASK:
            subpacket_count = 1;
            break;
        case VIFCommands::STROW:
            subpacket_count = 4;
            break;
        case VIFCommands::STCOL:
            subpacket_count = 4;
            break;
        case VIFCommands::MPG:
            subpacket_count = command.num != 0 ? command.num * 2 : 512;
            address = command.immediate * 8;
            break;
        case VIFCommands::UNPACKSTART ... VIFCommands::UNPACKEND:
            process_command();
            break;
        default:
            printf("[VIF%d]: Unknown command 0x%X\n", id, command.command);
            exit(1);
        }
        fifo.pop<uint32_t>();
    }
}

void VIF::process_unpack()
{
    address = command.address * 16;
    address += command.flg ? tops * 16 : 0;

    auto bit_size = (32 >> command.vl) * (command.vn + 1);
    auto word_count = ((bit_size + 0x1F) & ~0x1F) / 32;
    subpacket_count = word_count * command.num;
    write_mode = cycle.cycle_length < cycle.write_cycle_length ? WriteMode::Filling : WriteMode::Skipping;

    qwords_written = 0;
}

void VIF::execute_command()
{
    uint32_t data;
    if (fifo.read(&data))
    {
        switch (command.command)
        {
        case VIFCommands::STMASK:
            mask = data;
            break;
        case VIFCommands::STROW:
            rn[4 - subpacket_count] = data;
            break;
        case VIFCommands::STCOL:
            cn[4 - subpacket_count] = data;
            break;
        case VIFCommands::MPG:
            assert(id);
            bus->vu[id]->write<Memory::Code>(address, data);
            address += 4;
            break;
        case VIFCommands::UNPACKSTART ... VIFCommands::UNPACKEND:
            unpack_packet();
            return;
        }

        subpacket_count--;
        fifo.pop<uint32_t>();
    }
}

void VIF::unpack_packet()
{
    auto format = command.command & 0xF;
    uint128_t qword = 0;
    switch (format)
    {
    case VIFUFormat::S_32:
    {
        uint32_t data;
        if (fifo.read(&data))
        {
            uint32_t* words = (uint32_t*)&qword;
            words[0] = words[1] = words[2] = words[3] = data;

            subpacket_count--;
            fifo.pop<uint32_t>();
            break;
        }
        return;
    }
    case VIFUFormat::V4_32:
    {
        if (fifo.read(&qword))
        {
            subpacket_count -= 4;
            word_cycles = word_cycles >= 4 ? word_cycles - 3 : 0;
            fifo.pop<uint128_t>();
            break;
        }
        return;
    }
    default:
        printf("[VIF%d]: Unknown unpack format %d\n", id, format);
        exit(1);
    }

    bus->vu[id]->write<Memory::Data>(address, qword);

    address += 16;
    qwords_written++;

    if (write_mode == WriteMode::Skipping)
    {
        if (qwords_written >= cycle.cycle_length)
            address += (cycle.cycle_length - qwords_written) * 16;
    }
    else
        return;
}