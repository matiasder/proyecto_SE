# Requerimientos del Sistema
 
## Requerimientos Funcionales
 
### RF-01: Control PWM para bomba de vacío
 
El sistema debe controlar la bomba de vacío mediante una señal PWM proporcional al error de presión instantáneo, generada por el ESP32 a través de un MOSFET. El algoritmo debe mantener un duty cycle mínimo configurable para compensar la fuga constante inherente al apósito, limitar los cambios bruscos de potencia mediante slew rate.
 
**Prueba TC-01:**  
Verificar que la bomba modula su potencia mediante control proporcional puro en función del error de presión, manteniendo flujo base mínimo ante fugas, limitando cambios bruscos por slew rate y respetando el corte de seguridad. Se observa la respuesta de la bomba ante distintos errores de presión con la fuga constante activa.
 
---
 
### RF-02: Autocalibración y medición de presión (BMP180)
 
El sistema debe medir la presión diferencial mediante el sensor BMP180 usando comunicación I2C, actualizando la variable `g_pressure_mmhg` con un período de 100 ms. Debe ofrecer una rutina de autocalibración que promedia 64 muestras consecutivas para establecer el cero relativo respecto a la presión atmosférica local, almacenando el offset resultante. Durante la calibración la pantalla debe mostrar el proceso en curso y el control neumático debe permanecer inhibido.
 
**Prueba TC-02:**  
Activar la función de calibración desde el menú principal y verificar que la pantalla OLED transiciona al estado de calibración, que el firmware promedia 64 muestras y que tras completarse el sistema reporta un valor estable de 0 mmHg con el sensor expuesto a presión atmosférica.
 
---
 
### RF-03: Detección y tolerancia a fallos del sensor
 
El sistema debe identificar condiciones de fallo en el sensor BMP180 diferenciando dos tipos: timeout (ausencia de respuesta por más de `SENSOR_TIMEOUT_MS = 2000 ms`) y lecturas fuera de rango físico (hPa < 300 o hPa > 1100). Tras `SENSOR_FAULT_THRESHOLD = 5` lecturas inválidas consecutivas, el sistema debe ejecutar `emergency_stop()`, transicionar a `STATE_ERROR` y reportar el código de error correspondiente (`ERR_SENSOR_TIMEOUT` o `ERR_SENSOR_RANGE`).
 
**Prueba TC-03:**  
Desconectar físicamente el sensor BMP180 durante una sesión de terapia activa y verificar que el firmware detecta el fallo, apaga la bomba de forma inmediata y despliega el error en la interfaz OLED. Verificar que el error persiste hasta reconectar el sensor y resetear la alarma.
 
---
 
### RF-04: Detección automática de fugas masivas
 
El sistema debe detectar condiciones de fuga cuando la bomba permanece activa durante más de `LEAK_TIMEOUT_MS = 15000 ms` sin alcanzar la presión objetivo. Ante esta condición debe activar el código `ERR_LEAK_DETECTED`, detener por completo la bomba y activar la alarma por buzzer.
 
**Prueba TC-04:**  
Simular una fuga masiva retirando el apósito o desconectando la manguera principal durante la terapia activa. Verificar que la presión no alcanza el objetivo, que la bomba opera a máxima potencia durante el período de espera y que tras 15 segundos continuos el sistema detiene la bomba, activa el buzzer y muestra `ERR_LEAK_DETECTED` en pantalla hasta que se ejecute un reset de alarma.
 
---
 
### RF-05: Protección Fail-Safe contra sobre-vacío
 
El sistema debe verificar de forma independiente al controlador PWM que la presión no supere el límite de seguridad `THERAPY_MIN_MMHG = -125 mmHg`. Al detectar una lectura inferior a este umbral, `control_update()` debe llamar inmediatamente a `emergency_stop()` y registrar `ERR_PRESSURE_LOW`, forzando la transición a `STATE_ERROR` con bomba apagada y buzzer activo. Este mecanismo opera antes del cálculo de duty cycle y no puede ser inhibido por el controlador proporcional.
 
**Prueba TC-05:**  
Forzar artificialmente una condición de sobre-vacío superior al umbral de seguridad configurado y verificar que la bomba se apaga de inmediato, la pantalla parpadea mostrando `ERR_PRESSURE_LOW` y el buzzer se activa. Verificar que el sistema queda bloqueado hasta ejecutar un reset de alarma por botón físico o comando BLE.
 
---
 
### RF-06: Interfaz visual y despliegue de telemetría
 
El sistema debe mostrar en la pantalla OLED SSD1306 la información del estado operativo, la presión actual y la presión objetivo, actualizándose en tiempo real con un período de 100 ms. La interfaz debe incluir un menú principal navegable mediante los 4 botones físicos (UP/DOWN/ENTER/BACK), acceso a la pantalla de calibración y a la pantalla de terapia activa donde la presión objetivo pueda ajustarse en tiempo real. El parámetro `g_target_mmhg` debe poder modificarse desde los botones o por comandos BLE sin recompilar el firmware.
 
**Prueba TC-06:**  
Verificar que desde el menú principal se pueden navegar las subpantallas de calibración y terapia. En la pantalla de terapia, verificar que los valores de presión actual y objetivo se actualizan de forma continua y que la presión objetivo puede modularse con los botones UP y DOWN durante la sesión.
 
---
 
### RF-07: Registro de historial de eventos (Logger)
 
El sistema debe mantener un buffer circular de 32 entradas en RAM estática, protegido por mutex FreeRTOS, donde cada entrada almacena un timestamp y un mensaje de hasta 64 caracteres. Al superar la capacidad, las entradas más antiguas deben sobreescribirse de forma ordenada. El buffer debe ser accesible mediante el comando BLE `GET_LOGS`, volcando su contenido por UART en el formato `LOG │ idx │ ts │ msg` terminado en `LOG │ END`.
 
**Prueba TC-07:**  
Generar una secuencia de eventos (inicio de terapia, error simulado, reset) y enviar el comando `GET_LOGS` por BLE. Verificar que el volcado UART muestra las entradas en orden con timestamps válidos, que el índice de cabeza avanza correctamente al superar 32 entradas y que no se presentan corrupciones en los datos.
 
---
 
### RF-08: Comunicación inalámbrica y telemetría BLE
 
El sistema debe implementar comunicación BLE mediante el Nordic UART Service (NUS) usando el stack NimBLE de ESP-IDF, operando en modo solo escritura (el cliente envía comandos, las respuestas se entregan por UART). El sistema debe soportar el conjunto de comandos definido en el protocolo: `START`, `STOP`, `SET:<mmHg>`, `CALIB`, `STATUS`, `GET_STATE`, `GET_ERRORS`, `GET_LOGS`, `RESET_ALARMS`, `TIME:<epoch>`. Los comandos no reconocidos deben responder con `ERR:CMD_UNKNOWN` sin generar un error de sistema.
 
**Prueba TC-08:**  
Conectar una aplicación BLE compatible, verificar el descubrimiento y enlace exitoso. Enviar los comandos `SET:<mmHg>` y `CALIB` y verificar que la máquina de estados interpreta y ejecuta las acciones correspondientes.
 
---
 
### RF-09: Control de estados de la terapia (Inicio / Pausa / Parada)
 
El sistema debe gestionar cuatro estados operativos mediante la máquina de estados global: `IDLE`, `RUNNING`, `CALIBRATION` y `ERROR`. Las transiciones deben poder activarse mediante botones físicos o comandos BLE. Al detener la terapia (`STOP`), la señal PWM debe reducirse a 0% y los contadores temporales deben resetearse, manteniendo visibles el setpoint y los parámetros previos en la interfaz. Al reanudar (`START`), la modulación de la bomba debe reiniciarse de forma gradual hasta alcanzar el rango de presión establecido antes de la interrupción.
 
**Prueba TC-09:**  
Con la terapia activa y presión estabilizada, ejecutar el comando `STOP` (por botón o BLE) y verificar que la bomba se apaga, los contadores se resetean y la interfaz muestra el estado de pausa conservando los parámetros. Reanudar con `START` y verificar que la bomba vuelve a alcanzar el rango de presión establecido.
 
---
 
### RF-10: Priorización de alarmas críticas en pantalla
 
El sistema debe garantizar que cualquier condición de error (`STATE_ERROR`) interrumpe de forma inmediata y jerárquica cualquier pantalla activa en la interfaz OLED. La pantalla de error debe mostrarse de forma parpadeante con el código de error activo y debe bloquear la navegación ordinaria hasta que el operador ejecute un reset de alarma mediante el botón `BTN_ENTER` o el comando BLE `RESET_ALARMS`. Ningún comando de control debe ser procesado mientras el sistema se encuentre en `STATE_ERROR`.
 
**Prueba TC-10:**  
Con el operador navegando en subpantallas de configuración, disparar un error forzado. Verificar que la pantalla actual es reemplazada de forma inmediata por la pantalla de alarma parpadeante con el nombre del error, que se ignoran las entradas de los botones y comandos BLE de control hasta resolver la falla, y que al ejecutar `RESET_ALARMS` el sistema regresa a `STATE_IDLE`.
 
---
 
## Requerimientos No Funcionales
 
### RNF-01: Carcasa funcional y protección
 
El sistema debe estar integrado en una carcasa impresa en 3D (carcasa principal y tapa) que aloje y proteja todos los componentes electrónicos y neumáticos. Los elementos de interfaz (pantalla OLED, botones y puertos de manguera) deben alinearse correctamente con las aperturas de la carcasa para su operación sin obstrucciones. Los circuitos internos deben quedar aislados del exterior ante manipulación ordinaria.
 
**Prueba TC-11:**  
Verificar físicamente que las piezas de la carcasa encajan sin holguras ni deformaciones, que la pantalla OLED, los botones y los puertos de manguera son accesibles y operables desde el exterior, y que el sistema opera correctamente con la carcasa cerrada.
 
---
 
### RNF-02: Alimentación aislada y protección del microcontrolador
 
El sistema debe operar con una fuente de alimentación externa de 12V, utilizando una etapa MOSFET con optoacoplador para el control de la bomba que aísle el ESP32 (3.3V) del circuito de potencia (12V). Esta arquitectura debe garantizar la protección eléctrica del microcontrolador y la operación continua de todos los módulos. (Verificado conjuntamente con TC-01.)
 
---
 
### RNF-03: Configurabilidad en tiempo de ejecución
 
El sistema debe permitir la modificación del parámetro de presión objetivo (`g_target_mmhg`) en tiempo de ejecución sin recompilar el firmware, tanto mediante los botones físicos como mediante el comando BLE `SET:<mmHg>`. (Verificado conjuntamente con TC-06.)
 
---
 
### RNF-04: Ensamble en tarjeta universal
 
El sistema debe estar correctamente ensamblado y soldado en una tarjeta universal, con continuidad eléctrica verificada en los rieles de alimentación y tierra, sin falsos contactos ni puentes de estaño accidentales. 
 
**Prueba TC-12:**  
Verificar físicamente que todos los componentes están soldados de forma firme y limpia, que no existen puentes accidentales entre pistas, y que el sistema opera correctamente bajo las condiciones de vibración generadas por la bomba en funcionamiento.

## Plantilla requerimientos
![Foto plantilla](docs/images/plantilla_tc.png)










