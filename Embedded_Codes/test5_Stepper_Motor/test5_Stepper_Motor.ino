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

const int BASE_STEP_DISTANCE = 10; 

// --- تنظیمات موقعیت دوربین ---
const long CAMERA_UP_STEPS = 37 * BASE_STEP_DISTANCE;   // 370
const long CAMERA_LEFT_STEPS = 70 * BASE_STEP_DISTANCE; // 700
const long CAMERA_MOTOR_X = -CAMERA_UP_STEPS + CAMERA_LEFT_STEPS; // +330
const long CAMERA_MOTOR_Y =  CAMERA_UP_STEPS + CAMERA_LEFT_STEPS; // +1070

// پرچم برای تشخیص زمان رسیدن دقیق به موقعیت دوربین
bool waitingForCamera = false;

// --- پارامترهای شبکه 5x5 قرص‌ها ---
const int Q11_KEY8_COUNT = 146; // تعداد کلید 8 برای قرص 11
const int Q11_KEY4_COUNT = 80;  // تعداد کلید 4 برای قرص 11

const float ROW_STEP_KEY8 = 15.0; // کاهش کلید 8 به ازای هر سطر به پایین
const float COL_STEP_KEY4 = 15.0; // کاهش کلید 4 به ازای هر ستون به راست

// تابع محاسبه و حرکت به خانه قرص (دستور Q)
void moveToPill(int row, int col) {
  float key8_count = Q11_KEY8_COUNT - (row * ROW_STEP_KEY8);
  float key4_count = Q11_KEY4_COUNT - (col * COL_STEP_KEY4);

  long up_steps = (long)(key8_count * BASE_STEP_DISTANCE);
  long left_steps = (long)(key4_count * BASE_STEP_DISTANCE);

  long target_motor_x = -up_steps + left_steps;
  long target_motor_y =  up_steps + left_steps;

  motorX.moveTo(target_motor_x);
  motorY.moveTo(target_motor_y);

  Serial.print("Moving to Pill Q");
  Serial.print(row + 1);
  Serial.print(col + 1);
  Serial.println("...");
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50); 

  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // فعال‌سازی درایورها

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // حالت اولیه: خاموش (Active-Low)

  // تنظیمات سرعت و شتاب نرم
  motorX.setMaxSpeed(600);
  motorX.setAcceleration(300);

  motorY.setMaxSpeed(600);
  motorY.setAcceleration(300);

  motorX.setCurrentPosition(0);
  motorY.setCurrentPosition(0);

  Serial.println("CoreXY Multi-Step Control Ready:");
  Serial.println("  Numpad (1-9) for Jogging");
  Serial.println("  'Z'/'z': Set Current Position as New Home (0,0)");
  Serial.println("  'H'/'h': Go to Home (0,0)");
  Serial.println("  'C'/'c': Go to Camera");
  Serial.println("  'Q11' to 'Q55': Go to specific pill position (e.g., Q11, Q23, Q55)");
  Serial.println("  'R'/'r': Relay ON | 'O'/'o'/'0': Relay OFF");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readString();
    input.trim(); 

    if (input.length() > 0) {
      char key = input.charAt(0);

      // --- پشتیبانی از دستورات Q11 تا Q55 ---
      if (key == 'Q' || key == 'q') {
        if (input.length() >= 3) {
          int rowDigit = input.charAt(1) - '0';
          int colDigit = input.charAt(2) - '0';

          if (rowDigit >= 1 && rowDigit <= 5 && colDigit >= 1 && colDigit <= 5) {
            moveToPill(rowDigit - 1, colDigit - 1);
          } else {
            Serial.println("Error: Pill indices must be between 1 and 5 (e.g., Q11 to Q55).");
          }
        } else {
          Serial.println("Error: Invalid Q command. Use format like Q11, Q34, Q55.");
        }
        return;
      }

      // --- شمارش کلیدهای متوالی جهت‌های دستی ---
      int count = 0;
      for (size_t i = 0; i < input.length(); i++) {
        if (input.charAt(i) == key) {
          count++;
        }
      }

      if (count > 10) count = 10;
      long totalDistance = (long)BASE_STEP_DISTANCE * count;

      switch (key) {
        // --- حرکت‌های اصلی (ارتوگونال) ---
        case '8': // بالا (Y+)
          motorX.move(-totalDistance);
          motorY.move(totalDistance);
          break;

        case '2': // پایین (Y-)
          motorX.move(totalDistance);
          motorY.move(-totalDistance);
          break;

        case '6': // راست (X+)
          motorX.move(-totalDistance);
          motorY.move(-totalDistance);
          break;

        case '4': // چپ (X-)
          motorX.move(totalDistance);
          motorY.move(totalDistance);
          break;

        // --- حرکت‌های مورب ---
        case '9': // بالا-راست
          motorX.move(-totalDistance);
          motorY.move(0);
          break;

        case '7': // بالا-چپ
          motorX.move(0);
          motorY.move(totalDistance);
          break;

        case '3': // پایین-راست
          motorX.move(0);
          motorY.move(-totalDistance);
          break;

        case '1': // پایین-چپ
          motorX.move(totalDistance);
          motorY.move(0);
          break;

        // --- تنظیم صفر جدید ---
        case 'Z':
        case 'z':
          motorX.setCurrentPosition(0);
          motorY.setCurrentPosition(0);
          Serial.println("Current position set as NEW HOME (0,0)!");
          break;

        // --- موقعیت دوربین و بازگشت به خانه ---
        case 'C':
        case 'c':
          motorX.moveTo(CAMERA_MOTOR_X);
          motorY.moveTo(CAMERA_MOTOR_Y);
          waitingForCamera = true; // فعال‌سازی پرچم برای ارسال READY پس از توقف
          Serial.println("Moving to Camera position...");
          break;

        case 'H':
        case 'h':
          motorX.moveTo(0);
          motorY.moveTo(0);
          Serial.println("Returning Home (0,0)...");
          break;

        // --- کنترل رله ---
        case 'R': 
        case 'r':
          digitalWrite(RELAY_PIN, LOW);
          Serial.println("Relay activated (ON)");
          break;

        case 'O':
        case 'o': 
        case '0':
          digitalWrite(RELAY_PIN, HIGH);
          Serial.println("Relay deactivated (OFF)");
          break;
      }
    }
  }

  // به‌روزرسانی حرکت موتورها
  motorX.run();
  motorY.run();

  // بررسی شرط توقف کامل در موقعیت دوربین برای ارسال پیام READY
  if (waitingForCamera && motorX.distanceToGo() == 0 && motorY.distanceToGo() == 0) {
    waitingForCamera = false;
    Serial.println("READY");
  }
}