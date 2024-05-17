#include <LiquidCrystal.h>
#define A A2
#define B A3
#define C A4
#define D A0

#define NUMBER_OF_STEPS_PER_REV 512

int PIRinputPin = 8;               // input pin for the PIR sensor
int pirState = LOW;             // start off the PIR sensor assumin no motion
int PIRval = 0;                    // PIR variable
int red_light_pin= 7;           // pin for the red of the RGB led
int green_light_pin = 10;           // pin for the green of the RGB led
int blue_light_pin = 9;           // pin for the blue of the RGB led
const int tempsensor=A5;           //temperature sensor on pin A5
float tempc; //variable to store temperature in degree Celsius
String currentrequest;      //current request on the Serial line
int stovestatus=0;       //1 if stove is on, 0 if stove is off, start by assuming stove is off

//pins for the liquidcrystal display
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);


void setup() {
  pinMode(tempsensor,INPUT); // Configuring temp sensor pin as input 
  
  //configuing pins for the RGB LED
  pinMode(red_light_pin, OUTPUT);
  pinMode(green_light_pin, OUTPUT);
  pinMode(blue_light_pin, OUTPUT);

  //configuring the motor pins
  pinMode(A,OUTPUT);
  pinMode(B,OUTPUT);
  pinMode(C,OUTPUT);
  pinMode(D,OUTPUT);
  
  lcd.begin(16, 2); //start the Liquid Crystal display
  Serial.begin(9600); //start the serial communication

  pinMode(PIRinputPin, INPUT);     //configure PIR pin

}

//Commands for the stepper motor
void motorcommand(int a,int b,int c,int d){
  digitalWrite(A,a);
  digitalWrite(B,b);
  digitalWrite(C,c);
  digitalWrite(D,d);
}

//converts the RGB naming into analog values for the pins
void RGB_color(int red_light_value, int green_light_value, int blue_light_value) {
  analogWrite(red_light_pin, red_light_value);
  analogWrite(green_light_pin, green_light_value);
  analogWrite(blue_light_pin, blue_light_value);
}

//Commands to spin the motor forward
void forwards(){
motorcommand(1,0,0,0);
delay(5);
motorcommand(1,1,0,0);
delay(5);
motorcommand(0,1,0,0);
delay(5);
motorcommand(0,1,1,0);
delay(5);
motorcommand(0,0,1,0);
delay(5);
motorcommand(0,0,1,1);
delay(5);
motorcommand(0,0,0,1);
delay(5);
motorcommand(1,0,0,1);
delay(5);
}

void loop() {
  currentrequest = "";
  lcd.setCursor(0, 1); //sets the cursor to the top left
  RGB_color(255, 0, 0); // Red means no request recieved yet
  if (Serial.available() > 0) { //waits until request comes in
    currentrequest = Serial.readString(); //gets the request
    RGB_color(0, 255, 0); // Green means data recieved and processing
    currentrequest.trim(); //removes the newline character
    delay(1000);
  }
  if (currentrequest.equals("get_temp")){
    tempc=analogRead(tempsensor); //Reading the value from sensor
    tempc=(tempc*500)/1023;
    lcd.clear();
    lcd.print(tempc);
    lcd.print(" degrees C");
    Serial.print(tempc); //print to the serial port
    Serial.println(" degrees C");
    RGB_color(0, 0, 255); // Blue means request furfilled
    delay(1000);
  }
  if (currentrequest.equals("stove_on")){
    //turn the oven on
    if (stovestatus == 1){
      Serial.println("Stove is already on"); //if stove is on, nothing else to do
      RGB_color(0, 0, 255); // Blue means request furfilled
      delay(1000);
    } else {
      int i;
      i=0;

      while(i<200){
       forwards();
       i++;
    }
    stovestatus = 1; //stove is now on
    Serial.println("DONE");
    RGB_color(0, 0, 255); // Blue means request furfilled
    delay(1000);
    }
  }
  if (currentrequest.equals("stove_off")){
    //turn the oven off
    if (stovestatus == 0){
      Serial.println("Stove is already off"); //if stove is off, nothing else to do
      RGB_color(0, 0, 255); // Blue means request furfilled
      delay(1000);
    } else {
      int i;
      i=0;

      while(i<700){
       forwards();
       i++;
    }
    stovestatus = 0; //stove is now off
    Serial.println("DONE");
    RGB_color(0, 0, 255); // Blue means request furfilled
    delay(1000);
    }
  }
  if (currentrequest.equals("overflow_status")){
    //see if pot is overflowing
    PIRval = digitalRead(PIRinputPin);  // read input value
    if (PIRval == HIGH) {            // check if the input is HIGH
     if (pirState == LOW) {
       // we have just turned on
       Serial.println("Pot boiling over!");
        // We only want to print on the output change, not state
        pirState = HIGH;
      }
    } else {
     if (pirState == HIGH){
       // we have just turned of
       Serial.println("Pot stopped boiling over");
       // We only want to print on the output change, not state
        pirState = LOW;
     }
     Serial.println("Pot is ok");
   }
  RGB_color(0, 0, 255); // Blue means request furfilled
  delay(1000);
  } 
}
