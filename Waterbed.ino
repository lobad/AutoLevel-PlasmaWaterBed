
#include <TM1637Display.h>
#include <Wire.h>
#include <Arduino.h>

#define PistonPin 12
#define FillPin 7
#define DumpPin 8
#define RaisePin 9
#define LowerPin 10
#define CLK 2 //Display
#define DIO 3 //Display
#define PressurePin A0 //Transistor 
#define RelayPinA 4 //Air In
#define RelayPinB 5 //Air Out
#define RelayPinC 6 //Piston
#define RelayPinD 11 //Unused

// Create display object of type TM1637Display:
TM1637Display display = TM1637Display(CLK, DIO);
// Create array that turns all segments on:
const uint8_t data[] = {0xff, 0xff, 0xff, 0xff};
// Create array that turns all segments off:
const uint8_t blank[] = {0x00, 0x00, 0x00, 0x00};
    
const int pressurePin = A0; //select the analog input pin for the pressure transducer
const float voltageZero = 0.5; //analog reading of pressure transducer at 0psi
const float voltageMax = 4.5; //analog reading of pressure transducer at 30psi
const float pressuretransducermaxPSI = 30; //psi value of transducer being used
const int baudRate = 9600; //constant integer to set the baud rate for serial monitor
const int sensorreadDelay = 250; //constant integer to set the sensor read delay in milliseconds
//Average PSI reading
const int numReadings = 5;

int rawPressureVal[numReadings];      // the readings from the PSI input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int avgPressureValue = 0;                // the average
float fillMax = 2; //Max PSI for waterbed


enum State {
  NOTHING,
  FILLING,
  FILLING_CANCEL,
  DUMPING,
  DUMPING_CANCEL,
  RAISING,
  RAISING_CANCEL,
  LOWERING,
  LOWERING_CANCEL
};

enum Button {
  NONE,
  FILL,
  DUMP,
  RAISE,
  LOWER
};

State currentState = NOTHING;
Button previousButton = NONE;

Button getButton() {
  if (digitalRead(FillPin) == LOW) {
    return FILL;
  } else if (digitalRead(DumpPin) == LOW) { 
    return DUMP;
  } else if (digitalRead(RaisePin) == LOW) {
    return RAISE;
  } else if (digitalRead(LowerPin) == LOW) {
    return LOWER;
  }

  return NONE;
}

State nextState(Button currentButton) {
  if (currentState == NOTHING) {
    switch (currentButton) {
      case NONE:
        return NOTHING;
      case FILL:
        return FILLING;
      case DUMP:
        return DUMPING;
      case RAISE:
        return RAISING;
      case LOWER:
        return LOWERING;
    }
  } else if (currentState == FILLING && previousButton == NONE && currentButton == FILL) {
    return FILLING_CANCEL;
  } else if (currentState == DUMPING && previousButton == NONE && currentButton == DUMP) {
    return DUMPING_CANCEL;
  } else if (currentState == RAISING && currentButton == NONE) {
    return RAISING_CANCEL;
  } else if (currentState == LOWERING && currentButton == NONE) {
    return LOWERING_CANCEL;
  }
 
  return currentState;
}

float getCurrentPressure() {
  total = total - rawPressureVal[readIndex];// subtract the last reading:
  int rawPressureValue =  analogRead(pressurePin);  // read from the sensor:
  rawPressureVal[readIndex] = rawPressureValue;
  total = total + rawPressureVal[readIndex];  // add the reading to the total:
  readIndex = readIndex + 1;  // advance to the next position in the array:
  if (readIndex >= numReadings) {     // if we're at the end of the array...
    readIndex = 0;   // ...wrap around to the beginning:
  }
  // calculate the average:
  avgPressureValue = total / numReadings;
  
  // send it to the computer as ASCII digits
  Serial.println(rawPressureValue);
  Serial.println(avgPressureValue);

  float voltage = avgPressureValue * 5.0 / 1024;
  Serial.print(voltage, 4); //prints value from previous line to serial
  Serial.println("V"); //prints label to serial
  
  float pressureValue = pressuretransducermaxPSI*(voltage - voltageZero)/(voltageMax-voltageZero);
  Serial.print(pressureValue, 2); //prints value from previous line to serial
  Serial.println("psi"); //prints label to serial

  return pressureValue;
}

void setup() {
  // Set the display brightness (0-7):
  display.setBrightness(0x0e);
  display.clear();
  Serial.begin(9600);
  pinMode(PistonPin, INPUT_PULLUP);
  pinMode(FillPin, INPUT_PULLUP);
  pinMode(DumpPin, INPUT_PULLUP);
  pinMode(RaisePin, INPUT_PULLUP);
  pinMode(LowerPin, INPUT_PULLUP);
  pinMode(RelayPinA, OUTPUT);
  pinMode(RelayPinB, OUTPUT);
  pinMode(RelayPinC, OUTPUT);
  pinMode(RelayPinD, OUTPUT);
  delay(2000);
  digitalWrite(RelayPinA, HIGH);
  digitalWrite(RelayPinB, HIGH);
  digitalWrite(RelayPinC, HIGH);
  digitalWrite(RelayPinD, HIGH);
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    rawPressureVal[thisReading] = 0;
  }
}

void loop() {

  int pistonVal = digitalRead(PistonPin);   // read the Piston pin
  if (pistonVal == LOW) {
    Serial.println("Piston A");
    digitalWrite(RelayPinC, LOW);
  }
    else if (pistonVal == HIGH) {
    Serial.println("Piston B");
    digitalWrite(RelayPinC, HIGH);
  }
  
  const Button currentButton = getButton();
  State state = nextState(currentButton);

  float pressureValue = getCurrentPressure();
  
  // Show the Pressure on the display
  display.showNumberDecEx(pressureValue*10, 0b11100000, false, 3, 0);

  if (state == FILLING) {
    Serial.println("filling");
    if (pressureValue < fillMax) {
      digitalWrite(RelayPinA, LOW);
      display.clear();
      delay(50);
      const uint8_t fill[] = {
       SEG_A | SEG_F | SEG_G | SEG_E,                   // F
       SEG_E | SEG_F,                                   // I
       SEG_F | SEG_E | SEG_D,                           // L
       SEG_D | SEG_E | SEG_F                            // L
       };  
       display.setSegments(fill, 4, 4);
      delay(500);
      display.clear();
      delay(50);
      display.showNumberDecEx(pressureValue*10, 0b11100000, false, 3, 0);
      delay(500);
    } else {
      Serial.println("done filling");
      digitalWrite(RelayPinA, HIGH);
      state = NOTHING;
    }  
  } else if (state == FILLING_CANCEL) {
    Serial.println("cancel filling");
    digitalWrite(RelayPinA, HIGH);
    state = NOTHING;
  }
  else if (state == DUMPING) {
    Serial.println("dumping");
    if (pressureValue > 0.5) {
      digitalWrite(RelayPinB, LOW);
      display.clear();
      delay(50);
      const uint8_t dump[] = {
       SEG_A | SEG_F | SEG_E | SEG_C | SEG_B | SEG_D,            // D
       SEG_E | SEG_F | SEG_D | SEG_C | SEG_B,                    // U
       SEG_A | SEG_F | SEG_E | SEG_C | SEG_B,                    // N
       SEG_A | SEG_F | SEG_G | SEG_E | SEG_B | SEG_G             // P
       };  
      display.setSegments(dump, 4, 4);
      delay(500);
      display.clear();
      delay(500);
    } else {
      Serial.println("done dumping");
      digitalWrite(RelayPinB, HIGH);
      state = NOTHING;
    }
  }
  else if (state == DUMPING_CANCEL) {
    Serial.println("cancel dumping");
    digitalWrite(RelayPinB, HIGH);
    state = NOTHING;
  }
  else if (state == RAISING) {
    Serial.println("raising");
    digitalWrite(RelayPinA, LOW);
  }  else if (state == RAISING_CANCEL) {
    Serial.println("cancel rasing");
    digitalWrite(RelayPinA, HIGH);
    fillMax = pressureValue;
    state = NOTHING;
  }
  else if (state == LOWERING) {
    Serial.println("lowering");
    digitalWrite(RelayPinB, LOW);
  } else if (state == LOWERING_CANCEL) {
    Serial.println("cancel lowering");
    digitalWrite(RelayPinB, HIGH);
    fillMax = pressureValue;
    state = NOTHING;
  }
  else if (state == NOTHING) {

  }

  currentState = state;
  previousButton = currentButton;
  delay(sensorreadDelay); //delay in milliseconds between read values
}


//void loop2() 
//{
//
//  display.setBrightness(0x0e);
//
//
//
//
//
//     //Open relay until set pressure
//    if (fillVal == LOW){
//      do {
//       Serial.println("filling");
//       Serial.println(avgPressureValue);
//       digitalWrite(RelayPinA, LOW);
//       display.clear();
//       delay(50);
//       const uint8_t fill[] = {
//        SEG_A | SEG_F | SEG_G | SEG_E,                   // F
//        SEG_E | SEG_F,                                   // I
//        SEG_F | SEG_E | SEG_D,                           // L
//        SEG_D | SEG_E | SEG_F                            // L
//        };  
//        display.setSegments(fill, 4, 4);
//       delay(500);
//       display.clear();
//       delay(50);
//       display.showNumberDecEx(pressureValue, 0b11100000, false, 2, 0);
//       delay(500);
//        }
//        while (pressureValue != fillMax && FillPin == HIGH);
//    }
//            else if (RelayPinA == LOW && fillVal == LOW){
//            Serial.println("fillCancel");
//            digitalWrite(RelayPinA, HIGH); 
//               }               
//                 else (fillVal == HIGH && pressureValue < fillMax);{
//                 Serial.println("fillReady");
//                 digitalWrite(RelayPinA, HIGH);
//                   }
//
//  // Open Relay until empty
//   if (dumpVal == LOW && pressureValue >= 0) {
//     do {
//       Serial.println("dumping");
//       digitalWrite(RelayPinB, LOW);
//       display.clear();
//       delay(50);
//       const uint8_t dump[] = {
//        SEG_A | SEG_F | SEG_E | SEG_C | SEG_B | SEG_D,            // D
//        SEG_E | SEG_F | SEG_D | SEG_C | SEG_B,                    // U
//        SEG_A | SEG_F | SEG_E | SEG_C | SEG_B,                    // N
//        SEG_A | SEG_F | SEG_G | SEG_E | SEG_B | SEG_G             // P
//        };  
//       display.setSegments(dump, 4, 4);
//       delay(500);
//       display.clear();
//       delay(50);
//       display.showNumberDecEx(pressureValue, 0b11100000, false, 2, 0);
//       delay(500);
//       }while (pressureValue > 0); 
//    }
//    else if (dumpVal == HIGH && pressureValue >= 0) {
//    Serial.println("dumpReady");
//    digitalWrite(RelayPinB, HIGH);    
//    }
//    
//  int raiseVal = digitalRead(RaisePin);   // read the Raise pin
//  if (raiseVal == LOW) {
//    Serial.println("RaiseLow");
//    digitalWrite(RelayPinA, LOW);
//  } 
//    else if (raiseVal == HIGH) {
//    Serial.println("RaiseHigh");
//    digitalWrite(RelayPinA, HIGH);
//  }
//  
//  int lowerVal = digitalRead(LowerPin);   // read the Lower pin
//  if (lowerVal == LOW) {
//    Serial.println("LowerLow");
//    digitalWrite(RelayPinB, LOW);
//  }
//    else if (lowerVal == HIGH) {
//    Serial.println("LowerHigh");
//    digitalWrite(RelayPinB, HIGH);
//  }
//  
//}
