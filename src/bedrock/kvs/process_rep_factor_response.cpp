#include <fstream>
#include <chrono>
#include "zmq/socket_cache.hpp"
#include "common.hpp"
#include "hash_ring.hpp"
#include "kvs/rc_pair_lattice.hpp"
#include "kvs/kvs_handlers.hpp"

void process_rep_factor_response(string response_string,
    unsigned seed,
    unsigned& total_access,
    std::shared_ptr<spdlog::logger> logger,
    chrono::system_clock::time_point& start_time,
    unordered_map<unsigned, TierData> tier_data_map,
    unordered_map<unsigned, GlobalHashRing>& global_hash_ring_map,
    unordered_map<unsigned, LocalHashRing>& local_hash_ring_map,
    unordered_map<string, pair<chrono::system_clock::time_point, vector<PendingRequest>>>& pending_request_map,
    unordered_map<string, pair<chrono::system_clock::time_point, vector<PendingGossip>>> pending_gossip_map,
    unordered_map<string, multiset<std::chrono::time_point<std::chrono::system_clock>>>& key_access_timestamp,
    unordered_map<string, KeyInfo> placement,
    unordered_map<string, KeyStat>& key_stat_map,
    unordered_set<string>& local_changeset,
    ServerThread& wt,
    Serializer* serializer,
    SocketCache& pushers) {
  communication::Response response;
  response.ParseFromString(response_string);

  vector<string> tokens;
  split(response.tuple(0).key(), '_', tokens);
  string key = tokens[1];

  if (response.tuple(0).err_number() == 2) {
    auto respond_address = wt.get_replication_factor_connect_addr();
    issue_replication_factor_request(respond_address, key, global_hash_ring_map[1], local_hash_ring_map[1], pushers, seed);
    return;
  } else if (response.tuple(0).err_number() == 0) {
    communication::Replication_Factor rep_data;
    rep_data.ParseFromString(response.tuple(0).value());

    for (int i = 0; i < rep_data.global_size(); i++) {
      placement[key].global_replication_map_[rep_data.global(i).tier_id()] = rep_data.global(i).global_replication();
    }

    for (int i = 0; i < rep_data.local_size(); i++) {
      placement[key].local_replication_map_[rep_data.local(i).tier_id()] = rep_data.local(i).local_replication();
    }
  } else {
    for (unsigned i = MIN_TIER; i <= MAX_TIER; i++) {
      placement[key].global_replication_map_[i] = tier_data_map[i].default_replication_;
      placement[key].local_replication_map_[i] = DEFAULT_LOCAL_REPLICATION;
    }
  }

  vector<unsigned> tier_ids = { SELF_TIER_ID };
  bool succeed;

  if (pending_request_map.find(key) != pending_request_map.end()) {
    auto threads = get_responsible_threads(wt.get_replication_factor_connect_addr(), key, is_metadata(key), global_hash_ring_map, local_hash_ring_map, placement, pushers, tier_ids, succeed, seed);

    if (succeed) {
      bool responsible = threads.find(wt) != threads.end();
      vector<PendingRequest> request_vec = pending_request_map[key].second;

      for (auto it = request_vec.begin(); it != request_vec.end(); ++it) {
        auto now = chrono::system_clock::now();

        if (!responsible && it->addr_ != "") {
          communication::Response response;

          if (it->respond_id_ != "") {
            response.set_response_id(it->respond_id_);
          }

          communication::Response_Tuple* tp = response.add_tuple();
          tp->set_key(key);
          tp->set_err_number(2);

          for (auto iter = threads.begin(); iter != threads.end(); iter++) {
            tp->add_addresses(iter->get_request_pulling_connect_addr());
          }

          string serialized_response;
          response.SerializeToString(&serialized_response);
          zmq_util::send_string(serialized_response, &pushers[it->addr_]);
        } else if (responsible && it->addr_ == "") { // only put requests should fall into this category
          if (it->type_ == "P") {
            auto time_diff = chrono::duration_cast<chrono::milliseconds>(now - start_time).count();
            auto ts = generate_timestamp(time_diff, wt.get_tid());

            process_put(key, ts, it->value_, serializer, key_stat_map);
            key_access_timestamp[key].insert(now);

            total_access += 1;
            local_changeset.insert(key);
          } else {
            logger->error("Received a GET request with no response address.");
          }
        } else if (responsible && it->addr_ != "") {
          communication::Response response;

          if (it->respond_id_ != "") {
            response.set_response_id(it->respond_id_);
          }

          communication::Response_Tuple* tp = response.add_tuple();
          tp->set_key(key);

          if (it->type_ == "G") {
            auto res = process_get(key, serializer);
            tp->set_value(res.first.reveal().value);
            tp->set_err_number(res.second);

            key_access_timestamp[key].insert(std::chrono::system_clock::now());
            total_access += 1;
          } else {
            auto time_diff = chrono::duration_cast<chrono::milliseconds>(now - start_time).count();
            auto ts = generate_timestamp(time_diff, wt.get_tid());

            process_put(key, ts, it->value_, serializer, key_stat_map);
            tp->set_err_number(0);

            key_access_timestamp[key].insert(now);
            total_access += 1;
            local_changeset.insert(key);
          }

          string serialized_response;
          response.SerializeToString(&serialized_response);
          zmq_util::send_string(serialized_response, &pushers[it->addr_]);
        }
      }
    } else {
      logger->error("Missing key replication factor in process pending request routine.");
    }

    pending_request_map.erase(key);
  }

  if (pending_gossip_map.find(key) != pending_gossip_map.end()) {
    auto threads = get_responsible_threads(wt.get_replication_factor_connect_addr(), key, is_metadata(key), global_hash_ring_map, local_hash_ring_map, placement, pushers, tier_ids, succeed, seed);

    if (succeed) {
      if (threads.find(wt) != threads.end()) {
        for (auto it = pending_gossip_map[key].second.begin(); it != pending_gossip_map[key].second.end(); ++it) {
          process_put(key, it->ts_, it->value_, serializer, key_stat_map);
        }
      } else {
        unordered_map<string, communication::Request> gossip_map;

        // forward the gossip
        for (auto it = threads.begin(); it != threads.end(); it++) {
          gossip_map[it->get_gossip_connect_addr()].set_type("PUT");

          for (auto iter = pending_gossip_map[key].second.begin(); iter != pending_gossip_map[key].second.end(); iter++) {
            prepare_put_tuple(gossip_map[it->get_gossip_connect_addr()], key, iter->value_, iter->ts_);
          }
        }

        // redirect gossip
        for (auto it = gossip_map.begin(); it != gossip_map.end(); it++) {
          push_request(it->second, pushers[it->first]);
        }
      }
    } else {
      logger->error("Missing key replication factor in process pending gossip routine.");
    }

    pending_gossip_map.erase(key);
  }
}