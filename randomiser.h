
#ifndef ROBALETHEGAME_RANDOMISER_H
#define ROBALETHEGAME_RANDOMISER_H
#include <cstdint>

class Randomiser
{
    private:
    uint32_t next_value;
    // Constants for deterministic quasi-randomisation.
    const uint64_t MOD = 4294967291;
    const uint64_t MULT = 279410273;

    public:
    Randomiser() = delete;

    Randomiser(uint32_t);

    uint32_t rand();
};

#endif //ROBALETHEGAME_RANDOMISER_H
