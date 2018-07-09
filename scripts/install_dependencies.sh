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
    git clone https://github.com/google/protobuf.git
    cd protobuf/
    ./autogen.sh
    ./configure
    make
    make check
    sudo make install
    sudo ldconfig # refresh shared library cache.
    protoc -h
}

main() {
    set -x
    sudo apt-get -y update
    install_misc
    install_protobuf
    set +x
}

main
