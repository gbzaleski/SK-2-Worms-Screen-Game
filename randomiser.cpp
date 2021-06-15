#include "randomiser.h"

Randomiser::Randomiser(uint32_t seed)
{
    next_value = seed;
}

uint32_t Randomiser::rand()
{
    uint32_t res = next_value;
    uint64_t temp = MULT * next_value;
    temp %= MOD;
    next_value = temp;
    return res;
}
