#pragma once

#include <gs/queue.h>
#include <cstdint>
#include <int128.h>

namespace gs
{
    struct GraphicsSynthesizer;
}

union GIFCTRL
{
    uint32_t value;
    struct
    {
        uint32_t reset : 1;
        uint32_t : 2;
        uint32_t stop : 1;
    };
};

union GIFSTAT
{
    uint32_t value;
    struct
    {
        uint32_t path3_mask : 1;
        uint32_t path3_vif_mask : 1;
        uint32_t path3_imt : 1;
        uint32_t stop : 1;
        uint32_t : 1;
        uint32_t path3_interrupted : 1;
        uint32_t path3_queued;
        uint32_t path2_queued;
        uint32_t path1_queued;
        uint32_t output_path : 1;
        uint32_t active_path : 1;
        uint32_t direction : 1;
        uint32_t : 12;
        uint32_t fifo_count : 5;
    };
};

union GIFTag
{
    uint128_t value;
    struct
    {
        uint128_t nloop : 15;
        uint128_t eop : 1;
        uint128_t : 30;
        uint128_t pre : 1;
        uint128_t prim : 11;
        uint128_t flg : 2;
        uint128_t nreg : 4;
        uint128_t regs : 64;
    };
};

enum REGDesc : uint64_t
{
    PRIM = 0,
    RGBAQ = 1,
    ST = 2,
    UV = 3,
    XYZF2 = 4,
    XYZ2 = 5,
    FOG = 10,
    A_D = 14,
    NOP = 15
};


enum Format : uint32_t
{
    Packed = 0,
    Reglist = 1,
    Image = 2,
    Disable = 3
};

class GIF
{
public:
    GIF(gs::GraphicsSynthesizer* _gpu);
    ~GIF() = default;

    void tick(uint32_t cycles);
    void reset();

    uint32_t read(uint32_t addr);
    void write(uint32_t addr, uint32_t data);

    bool write_path3(uint32_t, uint128_t data);
private:
    void process_tag();
    void execute_command();

    void process_packed(uint128_t qword);

private:
    gs::GraphicsSynthesizer* gpu;
    GIFCTRL control = {};
    uint32_t mode = 0;
    GIFSTAT status = {};
    util::Queue<uint32_t, 64> fifo;

    GIFTag tag = {};
    int data_count = 0, reg_count = 0;

    uint32_t internal_Q;
};