#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <system.hpp>

using namespace eosio;

namespace data {
    using eosiosystem::authority;

    //@abi table accountlocks i64
    struct accountlock {
        account_name account;
        account_name owner;
        authority reclaim_auth;

        uint64_t primary_key() const { return account; }
    };

    typedef eosio::multi_index<N(accountlocks), accountlock> accountlock_table;

    //@abi table offerconfigs i64
    struct offerconfig {
        account_name owner;
        account_name name;
        asset start_price;
        uint32_t bidding_timeout_sec = 24 * 60 * 60;

        uint64_t primary_key() const { return name; }
    };

    typedef eosio::multi_index<N(offerconfigs), offerconfig> offerconfig_table;

    //@abi table auctions i64
    struct auction {
        account_name name;
        asset highest_bid;
        account_name highest_bidder;
        uint32_t bidding_timeout_sec = 24 * 60 * 60;
        uint64_t closes_at = now() + bidding_timeout_sec;

        uint64_t primary_key() const { return name; }
        void reset_close_time() { closes_at = now() + bidding_timeout_sec; }
    };

    typedef eosio::multi_index<N(auctions), auction> auctions_table;
}

using namespace data;
using eosiosystem::authority;
using eosiosystem::permission_level_weight;

class name_market : public contract {
  public:
    using contract::contract;

    void lockacnt(account_name account, account_name owner, const authority &reclaim_auth);
    void unlockacnt(account_name account);
    void offername(account_name owner, account_name name, asset start_price, uint32_t bidding_timeout_sec);
    void stopoffer(account_name owner, account_name name);
    void bidname(account_name bidder, account_name name, asset bid);
    void claimname(account_name claimer, account_name name);
    void earlyclose(account_name owner, account_name name);

  private:
    void start_auction(account_name bidder, asset bid, offerconfig offer);
    void process_bid(account_name bidder, asset bid, auction auc);
};

EOSIO_ABI(name_market, (lockacnt)(unlockacnt)(offername)(stopoffer)(bidname)(claimname)(earlyclose))
