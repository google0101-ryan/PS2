#pragma once

#include <int128.h>
#include <queue>

class Bus;

union IPUCommand
{
    uint32_t value;
    struct
    {
        uint32_t option : 28;
        uint32_t code : 4;
    };
};

union IPUControl
{
    uint64_t value;
    struct
	{
        uint64_t input_fifo_size : 4;
        uint64_t output_fifo_size : 4;
        uint64_t coded_block_pattern : 6;
        uint64_t error_code_detected : 1;
        uint64_t start_code_detected : 1;
        uint64_t intra_dc_precision : 2;
        uint64_t : 2;
        uint64_t scan_pattern_bdec : 1;
        uint64_t intra_vlc_format : 1;
        uint64_t quantize_step_bdec : 1;
        uint64_t mpeg1 : 1;
        uint64_t picture_type_vdec : 3;
        uint64_t : 3;
        uint64_t reset_ipu : 1;
        uint64_t busy : 1;
        uint64_t : 32;
	};
};

struct IPURegs
{
    IPUControl control;
};

class IPU
{
public:
    IPU(Bus* parent);
    ~IPU() = default;

    uint64_t read(uint32_t addr);
    void write(uint32_t addr, uint64_t data);

    uint128_t read_fifo(uint32_t addr);
    void write_fifo(uint32_t addr, uint128_t data);

    void decode_command(IPUCommand cmd);
    uint64_t get_command_result();

private:
    Bus* bus;
    IPURegs regs = {};
    std::queue<uint32_t> fifo;
};