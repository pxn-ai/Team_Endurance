#include <Arduino.h>

// Motor Control Pins
const int enA = 9;
const int in1 = 8;
const int in2 = 7;
const int enB = 3;
const int in3 = 5;
const int in4 = 4;

// PID Variables
float Kp = 0.5;
float Ki = 0.0;
float Kd = 0.1;
int error = 0;
int last_error = 0;
float integral = 0;
const int BASE_SPEED = 100;

// Safety Timeout Variables
unsigned long lastDataTime = 0;
const unsigned long TIMEOUT_MS = 500;
bool isSafeToDrive = false;

String currentMode = "IDLE";

bool isIntegerString(const String &value);
void applyPidFromError(int pidError);
void processIncomingLine(String line);
void processCommand(String line);
void setMotorSigned(int leftSpeed, int rightSpeed);
void executeMove(int leftSpeed, int rightSpeed, int durationMs);
void executeTurn(const String &direction, int speed, int durationMs);
void stopMotors();

void setup()
{
    Serial.begin(115200);
    Serial1.begin(115200);
    Serial1.setTimeout(20);

    pinMode(enA, OUTPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
    pinMode(enB, OUTPUT);
    pinMode(in3, OUTPUT);
    pinMode(in4, OUTPUT);

    stopMotors();
    Serial.println("Mega Ready. Waiting for UART commands...");
}

void loop()
{
    if (Serial1.available() > 0)
    {
        String incomingLine = Serial1.readStringUntil('\n');
        processIncomingLine(incomingLine);
    }

    if (millis() - lastDataTime > TIMEOUT_MS)
    {
        if (isSafeToDrive)
        {
            Serial.println("WARNING: UART timeout! Stopping motors.");
            stopMotors();
            isSafeToDrive = false;
        }
    }
}

void processIncomingLine(String line)
{
    line.trim();
    if (line.length() == 0)
    {
        return;
    }

    if (isIntegerString(line))
    {
        int pidError = line.toInt();
        applyPidFromError(pidError);
        lastDataTime = millis();
        isSafeToDrive = true;
        return;
    }

    processCommand(line);
    lastDataTime = millis();
}

void processCommand(String line)
{
    int firstSpace = line.indexOf(' ');
    String command = firstSpace == -1 ? line : line.substring(0, firstSpace);
    command.toUpperCase();

    String args = firstSpace == -1 ? "" : line.substring(firstSpace + 1);
    args.trim();

    if (command == "STOP")
    {
        stopMotors();
        isSafeToDrive = false;
        Serial.println("ACK STOP");
        return;
    }

    if (command == "MODE")
    {
        currentMode = args;
        currentMode.toUpperCase();
        Serial.print("ACK MODE ");
        Serial.println(currentMode);
        return;
    }

    if (command == "MOVE")
    {
        int leftSpeed = 0;
        int rightSpeed = 0;
        int durationMs = 0;

        int split1 = args.indexOf(' ');
        if (split1 == -1)
        {
            Serial.println("ERR MOVE args");
            return;
        }

        String leftStr = args.substring(0, split1);
        String rest = args.substring(split1 + 1);
        rest.trim();

        int split2 = rest.indexOf(' ');
        if (split2 == -1)
        {
            leftSpeed = leftStr.toInt();
            rightSpeed = rest.toInt();
        }
        else
        {
            String rightStr = rest.substring(0, split2);
            String durationStr = rest.substring(split2 + 1);
            durationStr.trim();
            leftSpeed = leftStr.toInt();
            rightSpeed = rightStr.toInt();
            durationMs = durationStr.toInt();
        }

        executeMove(leftSpeed, rightSpeed, durationMs);
        isSafeToDrive = true;
        Serial.println("ACK MOVE");
        return;
    }

    if (command == "TURN")
    {
        int split = args.indexOf(' ');
        if (split == -1)
        {
            Serial.println("ERR TURN args");
            return;
        }

        String direction = args.substring(0, split);
        direction.toUpperCase();

        String rest = args.substring(split + 1);
        rest.trim();

        int speed = 110;
        int durationMs = 450;

        int split2 = rest.indexOf(' ');
        if (split2 == -1)
        {
            speed = rest.toInt();
        }
        else
        {
            String speedStr = rest.substring(0, split2);
            String durationStr = rest.substring(split2 + 1);
            durationStr.trim();
            speed = speedStr.toInt();
            durationMs = durationStr.toInt();
        }

        executeTurn(direction, speed, durationMs);
        isSafeToDrive = false;
        Serial.println("ACK TURN");
        return;
    }

    if (command == "ARM")
    {
        args.toUpperCase();
        if (args == "PICK")
        {
            Serial.println("ACK ARM PICK");
            return;
        }
        if (args == "DROP")
        {
            Serial.println("ACK ARM DROP");
            return;
        }
        Serial.println("ERR ARM action");
        return;
    }

    Serial.print("ERR Unknown cmd: ");
    Serial.println(command);
}

void applyPidFromError(int pidError)
{
    error = pidError;

    float P = Kp * error;
    integral += error;
    float I = Ki * integral;
    float D = Kd * (error - last_error);

    int motorAdjustment = P + I + D;
    last_error = error;

    int leftSpeed = BASE_SPEED + motorAdjustment;
    int rightSpeed = BASE_SPEED - motorAdjustment;

    setMotorSigned(leftSpeed, rightSpeed);
}

void executeMove(int leftSpeed, int rightSpeed, int durationMs)
{
    setMotorSigned(leftSpeed, rightSpeed);
    if (durationMs > 0)
    {
        delay(durationMs);
        stopMotors();
    }
}

void executeTurn(const String &direction, int speed, int durationMs)
{
    speed = constrain(speed, 0, 255);

    if (direction == "LEFT")
    {
        setMotorSigned(-speed, speed);
    }
    else if (direction == "RIGHT")
    {
        setMotorSigned(speed, -speed);
    }
    else
    {
        Serial.println("ERR TURN direction");
        return;
    }

    if (durationMs > 0)
    {
        delay(durationMs);
        stopMotors();
    }
}

void setMotorSigned(int leftSpeed, int rightSpeed)
{
    leftSpeed = constrain(leftSpeed, -255, 255);
    rightSpeed = constrain(rightSpeed, -255, 255);

    if (leftSpeed >= 0)
    {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    }
    else
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    }

    if (rightSpeed >= 0)
    {
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
    }
    else
    {
        digitalWrite(in3, LOW);
        digitalWrite(in4, HIGH);
    }

    analogWrite(enA, abs(leftSpeed));
    analogWrite(enB, abs(rightSpeed));
}

void stopMotors()
{
    analogWrite(enA, 0);
    analogWrite(enB, 0);

    integral = 0;
    last_error = 0;
}

bool isIntegerString(const String &value)
{
    if (value.length() == 0)
    {
        return false;
    }

    int start = 0;
    if (value[0] == '-' || value[0] == '+')
    {
        if (value.length() == 1)
        {
            return false;
        }
        start = 1;
    }

    for (int i = start; i < value.length(); i++)
    {
        if (!isDigit(value[i]))
        {
            return false;
        }
    }

    return true;
}