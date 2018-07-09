#include <fstream>
#include "zmq/socket_cache.hpp"
#include "common.hpp"
#include "hash_ring.hpp"
#include "kvs/rc_pair_lattice.hpp"
#include "kvs/kvs_handlers.hpp"

void send_gossip(AddressKeysetMap& addr_keyset_map, SocketCache& pushers, Serializer* serializer) {
  unordered_map<string, communication::Request> gossip_map;

  for (auto map_it = addr_keyset_map.begin(); map_it != addr_keyset_map.end(); map_it++) {
    gossip_map[map_it->first].set_type("PUT");

    for (auto set_it = map_it->second.begin(); set_it != map_it->second.end(); set_it++) {
      auto res = process_get(*set_it, serializer);

      if (res.second == 0) {
        prepare_put_tuple(gossip_map[map_it->first], *set_it, res.first.reveal().value, res.first.reveal().timestamp);
      }
    }
  }

  // send gossip
  for (auto it = gossip_map.begin(); it != gossip_map.end(); it++) {
    push_request(it->second, pushers[it->first]);
  }
}

pair<ReadCommittedPairLattice<string>, unsigned> process_get(const string& key, Serializer* serializer) {
  unsigned err_number = 0;
  auto res = serializer->get(key, err_number);

  // check if the value is an empty string
  if (res.reveal().value == "") {
    err_number = 1;
  }
  return pair<ReadCommittedPairLattice<string>, unsigned>(res, err_number);
}

void process_put(const string& key,
    const unsigned long long& timestamp,
    const string& value,
    Serializer* serializer,
    unordered_map<string, KeyStat>& key_stat_map) {

  if (serializer->put(key, value, timestamp)) {
    // update value size if the value is replaced
    key_stat_map[key].size_ = value.size();
  }
}