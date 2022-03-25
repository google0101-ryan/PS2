#pragma once

#include <cstdint>

union uint128_t
{
    struct
    {
        uint64_t lo;
        uint64_t hi;
    } i;

    uint64_t ud[2];
    uint32_t uw[2];

    uint128_t inline operator |(uint128_t val)
    {
        uint128_t data;
        data.i.lo = i.lo | val.i.lo;
        data.i.hi = i.hi | val.i.hi;

        return data;
    }

    uint128_t& operator=(const int value)
    {
        uw[0] = value;
        return *this;
    }
};