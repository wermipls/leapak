#!/bin/bash

set -e

gcc src/pak.c -O2 -g -o pak
gcc src/unpak.c -O2 -g -o unpak
