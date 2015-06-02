/*--------------------------------------------------------------------
Code: UV-Timer.ino
Date: 02.06.2015
Author: Java-Jim  -  java-jim@gmx.com   
URL: https://hackaday.io/project/6087-uv-timer
URL: https://github.com/Java-Jim/UV-Timer
License: This code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
Purpose: It´s a simple count-down/up Timer for Switching a Relais or SSR. In my case for Timing the UV-light exposition of photo-positive pcbs.

Der Sketch verwendet 4.100 Bytes (50%) des Programmspeicherplatzes. Das Maximum sind 8.192 Bytes.
Globale Variablen verwenden 88 Bytes (17%) des dynamischen Speichers, 424 Bytes für lokale Variablen verbleiben. Das Maximum sind 512 Bytes.

toDo: 
  playtone (unblocking animation / delaymicroseconds)
  Tuning the internal osccillator on AVRtiny84 chips OSCCAL (15sec deviation in 10min)

    Parts LogicBoard: MCU ATtiny84@8MHz int. osc., ShiftRegister 2x 74HC595, 7-segment TOT-4301FG-1, Rotary Encoder, Buzzer, LED, Transistor 2N2222A, Resistors, Condensator
    Parts PowerBoard: Fuse, Transformator EL30, BridgeRectifier, LM2940-5, Ceramic + Electrolytic Condensators, Relais V23079-A1011-B301, Diode 1N4004
    
    Connections:    
    Vcc = 5v on Arduino
    
    Arduino pin 5 => 74HC595_1+2 pin 12
    Arduino pin 6 => 74HC595_1   pin 14
    Arduino pin 7 => 74HC595_1+2 pin 11
    
    74HC595_1 pin 1  (Q1)   => LED Pin 6  (B)
    74HC595_1 pin 2  (Q2)   => LED Pin 4  (C)
    74HC595_1 pin 3  (Q3)   => LED Pin 2  (D)
    74HC595_1 pin 4  (Q4)   => LED Pin 1  (E)
    74HC595_1 pin 5  (Q5)   => LED Pin 9  (F)
    74HC595_1 pin 6  (Q6)   => LED Pin 10 (G)
    74HC595_1 pin 7  (Q7)   => LED Pin 5  (DP)
    74HC595_1 pin 8  (GND)  => Ground
    74HC595_1 pin 9  (Q7S)  => 74HC595_2 pin 14
    74HC595_1 pin 10 (MR)   => Vcc (High)
    74HC595_1 pin 11 (SHCP) => Arduino pin 7
    74HC595_1 pin 12 (STCP) => Arduino pin 5
    74HC595_1 pin 13 (OE)   => Ground (Low)
    74HC595_1 pin 14 (DS)   => Arduino pin 6
    74HC595_1 pin 15 (Q0)   => LED Pin 7  (A)
    74HC595_1 pin 16 (Vcc)  => Vcc
    
    74HC595_2 pin 1  (Q1)   => LED Pin 3  Digit 2
    74HC595_2 pin 2  (Q2)   => LED Pin 2  Digit 3
    74HC595_2 pin 3  (Q3)   => NC
    74HC595_2 pin 4  (Q4)   => NC
    74HC595_2 pin 5  (Q5)   => NC
    74HC595_2 pin 6  (Q6)   => NC
    74HC595_2 pin 7  (Q7)   => NC
    74HC595_2 pin 8  (GND)  => Ground
    74HC595_2 pin 9  (Q7S)  => Not connected
    74HC595_2 pin 10 (MR)   => Vcc (High)
    74HC595_2 pin 11 (SHCP) => Arduino pin 7
    74HC595_2 pin 12 (STCP) => Arduino pin 5
    74HC595_2 pin 13 (OE)   => Ground (Low)
    74HC595_2 pin 14 (DS)   => 74HC595_1 pin 9
    74HC595_2 pin 15 (Q0)   => LED Pin 6  Digit 1
    74HC595_2 pin 16 (Vcc)  => Vcc
     
    LED anodes => 220 Ohm resistor => 74HC595_2 pin 2,3,6

 --------------------------------------------------------------------*/
 
#include <PinChangeInterrupt.h>      // Enhanced pin change interrupts for tiny processors from Rowdy Dog Software
#include <EEPROM.h>

//Pin definition
const int piezoPin = 2;        //Pin connected to PiezoSpeaker 
const int relaisPin = 3;       //Pin connected to Led + Relais Transistor
const int latchPin = 5;        //Pin connected to Pin 12 of 74HC595 (Latch)
const int dataPin  = 6;        //Pin connected to Pin 14 of 74HC595 (Data)
const int clockPin = 7;        //Pin connected to Pin 11 of 74HC595 (Clock)
const int encoderButton = 10;  //Pin connected to Button of RotarayEncoder
const int encoderPinA = 9;     //Pin connected to RotarayEncoder
const int encoderPinB = 8;     //Pin connected to RotarayEncoder

//Encoderhandling
const int upperLimit = 3600;   //Limit Encoder 60min
const int lowerLimit = 0;
volatile int encoderPos = 1;   // a counter for the dial
int lastEncoderPos = 0;        // change management
unsigned long lastInput = 0;
static boolean rotating=false; // debounce management
boolean A_set = false;              
boolean B_set = false;
boolean setMins = false;

//Buttonhandling
int ClickTime=50;              //Timer Start/Stop
int longClickTime=500;        //UV On/Off
int verylongClickTime=3000;    //Write Interval to EEPROM
unsigned long firstButtonInput;
boolean ButtonState = 0;
boolean lastButtonState = 0;
boolean buttonPressed = false;
boolean buttonLongPressed = false;
boolean buttonVeryLongPressed = false;

//Counterhandling
unsigned long t1;
unsigned long t2;
int interval = 14;
int defaultDuration = 0;
boolean counterRunning = false;
boolean counterUpRunning = false;

// Digit desciption
const byte numbers[11] = {
                    0b11111100,  //0
                    0b01100000,  //1
                    0b11011010,  //2
                    0b11110010,  //3
                    0b01100110,  //4
                    0b10110110,  //5
                    0b10111110,  //6
                    0b11100000,  //7
                    0b11111110,  //8
                    0b11100110,  //9
                    0b00000001   //DP
//                  0b11101110,  //A
//                  0b00111110,  //B
//                  0b10011100,  //C
//                  0b01111010,  //D
//                  0b10011110,  //E
//                  0b10001110   //F
                    };

const byte animation[6] = {
                    0b10000000,
                    0b01000000,
                    0b00100000,
                    0b00010000,
                    0b00001000,
                    0b00000100,
                    };
                    
// Digit anodes                    
const byte digit[3] = {128, 64, 32};

void setup(){
  pinMode(latchPin,  OUTPUT);
  pinMode(clockPin,  OUTPUT);
  pinMode(dataPin,   OUTPUT);
  pinMode(piezoPin,  OUTPUT); 
  pinMode(relaisPin, OUTPUT); 
  pinMode(encoderButton, INPUT); 
  pinMode(encoderPinA,   INPUT);
  pinMode(encoderPinB,   INPUT);
  digitalWrite(encoderButton, HIGH);
  digitalWrite(encoderPinA,   HIGH);
  digitalWrite(encoderPinB,   HIGH);
  digitalWrite(relaisPin,     LOW);
  
  attachPcInterrupt(encoderPinA, ReadEncoder, CHANGE);
  defaultDuration = EEPROMReadUint(0);
  interval = defaultDuration;
  showAnimation(2);
  t1 = millis();
}

void loop(){    
  CheckButtons();
  
  if(buttonPressed==true){
    buttonPressed=false;
    if (counterUpRunning){ 
        showAnimation(0);
        playTone(956,150);
        playTone(1915,150);
        playTone(1275,250);
        counterUpRunning=false; 
        digitalWrite(relaisPin, LOW); 
        return;
    }
    if(interval<=0) interval=defaultDuration+2;
    if (counterRunning){
       //Stop Counter
        showAnimation(0);
        playTone(956,150);
        playTone(1915,150);
        playTone(1275,250);
        counterRunning=false;
        digitalWrite(relaisPin, LOW);
    }
    else {
        //Start Counter
        showAnimation(0);
        playTone(1275,150);
        playTone(1915,150);
        playTone(956,250);
        counterRunning=true;
        digitalWrite(relaisPin, HIGH);        
    }    
  }
  
  if(buttonLongPressed==true){
      buttonLongPressed=false;
      counterRunning=false;
      if (counterUpRunning){
        //Stop CounterUp
        showAnimation(0);
        playTone(956,150);
        playTone(1915,150);
        playTone(1275,250);
        counterUpRunning=false;
        digitalWrite(relaisPin, LOW);
    }
    else {
        //Start CounterUp
        showAnimation(0);
        playTone(1275,150);
        playTone(1915,150);
        playTone(956,250);
        interval=0;
        counterUpRunning=true;
        digitalWrite(relaisPin, HIGH);        
    }
  }
  
  if(buttonVeryLongPressed==true){
      EEPROMWriteUint(0, interval - 1);
      defaultDuration = interval -1;
      buttonVeryLongPressed=false;
      showAnimation(0);
      playTone(956,150);
      playTone(956,150);
      playTone(956,150);
  }
  
  //Stepdown Counter
  if (counterRunning==true ){
    t2 = millis();
    if(t2 - t1 > 1000){
        interval--;
        t1 = t2;
        if(interval <= 0){
            showAnimation(0);
            playTone(956,150);
            playTone(1915,150);
            playTone(1275,150);
            playTone(1275,150);
            interval = 0;
            counterRunning=false;
            digitalWrite(relaisPin, LOW);  
        }
    }
  }
  
  //Stepup Counter
  if (counterUpRunning==true ){
    t2 = millis();
    if(t2 - t1 > 1000){
        interval++;
        t1 = t2;
    }
  }
  
  //7-Seg Display Handling
  if(interval>599){
    //Minuten Ziffern
    int b = (interval / 60) / 10;
    int c = (interval / 60) % 10;
    show(digit[1], numbers[b]);
    show(digit[2], numbers[c]);
  }
  else{ 
    //Minuten.Sekunden Ziffern
    int a = interval / 60;
    int b = (interval % 60) / 10;
    int c = (interval % 60) % 10;
    show(digit[0], numbers[a]);
    show(digit[0], numbers[10]);
    show(digit[1], numbers[b]);
    show(digit[2], numbers[c]);
  }
}

void CheckButtons(){
  //Handle Encoder
  rotating = true; 
  if (lastEncoderPos != encoderPos){
    lastInput = millis();
    lastEncoderPos = encoderPos;
    if (!digitalRead(encoderButton)){
        encoderPos *= 60;
        setMins=true;
    }
    else  
        setMins=false;
    if ((interval>=600) && (!setMins))
        encoderPos *= 60;
    interval += encoderPos;
    if ((interval + encoderPos) > upperLimit) interval = upperLimit;
    if ((interval + encoderPos) < lowerLimit) interval = lowerLimit; 
    encoderPos = 0;
  }
  
  //Handle Button
  ButtonState = !digitalRead(encoderButton);
  if (setMins){ 
      //setMins=false;
      lastButtonState = ButtonState; 
      return;
  }
  if (ButtonState && !lastButtonState) firstButtonInput = millis();
  if (!ButtonState && lastButtonState){
      if ((millis()-firstButtonInput) > ClickTime){
          buttonPressed=true;
          buttonLongPressed=false;
          buttonVeryLongPressed=false;
      }
      if ((millis()-firstButtonInput) > longClickTime){
          buttonPressed=false;
          buttonLongPressed=true;
          buttonVeryLongPressed=false;
      }
      if ((millis()-firstButtonInput) > verylongClickTime){
          buttonPressed=false;
          buttonLongPressed=false;
          buttonVeryLongPressed=true;
     }  
  }
  lastButtonState = ButtonState;
}

void ReadEncoder() {
  if ( rotating ) delay (1);
  if(digitalRead(encoderPinA) != A_set) {
    A_set = !A_set;
    B_set = !B_set;
    // adjust counter + if A leads B
    if (A_set == digitalRead(encoderPinB)){
      if (B_set==true) encoderPos += 1;}
    else{
      if (B_set==true) encoderPos -= 1;}
    rotating = false; 
  }
}

void showAnimation(int count){
  for (int i=0; i<=count; i++){
    for (int j=0; j<=5; j++){
      for (int k=0; k<=25; k++){
        show(digit[0], animation[j]);
        show(digit[1], animation[j]);
        show(digit[2], animation[j]);
      }
    }
  }
  show(digit[0], 0);
  show(digit[1], 0);
  show(digit[2], 0);
}

void show( byte digit, byte number){
  for(int i=0; i<=7; i++){
    byte toWrite = number & (0b10000000 >> i); 
    if(!toWrite) { continue; }
    shiftIt(digit, toWrite); 
  }
}

void shiftIt (byte digit, byte data){
    digitalWrite(latchPin, LOW);

    // Send Digit Number
    for (int l=0; l<=7; l++)
    {
        // clockPin LOW prior to sending bit
        digitalWrite(clockPin, LOW);  
        if ( digit & (1 << l))
        {
          digitalWrite(dataPin, HIGH); // turn "On"
        }
        else
        {	
          digitalWrite(dataPin, LOW); // turn "Off"
        }
      digitalWrite(clockPin, HIGH); 
     }
      
    // Send Data
    for (int j=0; j<=7; j++)
    {
      digitalWrite(clockPin, LOW); 
      if ( data & (1 << j) )
      {
        digitalWrite(dataPin, LOW); // turn "On"
      }
      else
      {	
        digitalWrite(dataPin, HIGH); // turn "Off"
      } 
      digitalWrite(clockPin, HIGH); 
    }
    
    digitalWrite(clockPin, LOW); 
    digitalWrite(latchPin, HIGH);
   // delay(20);  
}

void playTone(int tone, int duration) {
  for (long i = 0; i < duration * 1000L; i += tone * 2) {
    digitalWrite(piezoPin, HIGH);
    delayMicroseconds(tone);
    digitalWrite(piezoPin, LOW);
    delayMicroseconds(tone);
  }
}

unsigned int EEPROMReadUint(int address)
{
  unsigned int two = EEPROM.read(address);
  unsigned int one = EEPROM.read(address + 1);
  return ((two) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}

void EEPROMWriteUint(int address, unsigned int value)
{
 byte two = (value & 0xFF);
 byte one = ((value >> 8) & 0xFF);
 EEPROM.write(address, two);
 EEPROM.write(address + 1, one);
} 
