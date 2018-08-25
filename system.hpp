#include <eosiolib/eosio.hpp>

using std::vector;

namespace eosiosystem {
    using eosio::permission_level;

    struct public_key {
        unsigned_int type;
        std::array<char, 33> data;

        friend bool operator==(const public_key &a, const public_key &b) {
            return std::tie(a.type, a.data) == std::tie(b.type, b.data);
        }
        friend bool operator!=(const public_key &a, const public_key &b) {
            return std::tie(a.type, a.data) != std::tie(b.type, b.data);
        }
        EOSLIB_SERIALIZE(public_key, (type)(data))
    };

    struct permission_level_weight {
        permission_level permission;
        weight_type weight;

        EOSLIB_SERIALIZE(permission_level_weight, (permission)(weight))
    };

    struct key_weight {
        public_key key;
        weight_type weight;

        EOSLIB_SERIALIZE(key_weight, (key)(weight))
    };

    struct wait_weight {
        uint32_t wait_sec;
        weight_type weight;
    };

    struct authority {
        uint32_t threshold;
        vector<key_weight> keys;
        vector<permission_level_weight> accounts;
        vector<wait_weight> waits;

        authority() = default;
        authority(uint32_t threshold, vector<permission_level_weight> accounts) {
            this->threshold = threshold;
            this->accounts = accounts;
        }

        EOSLIB_SERIALIZE(authority, (threshold)(keys)(accounts)(waits))
    };
}
