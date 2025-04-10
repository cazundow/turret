<h1> Nerf Turret </h1> 
<p>A face tracking Nerf-firing turret powered by an ESP32, because why not?</p>

<h2> Overview </h2>
<p>this project botches a few technologies together to produce a face tracking, nerf firing turret for the purpose of learning more about micro electronics, control systems and power electronics.</p>

<p> 
    Primary control is handled through an exposed webpage on the esp-32 with a websocket connection to allow relatively responsive, full duplex 2 way communication. this should allow move commands and telemetry to be sent and received simultaneously.
</p>

<p>
    Secondary control can be handed off via a mode change to enable full automatic tracking and target acquisition. 
    this is handled by a python script and depends on there being a webcam on the machine. the script uses opencv's cv2 library to handle video capture and face detection and once again websocket is used to communication move commands to the esp-32 controller  
</p>


<h2>Features </h2>
- manual control
- sweep mode
- automatic target finder.
- single, purse and full auto firing modes

<h2>Hardware Requirements</h2>
- [3d prints](https://www.printables.com/model/338574-nerf-dart-turret-brushless-wifi-wip)
- ESP32 microcontroller.
- 2x 2206/2300 KV Brushless Motors.
- 2x 20-30a speed controllers.
- Power supply (e.g., battery pack).
- RC servo
- 2x Serial Bus servos
- Jumper wires and a breadboard (optional for prototyping).

<h2> PCB </h2> 
<p>
    after the prototyping on a breadboard, I created a more permanent power distribution board using perfboard. this requires a lil soldering but shouldn't be too intimidating 
</p>

<p>
    But since part of this project was to allow me to build my first pcb, i created one with easyEDA by jlcpcb. Thje files for which are include in this project. 
</p>

<h2>Software Requirements </h2>
- Arduino IDE or PlatformIO.
- ESP32 board support installed in the IDE.
- Required libraries (e.g., Servo library, Wi-Fi/Bluetooth libraries).
- python3 && opencv 

2. Install the software:
   - Clone this repository to your local machine.
   - Open the project in your preferred IDE.
   - Install any required libraries.

3. Configure the code:
   - Update the Wi-Fi credentials in main.cpp.
   - Adjust GPIO pin assignments as per your hardware setup.

4. Upload the code:
   - Connect the ESP32 to your computer via USB.
   - Compile and upload the code to the ESP32.

## Usage
- Power on the turret and control board.
- connect to the address printed to the serial monitor .
- update the ip address in facetracking.py with the provided ip and run the file for automatic target finding

## Disclaimer
This project is for educational and recreational purposes only. Please use responsibly and ensure safety when operating the turret. a direct shot to the eye will cause damage!!!

## License
[MIT License](LICENSE)



