
#ifndef APP_SPAWNER_DETERMINISTIC_RNG_H
#define APP_SPAWNER_DETERMINISTIC_RNG_H 1

#include <cstdint>

namespace SE {

// ---------------------------------------------------------------------------
// DeterministicRng — splitmix64: 8 bytes of state, no allocation, exact
// reproducibility from a seed (world seed ^ hash64(population id) ^ seed_offset).
// Same operation sequence ⇒ same output sequence, run to run — required for
// tests, replays and deterministic population. All spawner randomness flows
// through this; never seed from clocks, pointers or addresses.
// ---------------------------------------------------------------------------

class DeterministicRng {
public:
        explicit DeterministicRng(uint64_t uSeed) : state(uSeed) { ;; }

        uint64_t NextU64() {
                state += 0x9E3779B97F4A7C15ull;
                uint64_t z = state;
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
                z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
                return z ^ (z >> 31);
        }

        /// [0, 1)
        float Uniform01() {
                // 24 random mantissa bits — float-exact, no double round-trip drift
                return static_cast<float>(NextU64() >> 40) * (1.0f / 16777216.0f);
        }

        /// [a, b)
        float Range(float a, float b) {
                return a + (b - a) * Uniform01();
        }

        /// FNV-1a 64: stable cross-run string hash for seeding (std::hash is
        /// implementation-defined and could differ across stdlib builds).
        static uint64_t Hash64(const std::string & s) {
                uint64_t h = 0xCBF29CE484222325ull;
                for (const char c : s) {
                        h ^= static_cast<uint64_t>(static_cast<uint8_t>(c));
                        h *= 0x100000001B3ull;
                }
                return h;
        }

private:
        uint64_t state;
};

} // namespace SE

#endif
