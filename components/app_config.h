/**
 * @file app_config.h
 * @brief Parámetros configurables del sistema VAC Therapy.
 *
 * Definiciones de pines, constantes y parámetros globales del sistema.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// ─────────────────────────────────────────────────────────────
// VERSIÓN
// ─────────────────────────────────────────────────────────────
#define FW_VERSION  "2.2.0"

// ─────────────────────────────────────────────────────────────
// PINES DE HARDWARE
// ─────────────────────────────────────────────────────────────
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

#define PIN_BTN_UP          25
#define PIN_BTN_DOWN        26
#define PIN_BTN_ENTER       27      /* También resetea alarma desde pantalla ERROR */
#define PIN_BTN_BACK        33

#define PIN_PUMP            18      /* GPIO señal PWM a la bomba (via MOSFET) */
#define PIN_BUZZER          19      /* GPIO buzzer — HIGH = suena, LOW = silencio */

// ─────────────────────────────────────────────────────────────
// TERAPIA - RANGOS
// ─────────────────────────────────────────────────────────────

/** Setpoint por defecto al encender [mmHg] */
#define THERAPY_DEFAULT_MMHG        (-100)

/** Vacío mínimo terapéutico (menos negativo) [mmHg] */
#define THERAPY_MAX_MMHG            (-80)

/** Vacío máximo terapéutico (más negativo) [mmHg] */
#define THERAPY_MIN_MMHG            (-125)

/**
 * Margen de seguridad bajo el límite terapéutico [mmHg].
 *
 * El error de sobre-vacío se dispara cuando:
 *   pressure < THERAPY_MIN_MMHG - SAFETY_MARGIN_MMHG
 *   → pressure < -135 mmHg
 *
 * Esto permite configurar el target en -125 mmHg sin que el ruido
 * normal del BMP180 (±1-2 mmHg) dispare falsas alarmas de sobre-vacío.
 */
#define SAFETY_MARGIN_MMHG          10

// ─────────────────────────────────────────────────────────────
// CONTROL PWM DE LA BOMBA
// ─────────────────────────────────────────────────────────────

/**
 * Periférico LEDC del ESP32 para PWM de la bomba.
 * Frecuencia 1 kHz: adecuada para MOSFET con motor DC.
 * Resolución 10 bits: 0–1023 pasos (suficiente para este control).
 */
#define PUMP_LEDC_CHANNEL       LEDC_CHANNEL_0
#define PUMP_LEDC_TIMER         LEDC_TIMER_0
#define PUMP_LEDC_FREQ_HZ       1000
#define PUMP_LEDC_RESOLUTION    LEDC_TIMER_10_BIT   /* 0–1023 pasos */

/**
 * Duty cycle mínimo cuando la bomba está activa [%].
 *
 * Si el duty calculado cae por debajo de este valor, se aplica
 * este mínimo en lugar de apagar la bomba. Esto compensa el leak
 * permanente — la bomba nunca se apaga completamente durante terapia,
 * solo reduce su potencia al mínimo necesario para mantener el vacío.
 *
 * Ajustar según la bomba física: si no mantiene vacío con 30%,
 * subir a 40–50%.
 */
#define PUMP_DUTY_MIN_PCT       80.0f

/** Duty cycle máximo [%] */
#define PUMP_DUTY_MAX_PCT       100.0f

/**
 * Ganancia proporcional del controlador [% duty / mmHg de error].
 *
 * duty_pct = PWM_KP * error
 *
 * Con KP=2.0 y error=20 mmHg → duty=40%
 * Con KP=2.0 y error=50 mmHg → duty=100% (saturado)
 * Con KP=2.0 y error=0  mmHg → duty=0%  → se aplica PUMP_DUTY_MIN_PCT
 *
 * Subir KP si la bomba tarda en alcanzar el target.
 * Bajar KP si hay oscilaciones alrededor del setpoint.
 */
#define PWM_KP                  2.0f

/**
 * Slew rate: cambio máximo de duty por ciclo de control [%].
 *
 * Evita saltos bruscos de potencia al encender o al cambiar el setpoint.
 * Con PERIOD_CONTROL_MS=100ms y SLEW=5%: cambio máximo = 50%/segundo.
 */
#define PUMP_DUTY_SLEW_PCT      5.0f

// ─────────────────────────────────────────────────────────────
// SENSOR - VALIDACIÓN
// ─────────────────────────────────────────────────────────────

#define SENSOR_MIN_HPA              300.0f
#define SENSOR_MAX_HPA              1100.0f
#define SENSOR_TIMEOUT_MS           2000
#define SENSOR_FAULT_THRESHOLD      5

// ─────────────────────────────────────────────────────────────
// DETECCIÓN DE FUGAS
// ─────────────────────────────────────────────────────────────

/**
 * Tiempo máximo que la bomba puede correr sin alcanzar el target [ms].
 * Con leak permanente conocido, este timeout detecta fugas ADICIONALES
 * (ej. tubería desconectada) más allá del leak normal del sistema.
 */
#define LEAK_TIMEOUT_MS             15000

// ─────────────────────────────────────────────────────────────
// LOGGER
// ─────────────────────────────────────────────────────────────

#define LOGGER_BUFFER_SIZE          32
#define LOGGER_MSG_LEN              64

// ─────────────────────────────────────────────────────────────
// UI - OLED
// ─────────────────────────────────────────────────────────────

#define UI_BLINK_MS                 500

// ─────────────────────────────────────────────────────────────
// TAREAS FREERTOS
// ─────────────────────────────────────────────────────────────
#define TASK_SENSOR_STACK           3072
#define TASK_SENSOR_PRIO            8

#define TASK_CONTROL_STACK          3072
#define TASK_CONTROL_PRIO           7

#define TASK_UI_STACK               3072
#define TASK_UI_PRIO                4

#define TASK_UART_STACK             2048
#define TASK_UART_PRIO              5

#define PERIOD_SENSOR_MS            100
#define PERIOD_CONTROL_MS           100
#define PERIOD_UI_MS                100
#define PERIOD_BLE_MS               500

// ─────────────────────────────────────────────────────────────
// BLE
// ─────────────────────────────────────────────────────────────
#define BLE_DEVICE_NAME             "ESP32-VAC"

#endif /* APP_CONFIG_H */