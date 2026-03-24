#!/bin/bash

echo "========================================"
echo " Setting up Endurance Robot Environment"
echo "========================================"

# Directory where the script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$DIR"

# 1. Install system dependencies (OpenCV requirements and picamera2)
echo "1. Installing system dependencies..."
sudo apt-get update
sudo apt-get install -y python3-venv python3-picamera2 libgl1-mesa-glx libavcodec58 libgtk2.0-dev

# 2. Create the virtual environment (if it doesn't exist)
VENV_DIR="$DIR/venv"
if [ ! -d "$VENV_DIR" ]; then
    echo "2. Creating virtual environment in $VENV_DIR..."
    python3 -m venv --system-site-packages "$VENV_DIR"
else
    echo "2. Virtual environment already exists."
fi

# 3. Activate the virtual environment
echo "3. Activating virtual environment..."
source "$VENV_DIR/bin/activate"

# 4. Install Python requirements
echo "4. Installing Python requirements..."
pip install --upgrade pip
pip install -r requirements.txt

echo "========================================"
echo " Setup Complete!"
echo " To activate manually: source venv/bin/activate"
echo " To run the robot: source venv/bin/activate && python3 robot.py"
echo "========================================"
