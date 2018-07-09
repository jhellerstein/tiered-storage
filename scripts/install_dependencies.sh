#! /usr/bin/env bash

set -euo pipefail

install_misc() {
    # These dependencies are needed by a couple of different projects including
    # redis, protobuf, grpc, etc.
    sudo apt-get install \
      autoconf \
      automake \
      curl \
      libtool \
      make \
      unzip
}

main() {
    set -x
    sudo apt-get -y update
    install_misc
    set +x
}

main
