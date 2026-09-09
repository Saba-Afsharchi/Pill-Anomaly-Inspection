#include <AccelStepper.h>

#define ENABLE_PIN 8
#define RELAY_PIN  12 // پایه Spindle Enable روی CNC Shield

// X Axis (Motor A)
#define X_STEP_PIN 2
#define X_DIR_PIN  5

// Y Axis (Motor B)
#define Y_STEP_PIN 3
#define Y_DIR_PIN  6

AccelStepper motorX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper motorY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);

const int BASE_STEP_DISTANCE = 100; 

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50); 

  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // فعال‌سازی درایورها

  // تنظیمات پایه رله (مخصوص ماژول‌های Active-Low)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // حالت اولیه: خاموش (ارسال HIGH)

  motorX.setMaxSpeed(1200);
  motorX.setAcceleration(1000);

  motorY.setMaxSpeed(1200);
  motorY.setAcceleration(1000);

  motorX.setCurrentPosition(0);
  motorY.setCurrentPosition(0);

  Serial.println("CoreXY Multi-Step Control Ready:");
  Serial.println("Send 8, 88, 888 ... up to 10 chars for multiplied steps.");
  Serial.println("Send '1' to turn Relay ON, '0' to turn Relay OFF.");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readString();
    input.trim(); 

    if (input.length() > 0) {
      char key = input.charAt(0);
      int count = 0;

      for (size_t i = 0; i < input.length(); i++) {
        if (input.charAt(i) == key) {
          count++;
        }
      }

      if (count > 10) count = 10;

      long totalDistance = (long)BASE_STEP_DISTANCE * count;

      switch (key) {
        case '8': // حرکت به بالا (Y+)
          motorX.move(totalDistance);
          motorY.move(totalDistance);
          break;

        case '2': // حرکت به پایین (Y-)
          motorX.move(-totalDistance);
          motorY.move(-totalDistance);
          break;

        case '6': // حرکت به راست (X+)
          motorX.move(totalDistance);
          motorY.move(-totalDistance);
          break;

        case '4': // حرکت به چپ (X-)
          motorX.move(-totalDistance);
          motorY.move(totalDistance);
          break;

        // کنترل معکوس‌شده برای ماژول Active-Low
        case '1': // روشن کردن رله
          digitalWrite(RELAY_PIN, LOW); // ارسال LOW برای روشن شدن
          Serial.println("Relay activated (ON)");
          break;

        case '0': // خاموش کردن رله
          digitalWrite(RELAY_PIN, HIGH); // ارسال HIGH برای خاموش شدن
          Serial.println("Relay deactivated (OFF)");
          break;
      }
    }
  }

  motorX.run();
  motorY.run();
}