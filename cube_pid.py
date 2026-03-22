from picamera2 import Picamera2
import cv2
import numpy as np
import serial
import time

def create_uart(port='/dev/serial0', baudrate=115200, timeout=1):
    return serial.Serial(port, baudrate, timeout=timeout)


def create_camera(frame_size=(320, 240)):
    picam2 = Picamera2()
    config = picam2.create_preview_configuration(main={"size": frame_size})
    picam2.configure(config)
    picam2.start()
    return picam2


def run_cube_pid(
    port='/dev/serial0',
    baudrate=115200,
    frame_size=(320, 240),
    min_area=800,
    loop_delay=0.05,
):
    ser = create_uart(port=port, baudrate=baudrate)
    picam2 = create_camera(frame_size=frame_size)

    center_x = frame_size[0] // 2

    print("Vision Sensor Active. Broadcasting PID Error...")

    try:
        while True:
            frame = picam2.capture_array()
            hsv = cv2.cvtColor(frame, cv2.COLOR_RGB2HSV)

            lower_red_1 = np.array([0, 120, 70])
            upper_red_1 = np.array([10, 255, 255])
            lower_red_2 = np.array([170, 120, 70])
            upper_red_2 = np.array([180, 255, 255])

            mask = cv2.inRange(hsv, lower_red_1, upper_red_1) + cv2.inRange(hsv, lower_red_2, upper_red_2)
            contours, _ = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

            if contours:
                largest_contour = max(contours, key=cv2.contourArea)
                area = cv2.contourArea(largest_contour)

                if area > min_area:
                    x, y, w, h = cv2.boundingRect(largest_contour)
                    aspect_ratio = float(w) / h

                    if 0.7 <= aspect_ratio <= 1.3:
                        cube_center_x = x + (w // 2)
                        error = cube_center_x - center_x
                        print(f"Cube found! Error: {error}")
                        ser.write(f"{error}\n".encode('utf-8'))

            time.sleep(loop_delay)

    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        picam2.stop()
        ser.close()


if __name__ == '__main__':
    run_cube_pid()