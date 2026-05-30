/**
 * @file control.c
 * @brief Control PWM proporcional de la bomba y buzzer de alarma.
 *
 * ── Lógica de control (sin histéresis) ───────────────────────────────
 *
 *  Cada llamada a control_update():
 *
 *  1. Si no estamos en STATE_RUNNING → duty=0, salir.
 *
 *  2. Verificar límite inferior de seguridad:
 *       Si pressure < THERAPY_MIN_MMHG → emergency_stop(), error, salir.
 *
 *  3. Calcular error:
 *       error = pressure_mmhg - target_mmhg
 *       (positivo cuando falta vacío, negativo cuando hay exceso)
 *
 *  4. Calcular duty proporcional:
 *       duty_raw = PWM_KP * error
 *
 *       Si error > 0 (falta vacío):
 *         duty = clamp(duty_raw, PUMP_DUTY_MIN_PCT, PUMP_DUTY_MAX_PCT)
 *
 *       Si error <= 0 (vacío suficiente o excesivo):
 *         duty = PUMP_DUTY_MIN_PCT
 *         → La bomba nunca se apaga durante terapia.
 *           El mínimo compensa el leak permanente continuamente.
 *
 *  5. Aplicar slew rate al duty resultante.
 *
 *  6. Escribir duty al LEDC.
 *
 *  7. Detección de fuga adicional.
 *
 * ── Nota sobre signos ─────────────────────────────────────────────────
 *   Presión de vacío es NEGATIVA. Ejemplo con target = -100 mmHg:
 *
 *   pressure = -50  → error = -50-(-100) =  50 → duty = KP*50 = 100%
 *   pressure = -90  → error = -90-(-100) =  10 → duty = KP*10 =  20% → mín→30%
 *   pressure = -100 → error =   0               → duty = PUMP_DUTY_MIN = 30%
 *   pressure = -105 → error = -5  (exceso)      → duty = PUMP_DUTY_MIN = 30%
 */

#include "control.h"
#include "state_machine.h"
#include "logger.h"
#include "app_config.h"
#include "sensor.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "CONTROL";

// ─────────────────────────────────────────────────────────────
// CONSTANTES INTERNAS
// ─────────────────────────────────────────────────────────────

/* Máximos pasos LEDC para resolución de 10 bits: 2^10 - 1 = 1023 */
#define PUMP_LEDC_MAX_DUTY   ((1 << 10) - 1)

/* Convierte porcentaje (0.0–100.0) a pasos LEDC de 10 bits */
#define PCT_TO_DUTY(p)  ((uint32_t)((p) / 100.0f * PUMP_LEDC_MAX_DUTY))

// ─────────────────────────────────────────────────────────────
// ESTADO INTERNO
// ─────────────────────────────────────────────────────────────
static float    s_current_duty_pct = 0.0f;
static bool     s_target_reached   = false;
static uint32_t s_pump_start_tick  = 0;

// ─────────────────────────────────────────────────────────────
// HELPERS PRIVADOS — PWM
// ─────────────────────────────────────────────────────────────

static void _pump_set_duty(float duty_pct)
{
    /*
     * duty_pct aquí representa el duty real que queremos en la bomba:
     *   100% = potencia máxima
     *    80% = potencia mínima
     *     0% = OFF
     *
     * El LEDC debe invertir esa señal para el MOSFET/driver, por eso
     * el valor enviado al hardware se calcula como 100 - duty_pct.
     */
    if (duty_pct < 0.0f)             duty_pct = 0.0f;
    if (duty_pct > PUMP_DUTY_MAX_PCT) duty_pct = PUMP_DUTY_MAX_PCT;

    float output_pct = 100.0f - duty_pct;
    if (output_pct < 0.0f)   output_pct = 0.0f;
    if (output_pct > 100.0f) output_pct = 100.0f;

    uint32_t steps = PCT_TO_DUTY(output_pct);

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, PUMP_LEDC_CHANNEL, steps);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, PUMP_LEDC_CHANNEL);

    s_current_duty_pct = duty_pct;
}

/**
 * Slew rate: limita el cambio de duty a PUMP_DUTY_SLEW_PCT por ciclo.
 * Evita arranques bruscos a plena potencia y caídas instantáneas.
 */
static float _apply_slew(float current, float target)
{
    float delta = target - current;
    if (delta >  PUMP_DUTY_SLEW_PCT) return current + PUMP_DUTY_SLEW_PCT;
    if (delta < -PUMP_DUTY_SLEW_PCT) return current - PUMP_DUTY_SLEW_PCT;
    return target;
}

// ─────────────────────────────────────────────────────────────
// HELPER PRIVADO — DETECCIÓN DE FUGAS
// ─────────────────────────────────────────────────────────────

static void _check_leak(float pressure_mmhg, int target_mmhg)
{
    /* Verificar si la presión alcanzó el target alguna vez */
    if (pressure_mmhg <= (float)target_mmhg) {
        s_target_reached = true;
    }

    /* Si ya lo alcanzó, el leak permanente es conocido — no es fuga nueva */
    if (s_target_reached) return;

    uint32_t elapsed_ms =
        (xTaskGetTickCount() - s_pump_start_tick) * portTICK_PERIOD_MS;

    if (elapsed_ms > LEAK_TIMEOUT_MS) {
        ESP_LOGE(TAG, "FUGA: bomba activa %lu ms sin alcanzar target",
                 (unsigned long)elapsed_ms);
        logger_add("FUGA: bomba sin alcanzar target");
        emergency_stop();
        sm_set_error(ERR_LEAK_DETECTED);
    }
}

// ─────────────────────────────────────────────────────────────
// API PÚBLICA
// ─────────────────────────────────────────────────────────────

void control_init(void)
{
    /* ── Bomba: LEDC 1 kHz, 10 bits ── */
    ledc_timer_config_t pump_timer = {
        .speed_mode      = LEDC_HIGH_SPEED_MODE,
        .timer_num       = PUMP_LEDC_TIMER,
        .duty_resolution = PUMP_LEDC_RESOLUTION,
        .freq_hz         = PUMP_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&pump_timer);

    ledc_channel_config_t pump_ch = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = PUMP_LEDC_CHANNEL,
        .timer_sel  = PUMP_LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = PIN_PUMP,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&pump_ch);

    /* ── Buzzer: GPIO simple de salida ── */
    gpio_config_t buzzer_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&buzzer_cfg);
    gpio_set_level(PIN_BUZZER, 0);

    s_current_duty_pct = 0.0f;
    s_target_reached   = false;
    s_pump_start_tick  = 0;

    ESP_LOGI(TAG, "Control iniciado. Bomba=GPIO%d (PWM %dHz), Buzzer=GPIO%d",
             PIN_PUMP, PUMP_LEDC_FREQ_HZ, PIN_BUZZER);
}

void control_update(float pressure_mmhg, int target_mmhg)
{

    /* ── 1. Sin terapia activa: bomba OFF ── */
    if (!sm_is_running()) {
        _pump_set_duty(0.0f);
        s_pump_start_tick = 0;
        return;
    }

    /* ── 2. Límite inferior de seguridad (sobre-vacío) ── */
    if (pressure_mmhg < (float)(THERAPY_MIN_MMHG - SAFETY_MARGIN_MMHG)) {
        ESP_LOGE(TAG, "Sobre-vacío: %.1f mmHg (límite de seguridad: %d mmHg)",
                 pressure_mmhg, THERAPY_MIN_MMHG - SAFETY_MARGIN_MMHG);
        logger_add("ALARMA: presion excede limite inferior");
        emergency_stop();
        sm_set_error(ERR_PRESSURE_LOW);
        return;
    }

    /* Registrar tick de inicio de sesión la primera vez */
    if (s_pump_start_tick == 0) {
        s_pump_start_tick = xTaskGetTickCount();
    }

    /* ── 3. Calcular error ── */
    float error = pressure_mmhg - (float)target_mmhg;
    /*
     * error > 0: falta vacío (pressure menos negativa que target)
     * error = 0: exactamente en target
     * error < 0: exceso de vacío (pressure más negativa que target)
     */

    /* ── 4. Calcular duty proporcional ── */
    float duty_target;

    if (error > 0.0f) {
        /*
         * Falta vacío: duty proporcional al error.
         * Se clampea entre mínimo y máximo.
         */
        duty_target = PWM_KP * error;
        if (duty_target > PUMP_DUTY_MAX_PCT) duty_target = PUMP_DUTY_MAX_PCT;
        if (duty_target < PUMP_DUTY_MIN_PCT) duty_target = PUMP_DUTY_MIN_PCT;
    } else {
        /*
         * Vacío suficiente o en exceso: mantener mínimo.
         * La bomba no se apaga — compensa el leak permanente.
         * Si hay sobre-vacío real, ya fue capturado en el paso 2.
         */
        duty_target = PUMP_DUTY_MIN_PCT;
    }

    /* ── 5. Aplicar slew rate ── */
    float smoothed = _apply_slew(s_current_duty_pct, duty_target);

    /* ── 6. Escribir al hardware ── */
    _pump_set_duty(smoothed);

    ESP_LOGD(TAG, "pressure=%.1f target=%d error=%.1f duty=%.1f%%",
             pressure_mmhg, target_mmhg, error, smoothed);

    /* ── 7. Detección de fugas ── */
    _check_leak(pressure_mmhg, target_mmhg);
}

void emergency_stop(void)
{
    _pump_set_duty(0.0f);
    s_target_reached  = false;
    s_pump_start_tick = 0;
    buzzer_on();
    ESP_LOGW(TAG, "EMERGENCY STOP: bomba OFF (0%%), buzzer ON");
}

void buzzer_on(void)
{
    gpio_set_level(PIN_BUZZER, 1);
}

void buzzer_off(void)
{
    gpio_set_level(PIN_BUZZER, 0);
}

float control_get_duty_pct(void)
{
    return s_current_duty_pct;
}

bool control_pump_is_on(void)
{

    return (s_current_duty_pct >= PUMP_DUTY_MIN_PCT);
}

void control_reset_leak_timer(void)
{
    s_target_reached  = false;
    s_pump_start_tick = xTaskGetTickCount();
}
void control_set_duty_direct(float duty_pct)
{
    _pump_set_duty(duty_pct);
    ESP_LOGI(TAG, "Duty directo: %.1f%%", duty_pct);
}