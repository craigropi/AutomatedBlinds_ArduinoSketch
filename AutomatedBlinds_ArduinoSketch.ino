
#include <CurieBLE.h>
#include <CurieTimerOne.h>

//Clockwise rotor movement moves the blinds down.
BLEPeripheral blePeripheral;  // BLE Peripheral Device (the board you're programming)
BLEService blindsService("19B10000-E8F2-537E-4F6C-D104768A1214"); // BLE LED Service

// BLE Blinds Characteristic - custom 128-bit UUID, read and writable by central
BLEUnsignedCharCharacteristic blindsCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite);

int blindsPos = 0;    // variable to store blinds position. 0 is uncalibrated.  1 is down. 100 is fully up.  0 Is not initialized. 99 Is just below the limit switch.
static unsigned long lastLimitSwitchTime = 0;  //stores the time of the last limit switch transition in milliseconds.
//static unsigned long currentInterruptTime = 0;  //stores the time of the current interrupt in milliseconds
int limitSwitchState = 0; // stores the state of the limit switch'
int goingDown = 0; // 1 if motor is currently going down, 0 if not.
int goingUp = 0; // 1 if motor is currently going Up, 0 if not.
int debouncing = 0; // 1 if the 
static unsigned long goingDownStartTime = 0; // Time in ms whent the motor stated going down.
static unsigned long goingUpStartTime = 0; // Time in ms whent the motor stated going up.
static unsigned long motorNudgeDownTime = 1000; // The amount of time the motor should be driven down when commanded to nudge down, in ms.
static unsigned long motorNudgeUpTime = 1000; // The amount of time the motor should be driven down when commanded to nudge down, in ms.
static unsigned long debounceTime = 200; // Time the limit switch should be bounced, in ms.

int interruptMessagetoBePrinted = 0; // Bitmask for messages to be printed from interrupts
String interruptMessage1 = "limitPressed Interrupt Complete";
String interruptMessage2 = "Interrupt set blinds position to 100.";
String interruptMessage4 = "Interrupt set blinds position to 99.";


void setup() {
  Serial.begin(9600);  //This line opens a serial port so you can print things to the console for debugging.

  pinMode(2, OUTPUT);   // sets pin 2 (DIR) as output
  pinMode(3, OUTPUT);   // sets pin 3 (PWM) as output
  pinMode(4, OUTPUT);   // sets pin 4 (/SLP) as output
  pinMode(5, INPUT);   // sets pin 5 (/FAULT) as input
  pinMode(6, INPUT);   // sets pin 6 (CURRENT-SENSE) as input.  CS = 50mV +20mV/A, way to wide a range for this application.
  pinMode(13, INPUT_PULLUP);   // sets pin 13 as switch input with a pullup.  Pin is shorted to ground when blinds are up fully.
  attachInterrupt(13, limitPressed, FALLING);
  
  // set advertised local name and service UUID:
  blePeripheral.setLocalName("Automated Blinds");
  blePeripheral.setAdvertisedServiceUuid(blindsService.uuid());

  // add service and characteristic:
  blePeripheral.addAttribute(blindsService);
  blePeripheral.addAttribute(blindsCharacteristic);

  // set the initial value for the characeristic:
  blindsCharacteristic.setValue(0);

  // begin advertising BLE service:
  blePeripheral.begin();

  Serial.println("BLE Blinds Peripheral");
}

void loop() {
  digitalWrite(4, LOW); //Put the motor driver in sleep mode
  // listen for BLE peripherals to connect:
  BLECentral central = blePeripheral.central();
  
  checkMessagetobePrinted();
  checkGoingDown();
  checkGoingUp();
  checkifNoLongerUp();
  checkifDebouncing();
  
  // if a central is connected to peripheral:
  if (central) {
    //Serial.print("Connected to central: ");
    // print the central's MAC address:
    //Serial.println(central.address());

    // while the central is still connected to peripheral:
    while (central.connected()) {
      checkMessagetobePrinted();
      checkGoingDown();
      checkGoingUp();
      checkifNoLongerUp();
      checkifDebouncing();
      
      // if the remote device wrote to the characteristic,
      // use the value to control the LED:
      if (blindsCharacteristic.written()) {
        digitalWrite(4, HIGH); //Take the motor driver out of sleep mode
        limitSwitchState = digitalRead(13);
        if (blindsCharacteristic.value()) {   // any value other than 0 "Roll Blinds Down"
          //Serial.println(F("Blinds Down"));
          digitalWrite(2, LOW);  //Run the motor in the clockwise direction
          // Run the PWM (pin#, %ofPWM [MUST HAVE '.0' at end or this works a different way], Period in us)
          CurieTimerOne.pwmStart(3, 80.0, 50);
          goingDown = 1;
          goingDownStartTime = millis();
          //delay(1000);                       // Run the motor for 500ms
          //CurieTimerOne.pwmStop();
        } else if(blindsPos != 100 && limitSwitchState == HIGH){   // a 0 value "Roll Blinds Up", as long as the blinds aren't fully up already (limit switch is high and saved state is not 100)
          //Serial.println(F("Blinds Up"));
          digitalWrite(2, HIGH);  //Run the motor in the counterclockwise direction
          // Run the PWM (pin#, %ofPWM [MUST HAVE '.0' at end or this works a different way], Period in us)
          CurieTimerOne.pwmStart(3, 100.0, 50);
          goingUp = 1;
          goingUpStartTime = millis();
          //delay(1000);                       // Run the motor for 500ms
          //CurieTimerOne.pwmStop();
        } else {
          //Serial.println(F("Blinds were not raised, because they're already all the way up"));
        }
        // digitalWrite(4, LOW); //Put the motor driver in sleep mode
      }
    }
  }
}

// ISR triggered by a falling edge on pin 13 (limit switch is pressed).
void limitPressed() {
  if (blindsPos != 100 && debouncing != 1) {
    CurieTimerOne.pwmStop();  // Stop driving the motor's PWM
    digitalWrite(4, LOW); //Put the motor driver in sleep mode
    debouncing = 1;
    lastLimitSwitchTime = millis();
    blindsPos = 100;
    //Serial.println("Interrupt set blinds position to 100.");
    //Serial.println("limitPressed Interrupt Complete");
    interruptMessagetoBePrinted = 3;
  }
}

void checkifDebouncing () {
  if (millis() - lastLimitSwitchTime > debounceTime) {
    debouncing = 0;
  }
}

// Checks if the limit switch is no longer engaged and sets blindsPos to 99 if so.
void checkifNoLongerUp() {
  if (blindsPos == 100 && debouncing != 1) {
    limitSwitchState = digitalRead(13);
    if (limitSwitchState == HIGH) {
    blindsPos = 99;
    Serial.println("Set blinds position to 99, because limit switch was released.");
    debouncing  = 1;
    lastLimitSwitchTime = millis();
    }
  }
}

// Was an ISR for the limit switch being released, but this is being handles by a normal function now, since timing is not critical.
/*void limitReleased() {
  currentInterruptTime = millis();
  if (currentInterruptTime - lastInterruptTime > 500) {
    blindsPos = 99;
    //Serial.println("Interrupt set blinds position to 99.");
    interruptMessagetoBePrinted = 4;
  }
  lastInterruptTime = currentInterruptTime;
  //Serial.println("LimitReleased Interrupt Complete");
  interruptMessagetoBePrinted++;
}
*/

void checkMessagetobePrinted() {
  if (interruptMessagetoBePrinted) {  // If there is a message to be printed
    switch (interruptMessagetoBePrinted) { //interruptMessagetoBePrinted is a bitmask for the messages that should be printed.
      case 1:
      Serial.println(interruptMessage1);
      case 2:
      Serial.println(interruptMessage2);
      case 3:
      Serial.println(interruptMessage2);
      Serial.println(interruptMessage1);
      case 4:
      Serial.println(interruptMessage4);
      case 5:
      Serial.println(interruptMessage4);
      Serial.println(interruptMessage1);
    }
    interruptMessagetoBePrinted = 0;
  }
}

// Checks if the motor is currently going down, and if so, stops it if it's been 'motorNudgeDownTime' since starting.
void checkGoingDown() { 
  if (goingDown && millis() - goingDownStartTime > motorNudgeDownTime) { 
    CurieTimerOne.pwmStop(); // Stop driving the motor's PWM
    digitalWrite(4, LOW); //Put the motor driver in sleep mode
    goingDown = 0;
  }
}

// Checks if the motor is currently going up, and if so, stops it if it's been 'motorNudgeUpTime' since starting.
void checkGoingUp() {
  if (goingUp && millis() - goingUpStartTime > motorNudgeUpTime) {
    CurieTimerOne.pwmStop(); // Stop driving the motor's PWM
    digitalWrite(4, LOW); //Put the motor driver in sleep mode
    goingUp = 0;  
  }
}



