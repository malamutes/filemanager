#!/bin/bash
cd "$(dirname "$0")"

cd build && cmake .. && cmake --build . && clear && ./main; exit

exec ./main