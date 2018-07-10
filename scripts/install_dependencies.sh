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
    wget https://github.com/google/protobuf/releases/download/v3.6.0/protobuf-cpp-3.6.0.tar.gz
    tar -xzf protobuf-cpp-3.6.0.tar.gz
    the_dir=$(pwd)
    bash -c "cd protobuf-3.6.0 && ./autogen.sh && ./configure && make"
    sudo -i bash -c "cd ${the_dir}/protobuf-3.6.0 && make install"
    sudo -i ldconfig
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
