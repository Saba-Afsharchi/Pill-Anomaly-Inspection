#include <AccelStepper.h>

#define ENABLE_PIN 8

// X Axis (Motor A)
#define X_STEP_PIN 2
#define X_DIR_PIN  5

// Y Axis (Motor B)
#define Y_STEP_PIN 3
#define Y_DIR_PIN  6

AccelStepper motorX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper motorY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);

// گام پایه برای ۱ بار فشار دادن کلید
const int BASE_STEP_DISTANCE = 100; 

void setup() {
  Serial.begin(9600);
  // تنظیم تایم‌اوت سریال برای دریافت سریع رشته‌های متوالی
  Serial.setTimeout(50); 

  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // فعال‌سازی درایورها

  // تنظیمات سرعت و شتاب
  motorX.setMaxSpeed(1200);
  motorX.setAcceleration(1000);

  motorY.setMaxSpeed(1200);
  motorY.setAcceleration(1000);

  motorX.setCurrentPosition(0);
  motorY.setCurrentPosition(0);

  Serial.println("CoreXY Multi-Step Control Ready:");
  Serial.println("Send 8, 88, 888 ... up to 10 chars for multiplied steps.");
}

void loop() {
  if (Serial.available() > 0) {
    // خواندن تمام کاراکترهای فرستاده شده در یک پکت
    String input = Serial.readString();
    input.trim(); // حذف کاراکترهای اضافی مانند Space یا Enter

    if (input.length() > 0) {
      char key = input.charAt(0); // کاراکتر اصلی (8, 2, 6, 4)
      int count = 0;

      // شمارش تعداد کاراکترهای مشابه و محدود کردن آن حداکثر تا ۱۰
      for (size_t i = 0; i < input.length(); i++) {
        if (input.charAt(i) == key) {
          count++;
        }
      }

      if (count > 10) count = 10; // سقف ۱۰ تکرار

      long totalDistance = (long)BASE_STEP_DISTANCE * count;

      switch (key) {
        case '8': // حرکت به بالا (Y+)
          motorX.move(totalDistance);
          motorY.move(totalDistance);
          Serial.print("Moving UP - Steps: ");
          Serial.println(totalDistance);
          break;

        case '2': // حرکت به پایین (Y-)
          motorX.move(-totalDistance);
          motorY.move(-totalDistance);
          Serial.print("Moving DOWN - Steps: ");
          Serial.println(totalDistance);
          break;

        case '6': // حرکت به راست (X+)
          motorX.move(totalDistance);
          motorY.move(-totalDistance);
          Serial.print("Moving RIGHT - Steps: ");
          Serial.println(totalDistance);
          break;

        case '4': // حرکت به چپ (X-)
          motorX.move(-totalDistance);
          motorY.move(totalDistance);
          Serial.print("Moving LEFT - Steps: ");
          Serial.println(totalDistance);
          break;
      }
    }
  }

  // اجرای همزمان حرکت موتورها
  motorX.run();
  motorY.run();
}