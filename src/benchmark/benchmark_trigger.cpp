#include <zmq.hpp>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <memory>
#include <unordered_set>
#include "zmq/socket_cache.hpp"
#include "zmq/zmq_util.hpp"
#include "common.hpp"
#include "yaml-cpp/yaml.h"

using namespace std;

unsigned DEFAULT_LOCAL_REPLICATION;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <benchmark_threads>" << endl;
    return 1;
  }

  unsigned thread_num = atoi(argv[1]);
  // TODO(vikram): this is a hack that we should be able to remove once the
  // refactor is done
  DEFAULT_LOCAL_REPLICATION = 1;

  // read in the benchmark addresses
  vector<string> benchmark_address;

  // read the YAML conf
  vector<string> ips;
  YAML::Node conf = YAML::LoadFile("conf/config.yml");
  YAML::Node benchmark = conf["benchmark"];

  for (YAML::const_iterator it = benchmark.begin(); it != benchmark.end(); ++it) {
    ips.push_back(it->as<string>());
  }

  zmq::context_t context(1);
  SocketCache pushers(&context, ZMQ_PUSH);

  string command;
  while (true) {
    cout << "command> ";
    getline(cin, command);

    for (auto it = benchmark_address.begin(); it != benchmark_address.end(); it++) {
      for (unsigned tid = 0; tid < thread_num; tid++) {
        zmq_util::send_string(command, &pushers["tcp://" + *it + ":" + to_string(tid + COMMAND_BASE_PORT)]);
      }
    }
  }
}

