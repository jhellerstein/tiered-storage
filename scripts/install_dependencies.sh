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

install_protobuf() {
  # check to see if protobuf folder is empty
  if [ ! -d "$HOME/protobuf/lib" ]; then
    wget https://github.com/google/protobuf/archive/v3.6.0.tar.gz
    tar -xzvf v3.6.0.tar.gz
    cd protobuf-3.6.0 && ./configure --prefix=$HOME/protobuf && make && make install
  else
    echo "Using cached directory."
  fi
}

main() {
    set -x
    sudo apt-get -y update
    install_misc
    install_protobuf
    set +x
}

main
