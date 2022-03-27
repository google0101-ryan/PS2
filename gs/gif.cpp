#include <gs/gif.hpp>
#include <gs/gs.hpp>
#include <cassert>


static const char* REGS[10] =
{
    "GIF_CTRL",
    "GIF_MODE",
    "GIF_STAT",
    "GIF_TAG0",
    "GIF_TAG1",
    "GIF_TAG2",
    "GIF_TAG3",
    "GIF_CNT",
    "GIF_P3CNT",
    "GIF_P3TAG"
};

using namespace gs;

GIF::GIF(GraphicsSynthesizer* _gpu)
: gpu(_gpu) {}

uint32_t GIF::read(uint32_t addr)
{
    uint32_t data;
    auto offset = (addr & 0xF0) >> 4;
    switch (offset)
    {
    case 2:
        status.fifo_count = fifo.size<uint128_t>();
        data = status.value;
        break;
    default:
        printf("[GIF]: Read from unknown register %s\n", REGS[offset]);
        exit(1);
    }
    return data;
}

void GIF::write(uint32_t addr, uint32_t data)
{
    auto offset = (addr & 0xF0) >> 4;
    switch (offset)
    {
    case 0:
        control.value = data;

        if (control.reset)
            reset();
        break;
    default:
        printf("[GIF]: Write to unknown reg %s\n", REGS[offset]);
        exit(1);
    }
}

void GIF::tick(uint32_t cycles)
{
    while (!fifo.empty() && cycles--)
    {
        if (!data_count)
            process_tag();
        else
            execute_command();
    }
}

void GIF::reset()
{
    control = {};
    mode = 0;
    status = {};
    fifo = {};
    tag = {};
    data_count = reg_count = 0;
    internal_Q = 0;
}

bool GIF::write_path3(uint32_t, uint128_t qword)
{
    return fifo.push<uint128_t>(qword);
}

void GIF::process_tag()
{
    if (fifo.read(&tag.value))
    {
        data_count = tag.nloop;
        reg_count = tag.nreg;

        gpu->prim = tag.pre ? tag.prim : gpu->prim;

        gpu->rgbaq.q = 1.0f;

        fifo.pop<uint128_t>();
    }
}

void GIF::execute_command()
{
    uint128_t qword;
    if (fifo.read(&qword))
    {
        uint16_t format = tag.flg;
        switch (format)
        {
        case Format::Packed:
        {
            process_packed(qword);
            if (!reg_count)
            {
                data_count--;
                reg_count = tag.nreg;
            }
            break;
        }
        case Format::Image:
        {
            gpu->write_hwreg(qword);
            gpu->write_hwreg(qword >> 64);
            data_count--;
            break;
        }
        default:
            printf("[GIF]: Unknown format %d\n", format);
            exit(1);
        }
        fifo.pop<uint128_t>();
    }
}

void GIF::process_packed(uint128_t qword)
{
    int curr_reg = tag.nreg - reg_count;
    uint64_t regs = tag.regs;
    uint32_t desc = (regs >> 4 * curr_reg) & 0xF;

    switch (desc)
    {
    case REGDesc::PRIM:
    {
        gpu->write(0x0, qword & 0x7ff);
        break;
    }
    case REGDesc::RGBAQ:
    {
        RGBAQReg target;
        target.r = qword & 0xFF;
        target.g = (qword >> 32) & 0xFF;
        target.b = (qword >> 64) & 0xFF;
        target.a = (qword >> 96) & 0xFF;

        gpu->write(0x1, target.value);
        break;
    }
    case REGDesc::ST:
    {
        gpu->write(0x2, qword);
        internal_Q = qword >> 64;
        break;
    }
    case REGDesc::XYZF2:
    {
        bool disable_draw = (qword >> 111) & 1;
        auto address = disable_draw ? 0xC : 0x4;

        XYZF target;

		target.x = qword & 0xffff;
		target.y = (qword >> 32) & 0xffff;
		target.z = (qword >> 68) & 0xffffff;
		target.f = (qword >> 100) & 0xff;

        gpu->write(address, target.value);
        break;
    }
    case REGDesc::XYZ2:
    {
        bool disable_draw = (qword >> 111) & 1;
        auto address = disable_draw ? 0xC : 0x4;

        XYZF target;

		target.x = qword & 0xffff;
		target.y = (qword >> 32) & 0xffff;
		target.z = (qword >> 64) & 0xffffffff;

        gpu->write(address, target.value);
        break;
    }
    case REGDesc::A_D:
    {
        uint64_t data = qword;
        uint16_t addr = (qword >> 64) & 0xFF;
        gpu->write(addr, data);
        break;
    }
    default:
        printf("[GIF]: Unknown reg 0x%X\n", desc);
        exit(1);
    }
    reg_count--;
}