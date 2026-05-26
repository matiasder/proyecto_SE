/**
 * @file display.h
 * @brief Interfaz de usuario en pantalla OLED SSD1306.
 *
 * Pantallas disponibles:
 *   - Menú principal
 *   - Terapia en curso (presión actual y setpoint)
 *   - Calibración
 *   - Alarma (sobreescribe todo, parpadea)
 *
 * La pantalla de alarma tiene prioridad sobre cualquier otra.
 * Se llama display_update() periódicamente desde la tarea UI.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>

/** Pantallas posibles del sistema */
typedef enum {
    SCREEN_MENU        = 0,
    SCREEN_THERAPY     = 1,
    SCREEN_CALIBRATION = 2,
    SCREEN_ALARM       = 3,
} screen_t;

/** Inicializa la pantalla OLED y dibuja el menú inicial. */
void display_init(void);

/**
 * Actualiza lo que se muestra en pantalla.
 *
 * Si el sistema está en STATE_ERROR, sobreescribe con pantalla de alarma
 * independientemente de la pantalla activa.
 *
 * @param pressure_mmhg  Presión actual en mmHg.
 * @param target_mmhg    Setpoint actual en mmHg.
 */
void display_update(float pressure_mmhg, int target_mmhg);

/** Fuerza el redibujado completo en el próximo ciclo. */
void display_force_redraw(void);

/** Retorna la pantalla actualmente activa. */
screen_t display_get_screen(void);

/** Cambia la pantalla activa. */
void display_set_screen(screen_t screen);

#endif /* DISPLAY_H */
