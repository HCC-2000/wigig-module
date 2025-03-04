#/!bin/bash

./waf configure --disable-examples --disable-tests --disable-python --enable-modules='applications','core','internet','point-to-point','wifi','flow-monitor','spectrum' --enable-static -d optimized
    ./waf build

