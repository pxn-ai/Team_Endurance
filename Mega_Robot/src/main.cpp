#include <Arduino.h>

// --- PIN DEFINITIONS ---
#define ENA_PIN 4
#define IN1_PIN 22
#define IN2_PIN 23
#define ENB_PIN 5
#define IN3_PIN 24
#define IN4_PIN 25
#define LEFT_ENC_A 18
#define LEFT_ENC_B 19
#define RIGHT_ENC_A 20
#define RIGHT_ENC_B 21

const int frontIR = A0;
const int rightIR = A1;

// --- GLOBALS ---
volatile long leftEncoderTicks = 0;
volatile long rightEncoderTicks = 0;
int baseSpeed = 130;
int minSpeed = 100;
int maxSpeed = 180;

String currentMode = "IDLE";

// --- ENCODER ISRs ---
void leftEncoderISR()
{
  if (digitalRead(LEFT_ENC_B) == HIGH)
    leftEncoderTicks++;
  else
    leftEncoderTicks--;
}
void rightEncoderISR()
{
  if (digitalRead(RIGHT_ENC_B) == HIGH)
    rightEncoderTicks--;
  else
    rightEncoderTicks++;
}

// --- MOTOR DRIVER ---
class MotorDriver
{
public:
  void init()
  {
    pinMode(ENA_PIN, OUTPUT);
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(ENB_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);
    pinMode(IN4_PIN, OUTPUT);
    stop();
  }
  void drive(int leftSpeed, int rightSpeed)
  {
    leftSpeed = constrain(leftSpeed, -255, 255);
    rightSpeed = constrain(rightSpeed, -255, 255);
    digitalWrite(IN1_PIN, leftSpeed >= 0 ? HIGH : LOW);
    digitalWrite(IN2_PIN, leftSpeed >= 0 ? LOW : HIGH);
    analogWrite(ENA_PIN, abs(leftSpeed));
    digitalWrite(IN3_PIN, rightSpeed >= 0 ? HIGH : LOW);
    digitalWrite(IN4_PIN, rightSpeed >= 0 ? LOW : HIGH);
    analogWrite(ENB_PIN, abs(rightSpeed));
  }
  void stop() { drive(0, 0); }
};

// --- SENSORS ---
// Line Sensor (Raykha)
class RaykhaS8
{
private:
  bool detectHigh;

public:
  RaykhaS8(bool d) : detectHigh(d) {}
  void init()
  {
    DDRC = 0x00;
    PORTC = 0xFF;
  }
  uint8_t readRaw()
  {
    uint8_t raw = PINC;
    uint8_t data = 0;
    for (int i = 0; i < 8; i++)
      if (raw & (1 << i))
        data |= (1 << (7 - i));
    return detectHigh ? data : ~data;
  }
  float getError()
  {
    uint8_t raw = readRaw();
    if (raw == 0)
      return 999.0f;
    int active = 0;
    float total = 0;
    float weights[8] = {-3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5};
    for (int i = 0; i < 8; i++)
      if (raw & (1u << i))
      {
        total += weights[i];
        active++;
      }
    if (active >= 6)
      return 888.0f; // Junction
    return total / active;
  }
};

// IR Distance
float getDistance(int pin)
{
  long sum = 0;
  for (int i = 0; i < 5; i++)
  {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  float volts = (sum / 5.0) * (5.0 / 1023.0);
  volts = constrain(volts, 0.15, 3.2);
  float dist = 2.34 / (volts - 0.1);
  return constrain(dist, 2.0, 30.0);
}

// --- PID CONTROLLERS ---
struct MotorSpeeds
{
  int left;
  int right;
};

class RobotPID
{
public:
  RobotPID(float p, float i, float d) : Kp(p), Ki(i), Kd(d), prev(0), integral(0) {}
  void setTunings(float p, float i, float d)
  {
    Kp = p;
    Ki = i;
    Kd = d;
  }
  void reset()
  {
    integral = 0;
    prev = 0;
  }
  MotorSpeeds calculate(float error, int base, int minS, int maxS)
  {
    float P = Kp * error;
    integral += error;
    integral = constrain(integral, -50, 50);
    float D = Kd * (error - prev);
    prev = error;
    int corr = (int)(P + (Ki * integral) + D);
    return {constrain(base + corr, minS, maxS), constrain(base - corr, minS, maxS)};
  }

private:
  float Kp, Ki, Kd, prev, integral;
};

// --- OBJECTS ---
MotorDriver motor;
RaykhaS8 trackSensor(false);
RobotPID linePID(15.0, 0.0, 5.0);
RobotPID wallPID(2.5, 0.01, 1.2);

float wallSetpoint = 8.0;
float lastValidLineError = 0;

void setup()
{
  Serial1.begin(115200);  // To Raspberry Pi
  Serial2.begin(115200); // To ESP32

  motor.init();
  trackSensor.init();

  attachInterrupt(digitalPinToInterrupt(LEFT_ENC_A), leftEncoderISR, RISING);
  attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_A), rightEncoderISR, RISING);
}

void parseCommand(String cmd)
{
  cmd.trim();
  if (cmd.length() == 0)
    return;

  if (cmd.startsWith("STOP") || cmd.equalsIgnoreCase("Stop"))
  {
    currentMode = "IDLE";
    motor.stop();
  }
  else if (cmd.startsWith("MODE "))
  {
    currentMode = cmd.substring(5);
    currentMode.trim();
    linePID.reset();
    wallPID.reset();

    if (currentMode == "LINE_WITH_CUBE_SEARCH_RIGHT")
    {
      Serial2.println("CAM_RIGHT");
    }
    else if (currentMode == "CUBE_ALIGN")
    {
      Serial2.println("CAM_FRONT");
    }
  }
  else if (cmd.startsWith("TURN "))
  {
    // e.g. "TURN LEFT 110 450"
    String args = cmd.substring(5);
    int space1 = args.indexOf(' ');
    int space2 = args.indexOf(' ', space1 + 1);
    String dir = args.substring(0, space1);
    int speed = args.substring(space1 + 1, space2).toInt();
    int duration = args.substring(space2 + 1).toInt();

    motor.drive(dir == "LEFT" ? -speed : speed, dir == "LEFT" ? speed : -speed);
    delay(duration);
    motor.stop();
  }
  else if (cmd.startsWith("MOVE "))
  {
    if (cmd.equals("MOVE BACKWARD"))
    {
      currentMode = "MOVE BACKWARD";
    }
    else
    {
      // e.g. "MOVE 120 120 0"
      String args = cmd.substring(5);
      int space1 = args.indexOf(' ');
      int space2 = args.indexOf(' ', space1 + 1);
      int left = args.substring(0, space1).toInt();
      int right = args.substring(space1 + 1, space2).toInt();
      int duration = args.substring(space2 + 1).toInt();
      currentMode = "MANUAL_MOVE";
      motor.drive(left, right);
      if (duration > 0)
      {
        delay(duration);
        motor.stop();
      }
    }
  }
  else if (cmd.startsWith("ARM "))
  {
    String action = cmd.substring(4);
    Serial2.println(action); // Forward to ESP32: "PICK" or "DROP"

    // Simulating completion to send ready back to Pi
    delay(4000);
    if (action == "PICK")
    {
      Serial1.println("ready");
    }
  }
  else if (currentMode == "CUBE_ALIGN")
  {
    // Assume pure numerical entries are PID errors from Pi
    float cubeError = cmd.toFloat();
    int left = baseSpeed + (cubeError * 0.5); // tuning constant K
    int right = baseSpeed - (cubeError * 0.5);
    left = constrain(left, 0, 255);
    right = constrain(right, 0, 255);
    motor.drive(left, right);
  }
}

void loop()
{
  // Read from Raspberry Pi
  while (Serial1.available())
  {
    String line = Serial1.readStringUntil('\n');
    parseCommand(line);
  }

  // Pass through ESP32 Serial (if needed)
  while (Serial2.available())
  {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    // Forward messages to Pi or use them logically (currently optional)
  }

  float errorLine = trackSensor.getError();
  float distFront = getDistance(frontIR);
  float distRight = getDistance(rightIR);

  if (currentMode == "WALL_FOLLOW")
  {
    float errorWall = wallSetpoint - distRight;
    MotorSpeeds s = wallPID.calculate(errorWall, baseSpeed, 0, 255);
    motor.drive(s.left, s.right);
  }
  else if (currentMode == "WALL_WITH_LINE")
  {
    if (trackSensor.readRaw() != 0)
    {
      motor.stop();
      Serial1.println("Line found");
      currentMode = "IDLE";
    }
    else
    {
      float errorWall = wallSetpoint - distRight;
      MotorSpeeds s = wallPID.calculate(errorWall, baseSpeed, 0, 255);
      motor.drive(s.left, s.right);
    }
  }
  else if (currentMode == "LINE_FOLLOW" || currentMode == "LINE_WITH_CUBE_SEARCH_RIGHT")
  {
    if (errorLine == 888.0f && currentMode == "LINE_FOLLOW")
    {
      motor.stop();
      Serial1.println("Junction found");
      currentMode = "IDLE";
    }
    else if (errorLine < 800.0f)
    {
      lastValidLineError = errorLine;
      MotorSpeeds s = linePID.calculate(errorLine, baseSpeed, minSpeed, maxSpeed);
      motor.drive(s.left, s.right);
    }
    else if (errorLine >= 999.0f)
    {
      if (lastValidLineError > 0)
        motor.drive(130, -130);
      else
        motor.drive(-130, 130);
      linePID.reset();
    }
  }
  else if (currentMode == "LINE_FOLLOW_UNTIL_WALL")
  {
    if (distFront < 20.0)
    { // e.g. 20cm
      motor.stop();
      Serial1.println("Wall found");
      currentMode = "IDLE";
    }
    else
    {
      if (errorLine < 800.0f)
      {
        lastValidLineError = errorLine;
        MotorSpeeds s = linePID.calculate(errorLine, baseSpeed, minSpeed, maxSpeed);
        motor.drive(s.left, s.right);
      }
      else if (errorLine >= 999.0f)
      {
        if (lastValidLineError > 0)
          motor.drive(130, -130);
        else
          motor.drive(-130, 130);
        linePID.reset();
      }
    }
  }
  else if (currentMode == "CUBE_ALIGN")
  {
    if (distFront < 8.0)
    { // Cube is very close
      motor.stop();
      Serial1.println("Cube nearby");
      currentMode = "IDLE"; // Let Python initiate ARM PICK
    }
  }
  else if (currentMode == "MOVE BACKWARD")
  {
    motor.drive(-100, -100);
    if (trackSensor.readRaw() != 0)
    {
      motor.stop();
      Serial1.println("Line found");
      currentMode = "IDLE";
    }
  }

  delay(20);
}