import cv2
import numpy as np

def is_red_cube_present(frame, min_area=800):
    """
    Analyzes an OpenCV frame to detect the presence of a red cube.
    
    Args:
        frame: The image array from the camera.
        min_area: The minimum pixel area to be considered a valid cube (filters noise).
        
    Returns:
        bool: True if a red cube is detected, False otherwise.
    """
    # Convert RGB to HSV for reliable color detection
    hsv = cv2.cvtColor(frame, cv2.COLOR_RGB2HSV)

    # Define the two HSV ranges for Red (since red wraps around the HSV spectrum)
    lower_red_1 = np.array([0, 120, 70])
    upper_red_1 = np.array([10, 255, 255])
    lower_red_2 = np.array([170, 120, 70])
    upper_red_2 = np.array([180, 255, 255])

    # Create masks and combine them
    mask1 = cv2.inRange(hsv, lower_red_1, upper_red_1)
    mask2 = cv2.inRange(hsv, lower_red_2, upper_red_2)
    mask = mask1 + mask2

    # Find shapes (contours) in the mask
    contours, _ = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

    # If no red objects are found at all, return False immediately
    if not contours:
        return False

    # Grab the largest red object to evaluate
    largest_contour = max(contours, key=cv2.contourArea)
    area = cv2.contourArea(largest_contour)

    # Check if the object is large enough
    if area > min_area:
        # Get the bounding box to check the shape
        x, y, w, h = cv2.boundingRect(largest_contour)
        aspect_ratio = float(w) / h
        
        # Check if it is roughly square (a cube)
        if 0.7 <= aspect_ratio <= 1.3: 
            return True

    # If it fails the area or shape checks
    return False