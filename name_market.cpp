#include "name_market.hpp"

void name_market::lockacnt(account_name account, account_name owner, const authority &reclaim_auth) {
    require_auth2(account, N(owner));

    // Save reclaim authority and account owner
    accountlocks_table locks(_self, account);
    locks.emplace(account, [&](accountlock_type &lock) {
        lock.account = account;
        lock.owner = owner;
        lock.reclaim_auth = reclaim_auth;
    });

    // Set up new authority
    const auto new_auth = authority(1, {permission_level_weight{.permission = {_self, N(eosio.code)}, .weight = 1}});

    // Set the owner permission
    action(permission_level{account, N(owner)},
           N(eosio), N(updateauth),
           std::make_tuple(account, N(owner), static_cast<uint64_t>(0), new_auth))
        .send();
}

void name_market::unlockacnt(account_name account) {
    // Get lock from DB
    accountlocks_table locks(_self, account);
    auto &lock = locks.get(account, "Unable to find account in lock database");

    require_auth(lock.owner);

    eosio_assert(lock.active_auctions == 0, "Cannot unlock an account which still has active auctions");

    // Set the owner permission
    action(permission_level{account, N(owner)},
           N(eosio), N(updateauth),
           std::make_tuple(account, N(owner), 0, lock.reclaim_auth))
        .send();

    // Erase the lock record
    locks.erase(lock);
}

void name_market::offername(account_name owner, account_name name,
                            asset start_price, uint32_t bidding_timeout_sec) {
    require_auth(owner);

    eosio_assert(start_price.symbol == asset().symbol, "Starting price should be in the system token");
    eosio_assert(start_price.amount >= 0, "Starting price should not be negative");
    eosio_assert(bidding_timeout_sec <= 7 * 24 * 60 * 60, "Auction timeout should not be greater than a week");

    if (is_account(name)) {
        eosio_assert(this->has_account_locked(owner, name),
                     "The account with this name is not locked under the account of the creator of the offer");
    } else {
        account_name suffix = eosio::name_suffix(name);
        eosio_assert(suffix != name && suffix == owner, "This name cannot be offered by this account");
        eosio_assert(this->is_account_locked(owner),
                     "Owner account must be locked in order to offer its subnames");
    }

    offers_table offers(_self, name);
    auto offer_itr = offers.find(name);

    if (offer_itr == offers.end()) {
        offers.emplace(owner, [&](offer_type &offer) {
            offer.owner = owner;
            offer.name = name;
            offer.start_price = start_price;
            offer.bidding_timeout_sec = bidding_timeout_sec;
        });
    } else {
        offers.modify(offer_itr, 0, [&](offer_type &offer) {
            offer.start_price = start_price;
            offer.bidding_timeout_sec = bidding_timeout_sec;
        });
    }
}

bool name_market::is_account_locked(account_name account) {
    accountlocks_table locks(_self, account);
    auto lock_itr = locks.find(account);
    return lock_itr != locks.end();
}

bool name_market::has_account_locked(account_name owner, account_name account) {
    accountlocks_table locks(_self, account);
    auto lock_itr = locks.find(account);
    return lock_itr != locks.end() && lock_itr->owner == owner;
}

void name_market::closeoffer(account_name name) {
    offers_table offers(_self, name);
    auto &offer = offers.get(name, "No offer found for the provided name");

    require_auth(offer.owner);

    offers.erase(offer);
}

void name_market::bidname(account_name bidder, account_name name, asset bid) {
    require_auth(bidder);

    // Basic checks
    eosio_assert(name != 0, "The empty name is not a valid account name to bid on");
    eosio_assert((name & 0xFull) == 0, "13 character names are not valid account names to bid on");
    eosio_assert(bid.symbol == asset().symbol, "Bid should be in the system token");
    eosio_assert(bid.amount >= 0, "Bid cannot be negative");

    // Do the bid transfer (won't really run if this action fails after this point)
    action(permission_level{bidder, N(active)},
           N(eosio.token), N(transfer),
           std::make_tuple(bidder, _self, bid, std::string("Bid on name ") + (eosio::name{name}).to_string()))
        .send();

    // First check if there is an open auction for that name
    auctions_table auctions(_self, name);
    auto auction_itr = auctions.begin();
    if (auction_itr != auctions.end()) {
        // Auction exists
        this->process_bid(bidder, bid, *auction_itr);
    } else {
        // No open auction, check if there is an offer for this exact name
        offerconfig_table offers(_self, name);
        // TODO DO IT
        auto offer_itr = offers.find(name);
        if (offer_itr != offers.end()) {
            offerconfig offer = *offer_itr;
            eosio_assert(bid.amount >= offer.start_price.amount,
                         "Bid should be equal to or greater than the starting price of the auction");

            // Create new auction for this name
            auctions.emplace(bidder, [&](auction &row) {
                row.name = name;
                row.highest_bid = bid;
                row.highest_bidder = bidder;
                row.bidding_timeout_sec = offer.bidding_timeout_sec;
                row.reset_close_time();
            });
            // Remove offer from DB
            offers.erase(offer_itr);
        } else {
            // TODO We might accept bids anyway, to promote the platform to name providers
            // We'll call this an "eager auction"
            // We shouldn't directly let people start eager auctions on subnames that are
            // being generally offered but that particular one already exists
            eosio_assert(false, "This name is not currently for sale");
        }
    }
}

void name_market::process_bid(account_name bidder, asset bid, auction auc) {
    eosio_assert(auc.closes_at > now(), "Auction for this name is already closed");
    eosio_assert(bid.amount - auc.highest_bid.amount >= (auc.highest_bid.amount / 10),
                 "Must increase bid by at least 10%");
    eosio_assert(auc.highest_bidder != bidder, "Account is already the highest bidder");

    // Refund previous bid
    action(permission_level{_self, N(active)},
           N(eosio.token), N(transfer),
           std::make_tuple(_self, auc.highest_bidder, auc.highest_bid,
                           std::string("Refund bid on name ") + (eosio::name{auc.name}).to_string()))
        .send();
    // Accept new bid
    auctions_table auctions(_self, auc.name);
    auctions.modify(auc, bidder, [&](auction &row) {
        row.highest_bid = bid;
        row.highest_bidder = bidder;
        row.reset_close_time();
    });
}

void name_market::claimname(account_name claimer, account_name name) {
    require_auth(claimer);

    // TODO Check that claimer won the auction for the name

    const auto auth = authority(1, {permission_level_weight{.permission = {claimer, N(active)}, .weight = 1}});

    // Register the new account
    action(permission_level{owner, N(owner)},
           N(eosio), N(newaccount),
           std::make_tuple(owner, name, auth, auth))
        .send();

    // Buy RAM
    action(permission_level{claimer, N(active)},
           N(eosio), N(buyrambytes),
           std::make_tuple(claimer, name, 4 * 1024))
        .send();

    // Delegate and transfer bandwidth
    action(permission_level{claimer, N(active)},
           N(eosio), N(delegatebw),
           std::make_tuple(claimer, name, asset(1000), asset(1000), 1))
        .send();
}

void name_market::earlyclose(account_name owner, account_name name) {
    require_auth(owner);
}
