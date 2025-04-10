import cv2
from cvzone.FaceDetectionModule import FaceDetector
import pyfirmata
import numpy as np
from websockets.sync.client import connect
import json
import threading
import time

HORIZONTAL_FOV = 110  # degrees (example value, replace with actual)
VERTICAL_FOV = 40    # degrees (example value, replace with actual)

# Servo limits
PAN_SERVO_MIN = 0     # Min pan servo angle
PAN_SERVO_MAX = 1000   # Max pan servo angle
TILT_SERVO_MIN = 0    # Min tilt servo angle
TILT_SERVO_MAX = 1000  # Max tilt servo angle

previousX = 0
previousY = 0

cap = cv2.VideoCapture(0)
ws, hs = 1000, 1000
cap.set(3, ws)
cap.set(4, hs)

if not cap.isOpened():
    print("Camera couldn't Access!!!")
    exit()

# port = "COM7"
# board = pyfirmata.Arduino(port)
# servo_pinX = board.get_pin('d:9:s') #pin 9 Arduino
# servo_pinY = board.get_pin('d:10:s') #pin 10 Arduino

def sendData(websocket, data):
    print(f"Sending: {data}")
    json_data = json.dumps(data)  # Convert dictionary to JSON string
    try:
        websocket.send(json_data)  # Send JSON data
        # message = websocket.recv()  # Wait for a response
        # print(f"Received: {message}")
    except Exception as e:
        print(f"WebSocket Error: {e}")

def keepalive(websocket):
    while True:
        try:
            websocket.ping()
            time.sleep(60)  # Send a ping every 30 seconds
        except Exception as e:
            print(f"Keepalive Error: {e}")
            break

detector = FaceDetector()
servoPos = [500, 500] # initial servo position

# Establish WebSocket connection once
try:
    websocket = connect("ws://192.168.1.55:81/")
    threading.Thread(target=keepalive, args=(websocket,), daemon=True).start()
except Exception as e:
    print(f"WebSocket Connection Error: {e}")
    exit()

while True:
    success, img = cap.read()
    img = cv2.flip(img, 1)  # Flip the image around the y-axis
    img, bboxs = detector.findFaces(img, draw=False)

    if bboxs:
        #get the coordinate
        fx, fy = bboxs[0]["center"][0], bboxs[0]["center"][1]
        pos = [fx, fy]
        #convert coordinat to servo degree
        servoX = np.interp(fx, [0, ws], [PAN_SERVO_MIN, PAN_SERVO_MAX])
        servoY = np.interp(fy, [0, hs], [TILT_SERVO_MAX, TILT_SERVO_MIN])  # Invert the Y-axis mapping

        if servoX < PAN_SERVO_MIN:
            servoX = PAN_SERVO_MIN
        elif servoX > PAN_SERVO_MAX:
            servoX = PAN_SERVO_MAX
        if servoY < TILT_SERVO_MIN:
            servoY = TILT_SERVO_MIN
        elif servoY > TILT_SERVO_MAX:
            servoY = TILT_SERVO_MAX

        servoPos[0] = servoX
        servoPos[1] = servoY

        cv2.circle(img, (fx, fy), 80, (0, 0, 255), 2)
        cv2.putText(img, str(pos), (fx+15, fy-15), cv2.FONT_HERSHEY_PLAIN, 2, (255, 0, 0), 2 )
        cv2.line(img, (0, fy), (ws, fy), (0, 0, 0), 2)  # x line
        cv2.line(img, (fx, hs), (fx, 0), (0, 0, 0), 2)  # y line
        cv2.circle(img, (fx, fy), 15, (0, 0, 255), cv2.FILLED)
        cv2.putText(img, "TARGET LOCKED", (50, 200), cv2.FONT_HERSHEY_PLAIN, 3, (255, 0, 255), 3 )

    else:
        cv2.putText(img, "NO TARGET", (880, 50), cv2.FONT_HERSHEY_PLAIN, 3, (0, 0, 255), 3)
        cv2.circle(img, (640, 360), 80, (0, 0, 255), 2)
        cv2.circle(img, (640, 360), 15, (0, 0, 255), cv2.FILLED)
        cv2.line(img, (0, 360), (ws, 360), (0, 0, 0), 2)  # x line
        cv2.line(img, (640, hs), (640, 0), (0, 0, 0), 2)  # y line

    cv2.putText(img, f'Servo X: {int(servoPos[0])} deg', (50, 50), cv2.FONT_HERSHEY_PLAIN, 2, (255, 0, 0), 2)
    cv2.putText(img, f'Servo Y: {int(servoPos[1])} deg', (50, 100), cv2.FONT_HERSHEY_PLAIN, 2, (255, 0, 0), 2)

    # servo_pinX.write(servoPos[0])
    # servo_pinY.write(servoPos[1])
    data = {"camServoPan": servoPos[0], "camServoTilt": servoPos[1]}
    #check if position has changed by more than 5 degrees
    if abs(previousX - servoPos[0]) > 5 or abs(previousY - servoPos[1]) > 5:
        # Send data to the server
        data['fire'] = 1; 
    
    sendData(websocket, data)
    print(servoPos)
    previousX = servoPos[0]
    previousY = servoPos[1]
    cv2.imshow("Image", img)
    cv2.waitKey(1)

# Close the WebSocket connection when done
websocket.close()