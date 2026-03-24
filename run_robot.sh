#!/bin/bash

# Boot script meant to be run by systemd
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$DIR"

# Source the virtual environment
source "$DIR/venv/bin/activate"

# Run the python script
exec python3 robot.py "$@"
