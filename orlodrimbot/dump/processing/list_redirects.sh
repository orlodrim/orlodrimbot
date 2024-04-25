#!/bin/bash
DATA_DIR="$1"
grep '^[^|]*|[^|]*|R' "${DATA_DIR}/titles.dat" | cut -d '|' -f 1,4 > "${DATA_DIR}/redirections.dat"
