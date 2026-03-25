"""
Main robot control code for the SLRC competition.

This script sends high-level UART commands to Arduino Mega.
Protocol (line based): COMMAND [arg1] [arg2] ...\n
Examples:
    STOP\n
    TURN LEFT\n
    MOVE 120 120\n
    MODE WALL_FOLLOW\n
"""

from __future__ import annotations

import argparse
import time
from typing import Iterable

import serial
import cube_scan, cube_pid
from picamera2 import Picamera2
import cv2

from threading import Thread

class MegaUART:
    def __init__(self, port: str = "/dev/serial0", baudrate: int = 115200, timeout: float = 1.0) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser: serial.Serial | None = None

    def connect(self) -> None:
        self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
        time.sleep(0.2)
        print(f"UART connected: {self.port} @ {self.baudrate}")

    def close(self) -> None:
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("UART closed")

    def send(self, command: str, *args: object) -> None:
        if not self.ser or not self.ser.is_open:
            raise RuntimeError("UART is not connected")

        payload = " ".join([command.strip().upper(), *[str(arg) for arg in args]]) + "\n"
        self.ser.write(payload.encode("utf-8"))
        self.ser.flush()
        print(f"TX -> {payload.strip()}")

    def send_many(self, commands: Iterable[tuple[object, ...]], delay_s: float = 0.05) -> None:
        for item in commands:
            if not item:
                continue
            command = str(item[0])
            args = item[1:]
            self.send(command, *args)
            time.sleep(delay_s)

    def stop(self) -> None:
        self.send("STOP")

    def set_mode(self, mode_name: str) -> None:
        self.send("MODE", mode_name)

    def turn_left(self, speed: int = 110, duration_ms: int = 450) -> None:
        self.send("TURN", "LEFT", speed, duration_ms)

    def turn_right(self, speed: int = 110, duration_ms: int = 450) -> None:
        self.send("TURN", "RIGHT", speed, duration_ms)

    def move(self, left_speed: int, right_speed: int, duration_ms: int = 0) -> None:
        self.send("MOVE", left_speed, right_speed, duration_ms)

    def arm_pick(self) -> None:
        self.send("ARM", "PICK")

    def arm_drop(self) -> None:
        self.send("ARM", "DROP")

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Send UART commands to Arduino Mega")
    parser.add_argument("--port", default="/dev/serial0", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate")
    parser.add_argument("--demo", action="store_true", help="Run predefined demo sequence")
    parser.add_argument(
        "--cmd",
        nargs="+",
        help='Single command to send, e.g. --cmd TURN LEFT 120 450 or --cmd STOP',
    )
    return parser.parse_args()

def task_schedule(uart: MegaUART) -> None:
    
    # 1. Start in WALL_FOLLOW mode, follow the wall for 2 seconds
    uart.set_mode("WALL_FOLLOW")
    time.sleep(2.0)

    # 2. Switch to WALL_WITH_LINE mode
    uart.set_mode("WALL_WITH_LINE")

    # wait untill the robot sends "Line found"
    while True:
        # waits for a line from the Arduino
        line = uart.ser.readline().decode('utf-8').strip()
        if line == "Line found":
            uart.stop()
            break

    # 3. Turn left, then switch to LINE_WITH_CUBE_SEARCH_RIGHT mode
    uart.turn_left()
    time.sleep(1.0)
    uart.set_mode("LINE_WITH_CUBE_SEARCH_RIGHT")    # turn camera to right

    # --- START OF FIX ---
    # Initialize the camera using the native Pi library
    scan_cam = Picamera2()
    config = scan_cam.create_preview_configuration(main={"size": (320, 240)})
    scan_cam.configure(config)
    scan_cam.start()

    # call the cube detection function until it returns True
    while True:
        # Capture a frame directly as a numpy array
        frame = scan_cam.capture_array()  
        result = cube_scan.is_red_cube_present(frame)  
        
        if result:
            uart.stop()  # Stop the robot when the cube is found
            break
            
    # CRITICAL: Free the camera hardware so cube_pid can use it next!
    scan_cam.stop() 
    scan_cam.close()
    # --- END OF FIX ---
    
    # 4. Turn right, switch to CUBE_ALIGN mode, and move forward for 1 second
    uart.turn_right()
    time.sleep(1.0)
    uart.set_mode("CUBE_ALIGN")     # Rotate cam to front

    # run Cube_pid in a separate thread to continuously send PID error values to the Arduino while moving, so that the main thread can do communication with the Arduino
    thread = Thread(target=cube_pid.run_cube_pid, args=(uart.ser,))
    thread.start()

    # when the cube is nearby, stop the motors and pick up the cube
    # wait for the message "Cube nearby" from the Arduino, then stop and pick up
    while True:
        line = uart.ser.readline().decode('utf-8').strip()  # Blocking read, waits for a line from the Arduino
        if line == "Cube nearby":       # detected by front Sharp IR
            break

    uart.set_mode("Stop")
    uart.arm_pick()

    # wait untill the message "ready" is received from the Arduino, then go backward
    while True:
        line = uart.ser.readline().decode('utf-8').strip()  # Blocking read, waits for a line from the Arduino
        if line == "ready":
            break
    
    uart.set_mode("MOVE BACKWARD")  # until a line is found, detected by bottom Raykha IR sensors, then stop

    # move backward until a line is found, then stop
    while True:
        line = uart.ser.readline().decode('utf-8').strip()  # Blocking read, waits for a line from the Arduino
        if line == "Line found":
            uart.stop()
            break

    # Turn left, switch to "Line follow until wall"
    uart.turn_left()
    time.sleep(1.0)
    uart.set_mode("LINE_FOLLOW_UNTIL_WALL")

    # Move forward until the message "Wall found" is received from the Arduino, then stop
    while True:
        line = uart.ser.readline().decode('utf-8').strip()  # Blocking read, waits for a line from the Arduino
        if line == "Wall found":
            uart.stop()
            break

    uart.turn_left()
    time.sleep(1.0)

    # Change to wall with line detection
    uart.set_mode("WALL_WITH_LINE")

    # Move forward until the message "Line found" is received from the Arduino, then stop
    while True:
        line = uart.ser.readline().decode('utf-8').strip()  # Blocking read, waits for a line from the Arduino
        if line == "Line found":
            uart.stop()
            break

    # Start line follow mode , untill a junction is found, then stop
    uart.set_mode("LINE_FOLLOW")

    while True:
        line = uart.ser.readline().decode('utf-8').strip()  # Blocking read, waits for a line from the Arduino
        if line == "Junction found":
            uart.stop()
            break

    # move left
    uart.turn_left()
    time.sleep(1.0)

    # line follow again, until the message "Line found" is received from the Arduino, then stop
    uart.set_mode("LINE_FOLLOW")

    while True:
        line = uart.ser.readline().decode('utf-8').strip()  # Blocking read, waits for a junction from the Arduino
        if line == "Junction found":
            uart.stop()
            break

    # stop and cube drop
    uart.set_mode("Stop")

    # insert the cube into the box
    uart.arm_drop()


if __name__ == "__main__":
    args = parse_args()
    uart = MegaUART(port=args.port, baudrate=args.baud)

    try:
        uart.connect()
        task_schedule(uart)
    finally:
        uart.close()