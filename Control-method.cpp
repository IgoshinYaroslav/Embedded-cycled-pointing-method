/*
 * Код для гиростабилизированной платформы на Arduino (ATmega)
 * с MPU6500 и сервоприводами MG996R.
 *
 * Режимы работы:
 * - Ручной (стабилизация выключена): управление сервоприводами через WASD.
 * - Автоматический (стабилизация включена): платформа удерживает горизонтальное положение,
 *   ручное управление игнорируется.
 *
 * Команды по Serial (9600 бод):
 *   w, a, s, d  – ручное перемещение (только если стабилизация выключена)
 *   e, E        – включить/выключить стабилизацию
 *   x, X        – установить углы в 90° (нейтраль) и выключить стабилизацию
 *   p, P, ?     – вывести текущие показания датчиков и состояние
 *
 * Сборка: Arduino IDE, плата Arduino Nano/Uno и т.п.
 */

#include <Wire.h>
#include <Servo.h>

// ===================== Конфигурация MPU6500 =====================
#define MPU_ADDR   0x68
#define INT_PIN    2          // пин прерывания D2
#define DT         0.01f      // период обновления 100 Гц

// ===================== ПИН-ы сервоприводов =====================
#define SERVO_X_PIN 8
#define SERVO_Y_PIN 9

Servo servoX;
Servo servoY;

// ===================== ПИД-регулятор =====================
typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float integral;
    float prev_error;
} Servo_PID;

Servo_PID pid_x = {1.3f, 0.2f, 0.12f, 0.0f, 0.0f};
Servo_PID pid_y = {1.3f, 0.2f, 0.12f, 0.0f, 0.0f};

// ===================== Фильтр =====================
const float FILTER_WEIGHT = 0.98f;

// ===================== Глобальные переменные =====================
float angle_roll  = 0.0f;
float angle_pitch = 0.0f;

float gyroX_bias = 0.0f;
float gyroY_bias = 0.0f;

// Флаги прерываний
volatile bool mpu_data_ready = false;
volatile bool cpu_data_calc = false;
volatile bool cpu_data_calc_error = false;

// Ручное управление
float manualAngleX = 90.0f;    // текущий заданный угол по X (крен)
float manualAngleY = 90.0f;    // текущий заданный угол по Y (тангаж)

bool stabilizationOn = false;   // флаг включения стабилизации

// ===================== Обработчик прерывания MPU =====================
void mpu_interrupt_handler() {
    mpu_data_ready = true;
    if (cpu_data_calc == true) cpu_data_calc_error = true;
}

// ===================== Функция ПИД =====================
float compute_pid_correction(Servo_PID *pid, float error) {
    float p_term = pid->Kp * error;

    // Интегральная часть с защитой от насыщения
    pid->integral += error * DT;
    float max_i_contribution = 20.0f;
    if ((pid->integral * pid->Ki) > max_i_contribution)
        pid->integral = max_i_contribution / pid->Ki;
    if ((pid->integral * pid->Ki) < -max_i_contribution)
        pid->integral = -max_i_contribution / pid->Ki;
    float i_term = pid->Ki * pid->integral;

    // Дифференциальная часть
    float derivative = (error - pid->prev_error) / DT;
    pid->prev_error = error;
    float d_term = pid->Kd * derivative;

    return p_term + i_term + d_term;
}

// ===================== Инициализация MPU6500 =====================
void init_MPU6500() {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B); Wire.write(0x00);   // выход из сна
    Wire.endTransmission(true);

    // DLPF (цифровой ФНЧ) – настройка под частоту прерываний
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1A); Wire.write(5);      // 5 => ~92 Гц (рекомендовано для MG996R)
    Wire.endTransmission(true);

    // Делитель частоты (SRD) – 100 Гц
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x19); Wire.write(32);     // 1000/(1+9) ≈ 100 Гц
    Wire.endTransmission(true);

    // Настройка прерывания: автосброс при чтении, длительность 50 мкс
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x37); Wire.write(0x10);
    Wire.endTransmission(true);

    // Активация прерывания Data Ready
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x38); Wire.write(0x01);
    Wire.endTransmission(true);

    // Диапазоны: акселерометр ±2g, гироскоп ±250°/с
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1C); Wire.write(0x00);
    Wire.endTransmission(true);

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1B); Wire.write(0x00);
    Wire.endTransmission(true);
}

// ===================== Калибровка гироскопа =====================
void calibrate_gyro() {
    long sumX = 0, sumY = 0;
    const int samples = 500;
    for (int i = 0; i < samples; i++) {
        Wire.beginTransmission(MPU_ADDR);
        Wire.write(0x43);
        Wire.endTransmission(false);
        Wire.requestFrom(MPU_ADDR, 4, true);
        sumX += (int16_t)(Wire.read() << 8 | Wire.read());
        sumY += (int16_t)(Wire.read() << 8 | Wire.read());
        delay(2);
    }
    gyroX_bias = (float)sumX / samples;
    gyroY_bias = (float)sumY / samples;
}

// ===================== Вывод состояния и данных =====================
void printStatus() {
    Serial.println("===== Статус системы =====");
    Serial.print("Стабилизация: ");
    Serial.println(stabilizationOn ? "ВКЛ" : "ВЫКЛ");
    Serial.print("Угол крена (Roll):  ");
    Serial.println(angle_roll, 2);
    Serial.print("Угол тангажа (Pitch): ");
    Serial.println(angle_pitch, 2);
    Serial.print("Ручной X: ");
    Serial.print(manualAngleX, 1);
    Serial.print("°  Y: ");
    Serial.println(manualAngleY, 1);
    Serial.print("ПИД интеграл X: ");
    Serial.print(pid_x.integral, 4);
    Serial.print("  Y: ");
    Serial.println(pid_y.integral, 4);
    Serial.println("==========================");
}

// ===================== Обработка команд с Serial =====================
void processSerialCommand() {
    if (Serial.available() <= 0) return;

    char cmd = Serial.read();

    // Игнорируем символы перевода строки
    if (cmd == '\n' || cmd == '\r') return;

    switch (cmd) {
        case 'w': case 'W':
            if (!stabilizationOn) {
                manualAngleY = constrain(manualAngleY + 1.0f, 0.0f, 180.0f);
                Serial.print("Y+ -> ");
                Serial.println(manualAngleY, 1);
            } else {
                Serial.println("Стабилизация активна, ручное управление заблокировано.");
            }
            break;

        case 's': case 'S':
            if (!stabilizationOn) {
                manualAngleY = constrain(manualAngleY - 1.0f, 0.0f, 180.0f);
                Serial.print("Y- -> ");
                Serial.println(manualAngleY, 1);
            } else {
                Serial.println("Стабилизация активна, ручное управление заблокировано.");
            }
            break;

        case 'a': case 'A':
            if (!stabilizationOn) {
                manualAngleX = constrain(manualAngleX - 1.0f, 0.0f, 180.0f);
                Serial.print("X- -> ");
                Serial.println(manualAngleX, 1);
            } else {
                Serial.println("Стабилизация активна, ручное управление заблокировано.");
            }
            break;

        case 'd': case 'D':
            if (!stabilizationOn) {
                manualAngleX = constrain(manualAngleX + 1.0f, 0.0f, 180.0f);
                Serial.print("X+ -> ");
                Serial.println(manualAngleX, 1);
            } else {
                Serial.println("Стабилизация активна, ручное управление заблокировано.");
            }
            break;

        case 'e': case 'E':
            stabilizationOn = !stabilizationOn;
            if (stabilizationOn) {
                // Сбрасываем интегральные составляющие ПИД при включении
                pid_x.integral = 0.0f;
                pid_x.prev_error = 0.0f;
                pid_y.integral = 0.0f;
                pid_y.prev_error = 0.0f;
                Serial.println("Стабилизация ВКЛЮЧЕНА.");
            } else {
                Serial.println("Стабилизация ВЫКЛЮЧЕНА.");
            }
            break;

        case 'x': case 'X':
            // Установить нейтраль (90°) и выключить стабилизацию
            manualAngleX = 90.0f;
            manualAngleY = 90.0f;
            if (stabilizationOn) {
                stabilizationOn = false;
                Serial.println("Стабилизация выключена.");
            }
            Serial.println("Углы установлены в 90°.");
            break;

        case 'p': case 'P': case '?':
            printStatus();
            break;

        default:
            // Неизвестная команда – игнорируем
            break;
    }
}

// ===================== setup =====================
void setup() {
    Serial.begin(9600);
    Wire.begin();
    Wire.setClock(400000);

    pinMode(INT_PIN, INPUT);

    servoX.attach(SERVO_X_PIN);
    servoY.attach(SERVO_Y_PIN);

    servoX.write(90);
    servoY.write(90);

    init_MPU6500();
    calibrate_gyro();

    attachInterrupt(digitalPinToInterrupt(INT_PIN), mpu_interrupt_handler, RISING);

    Serial.println("Система готова. Команды: W A S D – ручное управление, E – стабилизация, X – нейтраль, P/? – статус.");
}

// ===================== loop =====================
void loop() {
    // 1. Обработка команд с консоли
    processSerialCommand();

    // 2. Если данные от MPU не готовы – выходим
    if (!mpu_data_ready) {
        return;
    }

    // Захват флага вычислений (для контроля переполнения)
    cpu_data_calc = true;

    // 3. Чтение данных из MPU
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 14, true);

    int16_t raw_ax = (Wire.read() << 8) | Wire.read();
    int16_t raw_ay = (Wire.read() << 8) | Wire.read();
    int16_t raw_az = (Wire.read() << 8) | Wire.read();
    Wire.read(); Wire.read(); // пропускаем температуру
    int16_t raw_gx = (Wire.read() << 8) | Wire.read();
    int16_t raw_gy = (Wire.read() << 8) | Wire.read();

    // Сброс флага готовности
    mpu_data_ready = false;

    // 4. Перевод в физические единицы
    float ax = (float)raw_ax / 16384.0f;
    float ay = (float)raw_ay / 16384.0f;
    float az = (float)raw_az / 16384.0f;

    float gyro_rate_x = ((float)raw_gx - gyroX_bias) / 131.0f;
    float gyro_rate_y = ((float)raw_gy - gyroY_bias) / 131.0f;

    // 5. Углы по акселерометру
    float accel_roll  = atan2(ay, az) * 57.29578f;
    float accel_pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 57.29578f;

    // 6. Комплементарный фильтр
    angle_roll  = FILTER_WEIGHT * (angle_roll  + gyro_rate_x * DT) + (1.0f - FILTER_WEIGHT) * accel_roll;
    angle_pitch = FILTER_WEIGHT * (angle_pitch + gyro_rate_y * DT) + (1.0f - FILTER_WEIGHT) * accel_pitch;

    // 7. Управление сервоприводами
    if (stabilizationOn) {
        // Режим стабилизации
        float error_x = 0.0f - angle_roll;
        float error_y = 0.0f - angle_pitch;

        float correction_x = compute_pid_correction(&pid_x, error_x);
        float correction_y = compute_pid_correction(&pid_y, error_y);

        float target_x = 90.0f - correction_x;
        float target_y = 90.0f + correction_y;

        target_x = constrain(target_x, 0.0f, 180.0f);
        target_y = constrain(target_y, 0.0f, 180.0f);

        servoX.write((int)target_x);
        servoY.write((int)target_y);
    } else {
        // Ручной режим
        servoX.write((int)manualAngleX);
        servoY.write((int)manualAngleY);
    }

    cpu_data_calc = false;
}