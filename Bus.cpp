#include <Bus.hpp>
#include <fstream>

Bus::Bus(std::string biosFilePath, gs::GraphicsSynthesizer *_gs)
: gs(_gs),
sif(new SIF())
{
    std::ifstream file(biosFilePath, std::fstream::in | std::fstream::binary);

    if (!file)
        printf("[BUS]: Tried to load BIOS %s but it doesn't exist!\n", biosFilePath.c_str());

    file.unsetf(std::ios::skipws);
    file.read((char*)bios, 1204*4096);
    file.close();
    console.open("console.txt", std::ios::out);
    console.clear();

    printf("[BUS]: BIOS loaded successfully\n");
}

uint8_t Bus::Read8(uint32_t addr, bool ee)
{
    addr = addr & KUSEG_MASKS[addr >> 29];
    if (ee)
    {
        if (addr >= 0x1FC00000 && addr < 0x20000000)
            return bios[addr - 0x1FC00000];
        if (addr < 0x2000000)
            return eeRam[addr];
        if (addr == 0x1F803204)
            return 0;
        if (addr >= 0x70000000 && addr <= 0x700003fff)
            return eeScratchpad[addr - 0x70000000];
    }
    printf("[BUS]: Read8 from unknown addr 0x%08X\n", addr);
    exit(1);
}

uint16_t Bus::Read16(uint32_t addr, bool ee)
{
    addr = addr & KUSEG_MASKS[addr >> 29];
    if (ee)
    {
        if (addr >= 0x1FC00000 && addr < 0x20000000)
            return *(uint16_t*)&bios[addr - 0x1FC00000];
        if (addr >= 0x70000000 && addr <= 0x700003fff)
            return *(uint16_t*)&eeScratchpad[addr - 0x70000000];
    }
    printf("[BUS]: Read from unknown addr 0x%08X\n", addr);
    exit(1);
}

uint32_t Bus::Read32(uint32_t addr, bool ee)
{
    addr = addr & KUSEG_MASKS[addr >> 29];
    if (addr >= 0x1FC00000 && addr < 0x20000000)
        return *(uint32_t*)&bios[addr - 0x1FC00000];
    if (addr >= 0x70000000 && addr <= 0x700003fff)
        return *(uint32_t*)&eeScratchpad[addr - 0x70000000];
    if (addr < 0x2000000)
        return *(uint32_t*)&eeRam[addr];
    if (addr == 0x1000F430 || addr == 0x1000F400)
        return 0;
    else if (addr == 0x1000F130 || addr == 0x1000F410)
        return 0;
    if (addr == 0x1000F440)
    {
        uint8_t SOP = (MCH_RICM >> 6) & 0xF;
        uint8_t SA = (MCH_RICM >> 16) & 0xFFF;
        if (!SOP)
        {
            switch (SA)
            {
            case 0x21:
                if (rdram_sdevid < 2)
                {
                    rdram_sdevid++;
                    return 0x1F;
                }
                return 0;
            case 0x23:
                return 0x0D0D;
            case 0x24:
                return 0x0090;
            case 0x40:
                return MCH_RICM & 0x1F;
            }
        }
        return 0;
    }
    if (addr >= 0x10000000 && addr < 0x10002000)
    {
        return timers->read(addr);    
    }
    if (addr == 0x1000F000 || addr == 0x1000F010)
        return intc->read(addr);
    if (addr == 0x1F80141C)
        return 0;
    else if (addr >= 0x1C000000 && addr <= 0x1C200000)
        return *(uint32_t*)&iopRam[addr & 0x1FFFFF];
    else if (addr >= 0x10008000 && addr < 0x1000E000)
        return dmac->read_channel(addr);
    switch (addr)
    {
    case 0x10003000:
    case 0x10003010:
    case 0x10003020:
    case 0x10003030:
    case 0x10003040:
    case 0x10003050:
    case 0x10003060:
    case 0x10003070:
    case 0x10003080:
    case 0x10003090:
    case 0x100030A0:
        return gif->read(addr);
    case 0x1000E000 ... 0x1000E060:
        return dmac->read_global(addr);
    case 0x1000F520:
        return dmac->read_enabler(addr);
    }
    printf("[BUS]: Read from unknown addr 0x%08X\n", addr);
    exit(1);
}

uint64_t Bus::Read64(uint32_t addr, bool ee)
{
    addr = addr & KUSEG_MASKS[addr >> 29];
    if (ee)
    {
        if (addr >= 0x1FC00000 && addr < 0x20000000)
            return *(uint64_t*)&bios[addr - 0x1FC00000];
        if (addr >= 0x70000000 && addr <= 0x700003fff)
            return *(uint64_t*)&eeScratchpad[addr - 0x70000000];
        if (addr < 0x2000000)
            return *(uint64_t*)&eeRam[addr];
    }
    printf("[BUS]: Read64 from unknown addr 0x%08X\n", addr);
    exit(1);
}

Register Bus::Read128(uint32_t addr)
{
    addr = addr & KUSEG_MASKS[addr >> 29];
    if (addr < 0x2000000)
    {
        return *(Register*)&eeRam[addr];
    }
    printf("[BUS]: Write128 to unknown addr 0x%08X\n", addr);
    exit(1);
}

void Bus::Write8(uint32_t addr, uint8_t data, bool ee)
{
    addr = addr & KUSEG_MASKS[addr >> 29];

    if (addr == 0x1000F180)
    {
        console << data;
        console.flush();
        return;
    }
    if (ee)
    {
        if (addr >= 0x1FC00000 && addr < 0x20000000)
        {
           bios[addr - 0x1FC00000] = data;
           return;
        }
        if (addr >= 0x70000000 && addr <= 0x700003fff)
        {
            eeScratchpad[addr - 0x70000000] = data;
            return;
        }
        if (addr < 0x2000000)
        {
            eeRam[addr] = data;
            return;
        }
    }

    printf("[BUS]: Write8 to unknown addr 0x%08X\n", addr);
    exit(1);
}

void Bus::Write16(uint32_t addr, uint16_t data, bool ee)
{
    addr = addr & KUSEG_MASKS[addr >> 29];

    if (addr == 0x1F801472 || addr == 0x1F801470)
        return;
    if (ee)
    {
        if (addr >= 0x70000000 && addr <= 0x700003fff)
        {
            *(uint16_t*)&eeScratchpad[addr - 0x70000000] = data;
            return;
        }
        if (addr >= 0x1A000000 && addr <= 0x1FC00000)
            return;
    }
    printf("[BUS]: Write16 to unknown addr 0x%08X\n", addr);
    exit(1);
}

void Bus::Write32(uint32_t addr, uint32_t data, bool ee)
{
    addr = addr & KUSEG_MASKS[addr >> 29];

    if (ee)
    {
        if (addr < 0x2000000 && ee)
        {
            *(uint32_t*)&eeRam[addr] = data;
            return;
        }
        else if (addr == 0x1000F500 || addr == 0x1000F510)
            return;
        else if (addr >= 0x70000000 && addr <= 0x700003fff)
        {
            *(uint32_t*)&eeScratchpad[addr - 0x70000000] = data;
            return;
        }
        else if (addr == 0x1000F140 || addr == 0x1000F150 || addr == 0x1000F100 || addr == 0x1000F120)
            return;
        else if (addr == 0x1000F130 || addr == 0x1000F400 || addr == 0x1000F420 || addr == 0x1000F450)
            return;
        else if (addr == 0x1000F410 || addr == 0x1000F480 || addr == 0x1000F490 || addr == 0x1000F460 || addr == 0x1000F410)
            return;
        else if (addr == 0x1000F430)
        {
            uint8_t SA = (data >> 16) & 0xFFF;
            uint8_t SBC = (data >> 6) & 0xF;

            if (SA == 0x21 && SBC == 0x1 && ((MCH_DRD >> 7) & 1) == 0)
                rdram_sdevid = 0;
            
            MCH_RICM = data & ~0x80000000;
            return;
        }
        else if (addr == 0x1000F440)
        {
            MCH_DRD = data;
            return;
        }
        else if (addr >= 0x1000F200 && addr <= 0x1000F260)
        {
            sif->write(addr, data);
            return;
        }
        else if (addr >= 0x10000000 && addr < 0x10002000)
        {
            timers->write(addr, data);
            return;
        }
        else if (addr == 0x1000F010 || addr == 0x1000F000)
        {
            intc->write(addr, data);
            return;
        }
        else if (addr == 0x1F80141C)
            return;
        else if (addr >= 0x1C000000 && addr <= 0x1C200000)
        {
            *(uint32_t*)&iopRam[addr & 0x1FFFFF] = data;
            return;
        }
        else if (addr >= 0x10008000 && addr < 0x1000E000)
        {
            dmac->write_channel(addr, data);
            return;
        }
        switch (addr)
        {
        case 0x10003000:
        case 0x10003010:
        case 0x10003020:
        case 0x10003030:
        case 0x10003040:
        case 0x10003050:
        case 0x10003060:
        case 0x10003070:
        case 0x10003080:
        case 0x10003090:
        case 0x100030A0:
            gif->write(addr, data);
            return;
        case 0x1000E000 ... 0x1000E060:
            dmac->write_global(addr, data);
            return;
        case 0x1000F590:
            dmac->write_enabler(addr, data);
            return;
        }
    }
    printf("[BUS]: Write to unknown addr 0x%08X\n", addr);
    exit(1);
}

void Bus::Write64(uint32_t addr, uint64_t data, bool ee)
{
    addr = addr & KUSEG_MASKS[addr >> 29];
    if (addr < 0x2000000 && ee)
    {
        *(uint64_t*)&eeRam[addr] = data;
        return;
    }
    if (addr >= 0x70000000 && addr < 0x70004000 && ee)
    {
        *(uint64_t*)&eeScratchpad[addr - 0x70000000] = data;
        return;
    }
    if (addr >= 0x12000000 && addr <= 0x12001080)
    {
        gs->write_priv(addr, data);
        return;
    }
    if ((addr == 0x1000F000 || addr == 0x1000F010) && ee)
    {
        intc->write(addr, data);
        return;
    }
    if (addr >= 0x11000000 && addr < 0x11010000)
    {
        bool vid = addr & 0x8000;
        bool memory = addr & 0x4000;

        if (memory)
            vu[vid]->write<Memory::Data, uint64_t>(addr, data);
        else
            vu[vid]->write<Memory::Code, uint64_t>(addr, data);
        return;
    }
    printf("[BUS]: Write64 to unknown addr 0x%08X\n", addr);
    exit(1);
}

void Bus::Write128(uint32_t addr, Register data)
{
    addr = addr & KUSEG_MASKS[addr >> 29];
    if (addr < 0x2000000)
    {
        *(Register*)&eeRam[addr] = data;
        return;
    }
    if (addr == 0x10006000)
    {
        uint128_t val = *(uint128_t*)data.ud;
        gif->write_path3(addr, val);
        return;
    }
    if (addr >= 0x11000000 && addr < 0x11010000)
    {
        bool vid = addr & 0x8000;
        bool memory = addr & 0x4000;
        uint128_t val = *(uint128_t*)data.ud;

        if (memory)
            vu[vid]->write<Memory::Data, uint128_t>(addr, val);
        else
            vu[vid]->write<Memory::Code, uint128_t>(addr, val);
        return;
    }
    printf("[BUS]: Write128 to unknown addr 0x%08X\n", addr);
    exit(1);
}