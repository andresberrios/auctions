#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <system.hpp>

using namespace eosio;

namespace data {
    using eosiosystem::authority;

    //@abi table accountlocks i64
    struct accountlock_type {
        account_name account;
        account_name owner;
        authority reclaim_auth;
        uint32_t active_auctions = 0;

        uint64_t primary_key() const { return account; }
    };

    typedef eosio::multi_index<N(accountlocks), accountlock_type> accountlocks_table;

    //@abi table offers i64
    struct offer_type {
        account_name owner;
        account_name name;
        asset start_price;
        uint32_t bidding_timeout_sec = 24 * 60 * 60;

        uint64_t primary_key() const { return name; }
    };

    typedef eosio::multi_index<N(offers), offer_type> offers_table;

    //@abi table auctions i64
    struct auction_type {
        account_name name;
        asset highest_bid;
        account_name highest_bidder;
        uint32_t bidding_timeout_sec = 24 * 60 * 60;
        uint64_t closes_at = now() + bidding_timeout_sec;

        uint64_t primary_key() const { return name; }
        void reset_close_time() { closes_at = now() + bidding_timeout_sec; }
    };

    typedef eosio::multi_index<N(auctions), auction_type> auctions_table;
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
    void closeoffer(account_name name);
    void bidname(account_name bidder, account_name name, asset bid);
    void claimname(account_name claimer, account_name name);
    void earlyclose(account_name owner, account_name name);

  private:
    bool is_account_locked(account_name account);
    bool has_account_locked(account_name owner, account_name account);
    void start_auction(account_name bidder, asset bid, offer_type offer);
    void process_bid(account_name bidder, asset bid, auction_type auction);
};

EOSIO_ABI(name_market, (lockacnt)(unlockacnt)(offername)(closeoffer)(bidname)(claimname)(earlyclose))
