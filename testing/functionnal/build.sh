#!/bin/bash

mkdir -p mod_dup/build
cd mod_dup/build
cmake ..
make

cd -

./env.sh mod_dup/build/src conf_2.0/functionnal/
