#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <freertos/task.h>

#define RX_PIN 16
#define TX_PIN 17
#define PAN_SERVO_ID 1
#define TILT_SERVO_ID 2
#define BAUD_RATE 115200
#define ONBOARDLED 2
#define leftEscPin 23
#define rightEscPin 22
#define fireServoPin 21
#define cameraServoPanPin 18
#define cameraServoTiltPin 19
// WiFi credentials
const char *ssid PROGMEM = "Jedi Council";
const char *password PROGMEM = "JTISPCCO";
const char *controllerAddress PROGMEM= "58:10:31:76:B0:CC";

// Web server and websocket
WebServer server(80);
WebSocketsServer webSocket(81);

// Servo objects
Servo panServo;
Servo tiltServo;
Servo leftFlywheel;
Servo rightFlywheel;
Servo fireServo;
Servo cameraServoPan;
Servo cameraServoTilt;

// Shared variables
volatile uint16_t currentPan = 500;
volatile uint16_t currentTilt = 400;
volatile boolean fireMode = 0;
volatile uint16_t motorSpeed = 1000;
volatile uint8_t flywheelSpeed = 60;
volatile bool fireCommand = false;
volatile uint8_t flywheelStatus = 0; // 0 -initialising, 1 - ready, 2 - busy - 3 - error
volatile uint8_t motorStatus = 0; // 0 - ready, 1 - busy - 2 - error
volatile uint8_t cameraPan = 90;
volatile uint8_t cameraTilt = 90;
volatile uint8_t mode = 2; // 0 - manual, 1 - sweap, 2 - auto
volatile uint8_t cameraStatus = 0; // 0 - ready, 1 - busy - 2 - error

// Constants
uint8_t BULLET_COUNT = 7;
uint8_t MAX_LOADER_VAL = 50; 
uint8_t MIN_LOADER_VAL = 0;

// UART2 for serial communication
HardwareSerial SerialUART(2);

// Function prototypes
void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void setupWiFi();
void setupWebServer();
void moveServo(uint8_t id, uint16_t position, uint16_t speed);
void setupFlywheel();
void enterLowPowerMode();
void setupCamera();
int getServoPosition(uint8_t id);

// Tasks
void webServerTask(void *pvParameters);
void servoControlTask(void *pvParameters);
void flywheelControlTask(void *pvParameters);
void uartCommunicationTask(void *pvParameters);
void telemetryTask(void *pvParameters);
void cameraControlTask(void *pvParameters);
void setup()
{
    Serial.begin(115200);
    pinMode(ONBOARDLED, OUTPUT);

    // Initialize UART2
    SerialUART.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    // Initialize WiFi and web server
    setupWiFi();
    setupWebServer();
    setupFlywheel();
    Serial.println("bluetooth control Ready.");

    // Initialize the camera servos in setup()
    setupCamera();
    // Create FreeRTOS tasks
    xTaskCreate(webServerTask, "WebServerTask", 4096, NULL, 1, NULL);
    xTaskCreate(servoControlTask, "ServoControlTask", 2048, NULL, 1, NULL);
    xTaskCreate(flywheelControlTask, "FlywheelControlTask", 2048, NULL, 1, NULL);
    xTaskCreate(uartCommunicationTask, "UARTCommunicationTask", 2048, NULL, 1, NULL);
    xTaskCreate(telemetryTask, "TelemetryTask", 2048, NULL, 1, NULL);
    xTaskCreate(cameraControlTask, "CameraControlTask", 2048, NULL, 1, NULL);
}

void loop()
{
    // The main loop is intentionally left empty as tasks handle the logic.
    // Enter low-power mode if no tasks require processing
    // enterLowPowerMode();
}

void initFireServo(){
  fireServo.attach(fireServoPin, 500, 2500);
  fireServo.write(0);
  delay(1000);
  fireServo.detach();
}

void fireSemi(){
  fireServo.write(80);
  delay(300);
  fireServo.write(0);
  BULLET_COUNT--;
  delay(400);
}
void fireBurst(){
  for(int i = 0; i < 3; i++){
    fireServo.write(50);
    delay(500);
    fireServo.write(0);
    BULLET_COUNT--;
    delay(500);
  }
}
void fireAuto(){
  for(int i = 0; i < 7; i++){
    fireServo.write(50);
    delay(500);
    fireServo.write(0);
    BULLET_COUNT--;
    delay(500);
  }
}
void fire(){
  fireServo.attach(fireServoPin, 500, 2500);
  flywheelStatus = 2;
  leftFlywheel.write(flywheelSpeed);
  rightFlywheel.write(flywheelSpeed);
  delay(1000);
    switch(fireMode){
    case 0:
      fireSemi();
      break;
    case 1:
      fireBurst();
      break;
    case 2:
      fireAuto();
      break;
  }
   leftFlywheel.write(0);
  rightFlywheel.write(0);
  fireCommand = false;
  flywheelStatus = 1;
  fireServo.detach();

}
void setupWiFi()
{
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("Connected!");
    Serial.println("IP Address: " + WiFi.localIP().toString());
}
void setupFlywheel()
{
    flywheelStatus = 0;
    leftFlywheel.attach(leftEscPin, 1000, 2500);
    rightFlywheel.attach(rightEscPin, 1000, 2500);
    
    Serial.println("Initialising flywheels...");
    leftFlywheel.write(5); // initialise the esc signal to low level
    rightFlywheel.write(5); // initialise the esc signal to low level
    delay(2000); // wait for 2 second
    leftFlywheel.write(60); // then set it to high level
    rightFlywheel.write(60); // then set it to high level
    delay(2000); // then wait again for 2 second
    leftFlywheel.write(0); // then set it back again to low level
    rightFlywheel.write(0); // then set it back again to low level
    delay(1000);// then wait again for 1 second
    flywheelStatus = 1;
    Serial.println("Flywheels ready");
}
void setupCamera() {
  cameraServoPan.attach(cameraServoPanPin, 500, 2500);
  cameraServoTilt.attach(cameraServoTiltPin, 500, 2500);
  cameraServoPan.setPeriodHertz(50);
  cameraServoTilt.setPeriodHertz(50);
  cameraServoPan.write(90);
  cameraServoTilt.write(90);
  delay(500);
  Serial.println("Camera ready");
}
void setupWebServer()
{
    server.on("/", []() {
      String html PROGMEM = "<!DOCTYPE html><html><head> <title>Auto Sentry</title> <style> html { font-family: Arial, sans-serif; font-size: 16px; color: #333; width: 100%; height: 100%; } body { height: 100vh; width: 100%; margin: 0; background-color: #2d2d2d; } .container { padding: 20px; display: flex; flex-direction: column; align-items: center; width: 100%; height: 100%; } .turret-container { width: 200px; height: 200px; position: relative; } .turret { width: 100%; height: 100%; animation: rotate 5s linear infinite backwards; transition: all 0.5s ease; animation-direction: alternate; } .turret_manual { width: 100%; height: 100%; transition: all 0.s linear; } @keyframes rotate { from { transform: rotate(180deg); } to { transform: rotate(360deg); } } .header { width: 100%; height: 100px; background-color: #f1f1f1; border-radius: 5px; margin-top: 10px; position: relative; padding: 0 20px; } .wrapper { display: flex; justify-content: space-evenly; width: 100%; padding: 60px; height: 100%; } .half { width: 50%; height: 100%; padding: 0px 15px } .first { display: flex; flex-direction: column; align-items: center; } .tracker { width: 100%; min-height: 400px; background-color: #f1f1f1; border: 1px solid #5a5353; border-radius: 5px; position: relative; height: 50%; overflow: hidden; margin-bottom: 20px; } .tracker:hover { cursor: grab; } #setPosition { width: 100%; } .sliderWrap { display: flex; justify-content: space-evenly; width: 100%; margin-top: 10px; } .slider { transition: all 0.5s; } .modeSelector { margin-bottom: 50px; } .toggles { display: flex; justify-content: space-evenly; width: 100%; margin-top: 50px; } .hidden { display: none; } .line { position: absolute; background-color: #000; } .horizontal { top: 50%; left: 0; transform: translateY(-50%); width: 100%; height: 1px; } .vertical { left: 50%; top: 0; transform: translateX(-50%); width: 1px; height: 100%; } .button { border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: block; font-size: 16px; cursor: pointer; width: 80%; height: 40%; margin: 0 auto; font-size: 2rem; } .controls { display: grid; grid-template-columns: auto auto auto; grid-template-rows: auto auto; grid-gap: 10px; background-color: #A07178; width: 100%; min-height: 300px; height: 50%; } .control-btn { width: 100%; height: 100%; display: flex; justify-content: center; align-items: center; } .green { background-color: #7AE7C7; } .orange { background-color: #FFA500; } .red { background-color: #FF0000; } .purple { background-color: #7D387D; } .blue { background-color: #2BD9FE; } .yellow { background-color: #E1D89F; } .pink { background-color: #FF0F80; } .black-text { color: black; } .telemetry { width: 100%; margin-bottom: 50px; } .telemetry .content { display: flex; } .telemetry .title { font-size: 2em; font-weight: bold; padding: 35px 20px; background-color: lightgray; color: black; } .dataItem { display: flex; font-size: 2rem; width: 50%; } .dataItem .label { padding: 35px 10px; text-align: center; color: white; } .dataItem .value { padding: 35px 10px; text-align: left; } .control-tabs { display: flex; width: 100%; background-color: gray; color: white; } .nav-btn { border: none; text-align: center; text-decoration: none; display: block; font-size: 16px; cursor: pointer; padding: 35px 20px; } .fireBtn { border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: block; font-size: 16px; cursor: pointer; width: auto; height: 40%; margin: 0 auto; font-size: 2rem; background-color: #FF0000; } .fire_selector.selected { border: 3px solid #7AE7C7; } .speed-selector.selected { background-color: #7AE7C7; } /* media queries for mobile */ @media only screen and (max-width: 768px) { .half { width: 100%; } .wrapper { flex-direction: column; } .dataItem, .telemetry .title { font-size: 1rem; } .button { font-size: 1rem; } } .tracker-indicator { display: flex; justify-content: end; align-items: center; width: 100%; } .indicator { width: 20px; height: 20px; border-radius: 50%; background-color: red; } .tracker-indicator p { margin-right: 20px; } </style></head><body style='background-color: #EEEEEE;'> <div class='header'> <div class='turret-container'> <svg class='turret' xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'> <!-- Base of the turret --> <circle cx='50' cy='50' r='20' fill='#ff9c07' /> <!-- Gun of the turret --> <rect x='20' y='45' width='20' height='5' fill='#ff9c07' /> </svg> </div> </div> <div class='container'> <div class='wrapper'> <div class='half first'> <div class='tracker-indicator'> <p>Double click to activate/deactivate tracker</p> <div class='indicator'></div> </div> <div class='tracker'> <div class='line horizontal'></div> <div class='line vertical'></div> </div> <div class='controls'> <div class='control-btn'> <button class='button purple fire_selector selected' id='semi'> Semi </button> </div> <div class='control-btn'> <button class='button orange fire_selector' id='burst'> Burst </button> </div> <div class='control-btn'> <button class='button red fire_selector' id='auto'> Auto </button> </div> <div class='control-btn black-text'> <button class='button black-text speed-selector selected' id='>'> > </button> </div> <div class='control-btn '> <button class='button black-text speed-selector' id='>>'> >> </button> </div> <div class='control-btn'> <button class='button black-text speed-selector' id='>>>'> >>> </button> </div> </div> </div> <div class='half second'> <div class='control-tabs'> <button id='home' class='nav-btn'> Home </button> <button id='settings' class='nav-btn'> Settings </button> </div> <div class='operation'> <div class='telemetry'> <div class='title'> Position </div> <div class='content'> <div class='dataItem'> <div class='label pink' id='positionXAxisLabel'> X axis: </div> <div class='value' id='positionXAxisValue'> N/A </div> </div> <div class='dataItem'> <div class='label pink' id='positionYAxisLabel'> Y axis: </div> <div class='value' id='positionYAxisValue'> N/A </div> </div> <div class='dataItem'> <div class='label pink' id='motorSpeedLabel'> Speed: </div> <div class='value' id='motorSpeedValue'> N/A </div> </div> </div> </div> <div class='telemetry'> <div class='title'> Magazines </div> <div class='content'> <div class='dataItem'> <div class='label blue' id='flywheelSpeedLabel'> FW </div> <div class='value' id='flywheelSpeedValue'> N/A </div> </div> <div class='dataItem'> <div class='label blue' id='magCountLabel'> Mag </div> <div class='value' id='magCountValue'> N/A </div> </div> <div class='dataItem'> <div class='label blue'> Bat </div> <div class='value'> N/A% </div> </div> </div> </div> <div class='telemetry'> <div class='title'> Status </div> <div class='content'> <div class='dataItem'> <div class='label yellow' id='motorStatusLabel'> Motors </div> <div class='value' id='motorStatusValue'> connecting... </div> </div> <div class='dataItem'> <div class='label yellow' id='flywheelStatustLabel'> Flywheel </div> <div class='value' id='flywheelStatusValue'> connecting... </div> </div> <div class='dataItem'> <div class='label yellow'> reset </div> <div class='value'> <button class='button black-text' id='reset'> Reset </button> </div> </div> </div> </div> <button class='button fireBtn'>Fire</button> </div> <div class='telemetry-wrap hidden'> <div class='telemetry'> <div class='title'> Flywheel Speed 0 - 180 RPM </div> <div class='content'> <div class='dataItem'> <div class='label pink'> Speed: </div> <div class='value'> N/A </div> </div> <div class='dataItem'> <div class='label pink'> Speed: </div> <div class='value'> N/A </div> </div> <div class='dataItem'> <div class='label pink'> Speed: </div> <div class='value'> N/A </div> </div> </div> </div> <div class='telemetry'> <div class='title'> Auto Speed (m/s) & Burst Number </div> <div class='content'> <div class='dataItem'> <div class='label purple'> Speed: </div> <div class='value'> 0 </div> </div> <div class='dataItem'> <div class='label purple'> Burst: </div> <div class='value'> 3 </div> </div> </div> </div> <div class='telemetry'> <div class='title'> Axis Servo Speed </div> <div class='content'> <div class='dataItem'> <div class='label blue'> X axis: </div> <div class='value'> 1000 </div> </div> <div class='dataItem'> <div class='label blue'> Y axis: </div> <div class='value'> 800 </div> </div> </div> </div> <div class='telemetry'> <div class='title'> Rotation Limit </div> <div class='content'> <div class='dataItem'> <div class='label yellow'> X axis: </div> <div class='value'> 800 </div> </div> <div class='dataItem'> <div class='label yellow'> Y axis: </div> <div class='value'> 75 </div> </div> </div> </div> </div> </div> </div></body><script> var Socket; var x = 500; var y = 500; var xPercent; var yPercent; let message = { mode: 0, x: x, y: y, motor_speed: 500, fire_mode: 0, flywheel_speed: 60, fire: 0, reset: 0, }; function getFlywheelStatusFromValue(value) { switch (value) { case 0: return 'initialising'; case 1: return 'ready'; case 2: return 'busy'; case 3: return 'error'; } }; function getMotorStatusFromValue(value) { switch (value) { case 0: return 'ready'; case 1: return 'busy'; case 2: return 'error'; } }; function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function (event) { processCommand(event); }; Socket.addEventListener('open', function (event) { console.log('Connected to ESP32'); document.getElementById('motorStatusValue').innerText = 'ready'; document.getElementById('flywheelStatusValue').innerText = getFlywheelStatusFromValue(0); }); Socket.addEventListener('error', function (event) { console.log('Error connecting to ESP32'); console.log(event); ocument.getElementById('flywheelStatusValue').innerText = getFlywheelStatusFromValue(3); }); }; function processCommand(event) { var obj = JSON.parse(event.data); document.getElementById('positionXAxisValue').innerText = obj.pan; document.getElementById('positionYAxisValue').innerText = obj.tilt; document.getElementById('flywheelSpeedValue').innerText = obj.flywheel_speed; document.getElementById('flywheelStatusValue').innerText = getFlywheelStatusFromValue(obj.flywheelStatus); document.getElementById('motorSpeedValue').innerText = obj.motor_speed; document.getElementById('magCountValue').innerText = obj.magCountValue + '/7'; document.getElementById('motorStatusValue').innerText = getMotorStatusFromValue(obj.motorStatus); x = obj.pan; y = obj.tilt; }; window.onload = function (event) { init(); }; function resetMessage() { const prevX = x; const prevY = y; prevFWSpeed = message.flywheel_speed; message = {}; message.mode = 0; message.x = prevX; message.y = prevY; message.flywheel_speed = prevFWSpeed; }; function sendData() { console.log('Sending data to ESP32'); console.log({ message }); const data = { mode: message.mode, pan: message.x, tilt: message.y, motor_speed: message.motor_speed, fire_mode: message.fire_mode, flywheel_speed: message.flywheel_speed, fire: message.fire, reset: message.reset, }; if (Socket && Socket.readyState === 1) { Socket.send(JSON.stringify(data)); }; resetMessage(); }; document.getElementById('reset').addEventListener('click', function (event) { message.reset = 1; sendData(); }); let eventCount = 0; var tracker = document.querySelector('.tracker'); var trackerActive = false; tracker.addEventListener('mousemove', function (event) { if (!trackerActive) { return; } var rawX = event.clientX - tracker.getBoundingClientRect().left; var rawY = event.clientY - tracker.getBoundingClientRect().top; xPercent = Math.round(rawX / tracker.offsetWidth * 100); yPercent = Math.round(rawY / tracker.offsetHeight * 100); x = 1000 - (xPercent / 100 * 1000); y = 1000 - (yPercent / 100 * 1000); console.log(x, y); message.x = x; message.y = y; if (eventCount % 50 === 0 && Socket.readyState === 1) { sendData(); document.getElementById('positionXAxisValue').innerText = x; document.getElementById('positionYAxisValue').innerText = y; } eventCount++; }); tracker.addEventListener('dblclick', function (event) { trackerActive = !trackerActive; if (trackerActive) { document.querySelector('.indicator').style.backgroundColor = 'green'; } else { document.querySelector('.indicator').style.backgroundColor = 'red'; } }); tracker.addEventListener('click', function (event) { if (!trackerActive) { return; } message.fire = 1; sendData(); }); var navBtn = document.querySelectorAll('.nav-btn'); navBtn.forEach(function (btn) { btn.addEventListener('click', function (event) { var id = event.target.id; if (id === 'home') { document.querySelector('.telemetry-wrap').classList.add('hidden'); document.querySelector('.operation').classList.remove('hidden'); } else if (id === 'settings') { document.querySelector('.operation').classList.add('hidden'); document.querySelector('.telemetry-wrap').classList.remove('hidden'); } }); }); var speed_selectors = document.querySelectorAll('.speed-selector'); speed_selectors.forEach(function (selector) { selector.addEventListener('click', function (event) { var id = event.target.id; if (id === '>') { message.flywheel_speed = 60; } else if (id === '>>') { message.flywheel_speed = 90; } else if (id === '>>>') { message.flywheel_speed = 180; } sendData(); document.querySelector('.speed-selector.selected').classList.remove('selected'); event.target.classList.add('selected'); }); }); var fire_selectors = document.querySelectorAll('.fire_selector'); fire_selectors.forEach(function (selector) { selector.addEventListener('click', function (event) { var id = event.target.id; if (id === 'semi') { message.fire_mode = 0; } else if (id === 'burst') { message.fire_mode = 1; } else if (id === 'auto') { message.fire_mode = 2; } sendData(); document.querySelector('.fire_selector.selected').classList.remove('selected'); event.target.classList.add('selected'); }); }); document.querySelector('.fireBtn').addEventListener('click', function (event) { message.fire = 1; sendData(); }); function setLaser(event) { message.laser = Number(event.target.value); sendData(); }; function setSpeed(event) { message.speed = Number(event.target.value); sendData(); }; function setMode(event) { message.mode = Number(event.target.value); sendData(); }; function reverseNumber(num) { sign = num < 0 ? '-' : ''; num = Math.abs(num) + ''; return Number(sign + num.split('').reverse().join('')); }; function map_range(value, low1, high1, low2, high2) { return low2 + (high2 - low2) * (value - low1) / (high1 - low1); };</script></html>";
        server.send(200, "text/html", html);
    });

    server.begin();

    webSocket.begin();
    webSocket.onEvent(handleWebSocketEvent);
}

void webServerTask(void *pvParameters)
{
    for (;;)
    {
        server.handleClient();
        webSocket.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void servoControlTask(void *pvParameters)
{
    for (;;)
    {
        // Move servos to the desired positions
        moveServo(PAN_SERVO_ID, currentPan, motorSpeed);
        moveServo(TILT_SERVO_ID, currentTilt, motorSpeed);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void flywheelControlTask(void *pvParameters)
{
    initFireServo();


    for (;;)
    {
        if (fireCommand)
        {
            flywheelStatus = 2;
            fire();
            fireCommand = false;
            flywheelStatus = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void uartCommunicationTask(void *pvParameters)
{
    for (;;)
    {
        if (SerialUART.available())
        {
          String data = SerialUART.readStringUntil('\n');
          Serial.println("UART Received: " + data);

          // Print raw data in hexadecimal format
          Serial.print("Raw Data: ");
          for (size_t i = 0; i < data.length(); i++)
          {
              Serial.printf("%02X ", data[i]);
          }
          Serial.println(getServoPosition(1));

          ;
          getServoPosition(2);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void telemetryTask(void *pvParameters)
{
    for (;;)
    {
        StaticJsonDocument<200> doc;
        doc["pan"] = currentPan;
        doc["tilt"] = currentTilt;
        doc["motor_speed"] = motorSpeed;
        doc["flywheel_speed"] = flywheelSpeed;
        doc["flywheelStatus"] = flywheelStatus;
        doc["fire_mode"] = fireMode;
        doc["magCountValue"] = BULLET_COUNT;
        doc["motorStatus"] = motorStatus;
        doc["camraStatus"] = cameraStatus;
        doc["mode"] = mode;
        doc["cameraPan"] = cameraPan;
        doc["cameraTilt"] = cameraTilt;

        String jsonString;
        serializeJson(doc, jsonString);

        webSocket.broadcastTXT(jsonString);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Send telemetry every 100ms
    }
}

void cameraControlTask(void *pvParameters)
{
      

    for (;;)
    {
      Serial.printf("CameraControlTask - Pan: %d, Tilt: %d\n", cameraPan, cameraTilt);

        if(mode == -1){
            cameraServoPan.write(90);
            cameraServoTilt.write(90);
        }
        else if (mode == 0 || mode == 2){
            cameraServoPan.write(cameraPan);
            cameraServoTilt.write(cameraTilt);

        }
        else if (mode == 1){
          Serial.print("Sweap mode: ");
          Serial.print(mode);
            //sweap 
            for (int i = 15; i < 165; i++){
                cameraServoPan.write(i);
                cameraServoTilt.write(i);
                vTaskDelay(pdMS_TO_TICKS(30));

            }
            for (int i = 165; i > 15; i--){
                cameraServoPan.write(i);
                cameraServoTilt.write(i);
                vTaskDelay(pdMS_TO_TICKS(30));

            }
        }
        else {
            cameraServoPan.write(cameraPan);
            cameraServoTilt.write(cameraTilt);
        }
       
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}



void enterLowPowerMode()
{
    // Set timer to wake up periodically
    esp_sleep_enable_timer_wakeup(5000000); // 5 seconds
    esp_light_sleep_start();
}

void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{

    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.printf("Client %u disconnected.\n", num);
        break;
    case WStype_CONNECTED:
        Serial.printf("Client %u connected.\n", num);
        break;
    case WStype_TEXT:
    {
      // Serial.printf("Received Raw Data: %s\n", payload); // Debug output

        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error)
        {
            Serial.println("Failed to parse JSON");
            Serial.println(error.c_str());
            return;
        }

        
        motorSpeed = doc["motor_speed"] | motorSpeed;
        flywheelSpeed = doc["flywheel_speed"] | flywheelSpeed;
        fireMode = doc["fire_mode"] | fireMode;
        fireCommand = doc["fire"] | fireCommand;
        mode = doc["mode"] | mode;
        int camServoX = doc["camServoPan"].as<int>();;
        int camServoY = doc["camServoTilt"].as<int>();;
        if(mode == 0){
          currentPan = doc["pan"] | currentPan;
          currentTilt = doc["tilt"] | currentTilt;
         
        }
        else if(mode == 2){
          currentPan = camServoX; 
          currentTilt = camServoY;
        }
        else{
          cameraPan = 90;
          cameraTilt = 90;
        }
        cameraPan = camServoX; 
        cameraTilt = camServoY;
        break;
    }
    break;
    default:
        break;
    }
}



void moveServo(uint8_t id, uint16_t position, uint16_t speed)
{
  motorStatus = 1;
   // Position and speed are in a range from 0-1023
  uint8_t posLow = position & 0xFF;
  uint8_t posHigh = (position >> 8) & 0xFF;
  uint8_t speedLow = speed & 0xFF;
  uint8_t speedHigh = (speed >> 8) & 0xFF;

  // LX-824 Command format
  uint8_t command[] = {
    0x55, 0x55,               // Header
    id,                       // Servo ID
    7,                        // Length of data (excluding header)
    1,                        // Command type: Move
    posLow, posHigh,          // Target position
    speedLow, speedHigh,      // Moving speed
    0x00                      // Placeholder for checksum
  };

  // Calculate checksum
  uint8_t checksum = 0;
  for (int i = 2; i < 9; i++) {
    checksum += command[i];
  }
  command[9] = ~checksum;     // Bitwise NOT for checksum

  // Send command to servo
  SerialUART.write(command, sizeof(command));
  Serial.println("Move command sent");
  motorStatus = 0;
}

int getServoPosition(uint8_t id){
  uint8_t command[] = {
    0x55, 0x55,          // Header
    id,             // Servo ID
    0x03,                // Length of command packet
    0x1C,                // Command for position read
    0x00                         // Placeholder for checksum
  };

  // Calculate checksum
  uint8_t checksum = 0;
  for (int i = 2; i < 6; i++) {
    checksum += command[i];
  }
  command[6] = ~checksum;     // Bitwise NOT for checksum

  // Send the read command to the servo
  SerialUART.write(command, sizeof(command));

  // Wait for the response (the response packet size is 7 bytes)
  delay(10);
  
  if (SerialUART.available() >= 7) {
    uint8_t response[7];
    for (int i = 0; i < 7; i++) {
      response[i] = SerialUART.read();
    }
    
    // Verify the checksum of the response
    uint8_t responseChecksum = 0;
    for (int i = 2; i < 6; i++) {
      responseChecksum += response[i];
    }
    responseChecksum = ~responseChecksum;

    if (responseChecksum == response[6]) {
      return response[5]; // The temperature is stored in response[5]
    }
  
  }

  return -1; // Return -1 if reading failed
}
