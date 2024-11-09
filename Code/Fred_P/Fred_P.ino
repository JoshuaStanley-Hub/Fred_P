/*
   Description: This program is used to control a remote control hexagonal folding robot with vibrational turning

        Wiring: The required components are 3x MG90S servos, a NRF24L01 radio module, and an Arduino Nano
        The servos are connected with brown wire to GND, red wire to Vin, and orange wire to D3, D5, and D6
        The NRF24L01 is connected 3.3V to 3v3, GND to GND, CSN to D10, MOSI to D11, CE to D9, SCK to D13, MISO to D12
*/

//Libraries
#include "Servo.h"
Servo servo[3];

#include "SPI.h"
#include "RF24.h"
RF24 radio(9, 10);

#include "Ramp.h"
ramp angle[3];

//Variables
byte cnt = 0;
byte mode = 0;
int yDir = 0;
int xDir = 0;
int duration = 0;                                                                                   //Duration of move from one shape to the next
int period = 0;                                                                                     //Period of vibration
byte data[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
byte but[][4] = {{2, 3, 6, 7}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
unsigned long millisPrev = 0;

//Constants
const byte pinout[3] = {6, 5, 3};                                                                   //Array defining servo connections
const byte chan[6] = "00007";                                                                       //Radio channel used
const byte straight[3] = {5, 0, 0};                                                                 //Calibration array defining the written positions of each servo when the joint is straight
const byte right[3] = {99, 93, 93};                                                                 //Calibration array defining the written positions of each servo when the joint is at a right angle
const byte flat[3] = {172, 172, 16};                                                                //Array with written position of each servo for the robot to go to its flat state

const byte seq[4][6][3] = {   /*[mode][cnt][servo]*/                                                //Array containing the servo angles to cycle through for each rolling gait
  {{150, 150, 60}, {60, 150, 150}, {150, 60, 150}, {150, 150, 60}, {60, 150, 150}, {150, 60, 150}},
  {{163, 163, 94}, {60, 120, 120}, {163, 94, 163}, {120, 120, 60}, {94, 163, 163}, {120, 60, 120}},
  {{180, 90, 90}, {180, 135, 45}, {90, 180, 90}, {45, 180, 135}, {90, 90, 180}, {135, 45, 180}},
  {{180, 90, 90}, {150, 150, 60}, {90, 180, 90}, {60, 150, 150}, {90, 90, 180}, {150, 60, 150}}
};
int offset[][3] {{4, 4, -8}, { -8, 4, 4}, {4, -8, 4}, {4, 4, -8}, { -8, 4, 4}, {4, -8, 4}};         //Array containing the offsets to apply to each angle to result in vibration

void setup() {
  Serial.begin(9600);
  Serial.println("Serial Communication Initialized");

  radio.begin();                                                                                    //Begin radio communication
  radio.openReadingPipe(1, chan);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();
  Serial.println("Radio Communication Initialized");

  for (int i = 0; i < 3; i++) {                                                                     //Initialize servos
    servo[i].attach(pinout[i]);
    angle[i].go(right[i] - (right[i] - straight[i]) / 3);
    servo[i].write(angle[i].update());
  }
  Serial.println("Servos Attached");

  delay(250);
  Serial.println("Running");
}

void loop() {
  if (radio.available()) {
    radio.read(&data, sizeof(data));                                                                //Read in radio data if available

    if (data[1] == 0) yDir = 0;                                                                     //Forward and reverse controlled by front to back motion of left joystick
    else if (data[1] > 127) yDir = 1;
    else yDir = -1;

    if (data[4] == 0) xDir = 0;                                                                     //Turning controlled by left to right motion of right joystick
    else if (data[4] > 127) xDir = 1;
    else xDir = -1;

    duration = map(data[9], 0, 255, 400, 100);                                                      //Driving speed controlled by right potentiometer
    period = map(data[8], 0, 255, 5, 25);                                                           //Vibrational turning rate controlled by left potentimeter
  }
  debounce();

  if (but[2][2]) {                                                                                  //Goes to flat state if right joystick is pressed
    for (int i = 0; i < 3; i++) {
      angle[i].go(map(flat[i], 90, 180, right[i], straight[i]));
    }
  }
  else if (but[2][3]) {                                                                             //Increment rolling gait [mode] if right trigger is pressed
    if (mode == 3) mode = 0;
    else mode++;
    cnt = 0;
    for (int i = 0; i < 3; i++) {
      angle[i].go(map(seq[mode][cnt][i], 90, 180, right[i], straight[i]), duration);
    }
  }
  else if (but[2][1]) {                                                                             //Decrement rolling gait [mode] if left trigger is pressed
    if (mode == 0) mode = 3;
    else mode--;
    cnt = 0;
    for (int i = 0; i < 3; i++) {
      angle[i].go(map(seq[mode][cnt][i], 90, 180, right[i], straight[i]), period);
    }
  }

  if ((millis() - millisPrev) > angle[0].getDuration()) {                                           //If the previous shape change is complete:
    if (yDir != 0) {                                                                                //Increment or decrement the shape [cnt] (depending on forward or reverse commanded)
      if (yDir > 0) {
        if (cnt == 5) cnt = 0;
        else cnt++;
      }
      else {
        if (cnt == 0) cnt = 5;
        else cnt--;
      }
      for (int i = 0; i < 3; i++) {
        angle[i].go(map(seq[mode][cnt][i], 90, 180, right[i], straight[i]), duration);              //Send ramp variable to angles of next shape in sequence
      }
      millisPrev = millis();
    }
    else if (xDir != 0 && mode == 0) {                                                              //Move to appropriate panel and vibrate if turning is commanded
      mode = 0;
      if (xDir > 0) {
        if (cnt == 1 || cnt == 3)cnt++;
        if (cnt == 5) cnt = 0;
      }
      else {
        if (cnt == 0 || cnt == 2 || cnt == 4) cnt++;
      }

      for (int i = 0; i < 3; i++) {
        angle[i].go(map((seq[mode][cnt][i] + offset[cnt][i]), 90, 180, right[i], straight[i]), period);
        offset[cnt][i] = -offset[cnt][i];
      }
      millisPrev = millis();
    }
  }
  for (int i = 0; i < 3; i++) {
    servo[i].write(angle[i].update());                                                              //Set servo angles to updated ramp angle value
  }
}

void debounce() {                                                                                   //Causes momentary button inputs from controller to trigger on once when pressed and not for the duration of being pressed
  for (int i = 0; i < 4; i++) {
    if (data[but[0][i]]) {
      if (!but[1][i]) {
        but[2][i] = 1;
      }
      else but[2][i] = 0;
      but[1][i] = 1;
    }
    else {
      but[1][i] = 0;
      but[2][i] = 0;
    }
  }
}
