
#include <CurieBLE.h>
#include <CurieTimerOne.h>

//Clockwise rotor movement moves the blinds down.
//In this file, 'roll' menas roll the blinds all the way up/down, nudge means move them a small distance.
BLEPeripheral blePeripheral;  // BLE Peripheral Device (the board you're programming)
BLEService blindsService("19B10000-E8F2-537E-4F6C-D104768A1214"); // BLE LED Service

// BLE Blinds Characteristic - custom 128-bit UUID, read and writable by central
BLEUnsignedCharCharacteristic blindsCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite);

static unsigned long blindsPos = 0;    // variable to store blinds position. 0 is uncalibrated.  1 is down. 100 is fully up.  0 Is not initialized. 99 Is just below the limit switch.
static unsigned long lastLimitSwitchTime = 0;  //stores the time of the last limit switch transition in milliseconds.
//static unsigned long currentInterruptTime = 0;  //stores the time of the current interrupt in milliseconds
int limitSwitchState = 0; // stores the state of the limit switch.
int debouncing = 0; // 1 if the switch has been toggled .

int rollingDown = 0; // 1 if motor is currently rolling down, 0 if not.
int rollingUp = 0; // 1 if motor is currently rolling up, 0 if not.
int nudgingDown = 0; // 1 if motor is currently nudging down, 0 if not.
int nudgingUp = 0; // 1 if motor is currently nudging up, 0 if not.

static unsigned long nudgeDownStartTime = 0; // Time in ms when the motor started nudging down.
static unsigned long nudgeUpStartTime = 0; // Time in ms when the motor started nudging up.
static unsigned long rollDownStartTime = 0; // Time in ms when the motor started rolling down.
static unsigned long rollUpStartTime = 0; // Time in ms when the motor started rolling up.
static unsigned long motorNudgeDownTime = 500; // The amount of time the motor should be driven down when commanded to nudge down, in ms.
static unsigned long motorNudgeUpTime = 500; // The amount of time the motor should be driven down when commanded to nudge down, in ms.
static unsigned long motorRollDownTime = 4300; // The amount of time the motor should be driven down when commanded to roll down, in ms.
static unsigned long motorRollUpTime = 4300; // The amount of time the motor should be driven down when commanded to roll down, in ms.

static unsigned long rollDownConstant = 40; // The amount of time it takes the blinds to roll down 1%, in ms.
static unsigned long rollUpConstant = 40; // The amount of time it takes the blinds to roll up 1%, in ms.

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
  //Motor Driver should always be in sleep mode if the is no BLE device connected.
  digitalWrite(4, LOW); //Put the motor driver in sleep mode
  // listen for BLE peripherals to connect:
  BLECentral central = blePeripheral.central();
  
  checkMessagetobePrinted();
  checkNudgingDown();
  checkNudgingUp();
  checkRollingDown();
  checkRollingUp();
  checkifNoLongerUp();
  checkifDebouncing();
  
  // if a central is connected to BLE peripheral:
  if (central) {
    //Serial.print("Connected to central: ");
    // print the central's MAC address:
    //Serial.println(central.address());

    // while the central is still connected to peripheral:
    while (central.connected()) {
      checkMessagetobePrinted();
      checkNudgingDown();
      checkNudgingUp();
      checkRollingDown();
      checkRollingUp();
      checkifNoLongerUp();
      checkifDebouncing();
      
      
      // if the remote device wrote to the characteristic and the blinds are not in motion already,
      // use the value set via BLE to control the blinds
      if (blindsCharacteristic.written() && rollingDown == 0 && rollingUp == 0 && nudgingDown == 0 && nudgingUp == 0) {
        digitalWrite(4, HIGH); //Take the motor driver out of sleep mode
        limitSwitchState = digitalRead(13);
        if (blindsCharacteristic.value() == 1) {   // Value of 1 means "Nudge Blinds Down"
          Serial.println("Nudge Blinds Down");
          Serial.println(blindsPos);
          digitalWrite(2, LOW);  //Run the motor in the clockwise direction
          nudgingDown = 1;
          nudgeDownStartTime = millis();
          // Run the PWM (pin#, %ofPWM [MUST HAVE '.0' at end or this works a different way], Period in us)
          CurieTimerOne.pwmStart(3, 77.0, 50);
        
        } else if(blindsCharacteristic.value() == 3 && blindsPos > 1){   // Value of 3 means "Roll Blinds Down" all the way
          Serial.println("Roll Blinds Down");
          Serial.println(blindsPos);
          digitalWrite(2, LOW);  //Run the motor in the clockwise direction
          rollingDown = 1;
          rollDownStartTime = (millis() - ((100-blindsPos)*rollDownConstant)); // When rolling down, prorate the roll down by however much the blinds are already rolled down.
          Serial.println(millis());
          Serial.println(((100-blindsPos)*rollDownConstant));
          // Run the PWM (pin#, %ofPWM [MUST HAVE '.0' at end or this works a different way], Period in us)
          CurieTimerOne.pwmStart(3, 77.0, 50);
        
        } else if (blindsCharacteristic.value() == 0 && blindsPos != 100 && limitSwitchState == HIGH){   // Value of 0 means "Nudge Blinds Up", as long as the blinds aren't fully up already (limit switch is high and saved state is not 100)
          Serial.println("Nudge Blinds Up");
          Serial.println(blindsPos);
          digitalWrite(2, HIGH);  //Run the motor in the counterclockwise direction
          nudgingUp = 1;
          nudgeUpStartTime = millis();
          // Run the PWM (pin#, %ofPWM [MUST HAVE '.0' at end or this works a different way], Period in us)
          CurieTimerOne.pwmStart(3, 100.0, 50);
        
        } else if(blindsCharacteristic.value() == 2 && blindsPos != 100 && limitSwitchState == HIGH){   // a 2 value "Roll Blinds Up" all the way, as long as the blinds aren't fully up already (limit switch is high and saved state is not 100)
          Serial.println("Roll Blinds Up");
          Serial.println(blindsPos);
          digitalWrite(2, HIGH);  //Run the motor in the counterclockwise direction
          rollingUp = 1;
          rollUpStartTime = millis();
          // Run the PWM (pin#, %ofPWM [MUST HAVE '.0' at end or this works a different way], Period in us)
          CurieTimerOne.pwmStart(3, 100.0, 50);
        } else {
          //Serial.println(F("Blinds were not raised, because they're already all the way up"));
        }
      }
    }
  }
}

// ISR triggered by a falling edge on pin 13 (limit switch is pressed).
void limitPressed() {
  if (blindsPos != 100 && debouncing != 1) {
    digitalWrite(4, LOW); //Put the motor driver in sleep mode
    CurieTimerOne.pwmStop();  // Stop driving the motor's PWM
    nudgingDown = 0;
    rollingDown = 0;
    nudgingUp = 0;
    rollingUp = 0;
    debouncing = 1;
    lastLimitSwitchTime = millis();
    blindsPos = 100;
    //Serial.println("Interrupt set blinds position to 100.");
    //Serial.println("limitPressed Interrupt Complete");
    interruptMessagetoBePrinted = 3;
  }
}

void checkifDebouncing () {
  if (debouncing == 1 && ((millis() - lastLimitSwitchTime) > debounceTime)) {
    debouncing = 0;
    Serial.println((String) "Debouncing disabled, blindsPos = " + blindsPos);
  }
}

// Checks if the limit switch is no longer engaged and sets blindsPos to 99 if so.
void checkifNoLongerUp() {
  if (blindsPos == 100 && debouncing != 1 && (rollingDown || nudgingDown)) {
    limitSwitchState = digitalRead(13);
    if (limitSwitchState == HIGH) {
    lastLimitSwitchTime = millis();
    debouncing  = 1;
    blindsPos = 99;
    Serial.println("Set blinds position to 99, because limit switch was released.");
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
    Serial.println((String)"interruptMessagetoBePrinted " + interruptMessagetoBePrinted);
    switch (interruptMessagetoBePrinted) { //interruptMessagetoBePrinted is a bitmask for the messages that should be printed.
      case 1:
      Serial.println(interruptMessage1);
      break;
      case 2:
      Serial.println(interruptMessage2);
      break;
      case 3:
      Serial.println(interruptMessage2);
      Serial.println(interruptMessage1);
      break;
      case 4:
      Serial.println(interruptMessage4);
      break;
      case 5:
      Serial.println(interruptMessage4);
      Serial.println(interruptMessage1);
      break;
    }
    interruptMessagetoBePrinted = 0;
  }
}

//String interruptMessage1 = "limitPressed Interrupt Complete";
//String interruptMessage2 = "Interrupt set blinds position to 100.";
//String interruptMessage4 = "Interrupt set blinds position to 99.";

// Checks if the motor is currently nudging down, and if so, stops it if it's been 'motorNudgeDownTime' since starting.
void checkNudgingDown() { 
  if (nudgingDown && ((millis() - nudgeDownStartTime) > motorNudgeDownTime)) { 
    CurieTimerOne.pwmStop(); // Stop driving the motor's PWM
    digitalWrite(4, LOW); //Put the motor driver in sleep mode
    nudgingDown = 0;
    if(blindsPos > motorNudgeDownTime/rollDownConstant) {
      blindsPos -= (motorNudgeDownTime/rollDownConstant);
    } else {
      blindsPos = 1;
    }
  }
}

// Checks if the motor is currently rolling down, and if so, stops it if it's been 'motorRollDownTime' since starting.
void checkRollingDown() { 
  if (rollingDown && ((millis() - rollDownStartTime) > motorRollDownTime)) { 
    CurieTimerOne.pwmStop(); // Stop driving the motor's PWM
    digitalWrite(4, LOW); //Put the motor driver in sleep mode
    rollingDown = 0;
    blindsPos = 1;
  }
}

// Checks if the motor is currently nudging up, and if so, stops it if it's been 'motorNudgeUpTime' since starting.
void checkNudgingUp() {
  if (nudgingUp && ((millis() - nudgeUpStartTime) > motorNudgeUpTime)) {
    CurieTimerOne.pwmStop(); // Stop driving the motor's PWM
    digitalWrite(4, LOW); //Put the motor driver in sleep mode
    nudgingUp = 0;
    if(blindsPos < (100-motorNudgeDownTime/rollDownConstant)) {
      blindsPos += (motorNudgeUpTime/rollUpConstant);
    } else {
      blindsPos = 100;
    }
  }
}

// Checks if the motor is currently rolling up, and if so, stops it if it's been 'motorRollUpTime' since starting.
void checkRollingUp() {
  if (rollingUp && ((millis() - rollUpStartTime) > motorRollUpTime)) {
    CurieTimerOne.pwmStop(); // Stop driving the motor's PWM
    digitalWrite(4, LOW); //Put the motor driver in sleep mode
    rollingUp = 0;
    blindsPos = 99; // Will only get here if the limit switch is not hit.
  }
}



