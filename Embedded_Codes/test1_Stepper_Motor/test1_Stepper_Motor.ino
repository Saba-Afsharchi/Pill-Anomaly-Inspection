#include <AccelStepper.h>

#define ENABLE_PIN 8

// X Axis
#define X_STEP_PIN 2
#define X_DIR_PIN  5

// Y Axis
#define Y_STEP_PIN 3
#define Y_DIR_PIN  6

AccelStepper motorX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper motorY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);

void setup() {

  pinMode(ENABLE_PIN, OUTPUT);

  // فعال کردن هر دو درایور
  digitalWrite(ENABLE_PIN, LOW);

  // تنظیمات X
  motorX.setMaxSpeed(400);
  motorX.setAcceleration(150);

  // تنظیمات Y
  motorY.setMaxSpeed(400);
  motorY.setAcceleration(150);

  // موقعیت اولیه
  motorX.setCurrentPosition(0);
  motorY.setCurrentPosition(0);

  // حرکت اولیه
  motorX.moveTo(1000);
  motorY.moveTo(1000);
}

void loop() {

  // وقتی هر دو به مقصد رسیدند
  if (motorX.distanceToGo() == 0 &&
      motorY.distanceToGo() == 0) {

    // برگشت
    motorX.moveTo(-motorX.currentPosition());
    motorY.moveTo(-motorY.currentPosition());
  }

  // اجرای همزمان
  motorX.run();
  motorY.run();
}