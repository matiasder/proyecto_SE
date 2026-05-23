# Requerimientos del Sistema

## Requerimientos funcionales

### RF-01: Actuación de bomba de vacío
El sistema debe controlar una bomba de vacío mediante una señal PWM generada por el ESP32 a través de un MOSFET. La activación y desactivación de la bomba debe responder a la lógica de control de presión implementada en el firmware.

**Prueba TC-01:**  
Verificar el funcionamiento del sistema completo observando la activación y desactivación de la bomba en respuesta a las condiciones de control definidas.

---

### RF-02: Control de mecanismo de liberación de presión
El sistema debe controlar un servo motor encargado de accionar un mecanismo de apertura y cierre de un orificio, permitiendo la regulación de la presión interna del sistema.

**Prueba TC-02:**  
Verificar físicamente el movimiento del servo al ejecutar acciones de apertura y cierre en función del comportamiento del sistema.

---

### RF-03: Comunicación inalámbrica BLE
El sistema debe implementar comunicación inalámbrica mediante BLE utilizando el protocolo Nordic UART Service (NUS) para el intercambio de datos entre el sistema embebido y una interfaz externa.

**Prueba TC-03:**  
Establecer conexión BLE con una aplicación compatible y verificar la transmisión correcta de datos del sistema.

---

### RF-04: Interfaz de usuario del sistema
El sistema debe presentar la información de presión, estado operativo y alarmas del sistema en una interfaz visual integrada.

**Prueba TC-04:**  
Verificar que la interfaz del sistema actualiza la información en tiempo real de forma correcta.

---

### RF-05: Medición de presión mediante sensor BMP180
El sistema debe medir la presión mediante un sensor BMP180 utilizando comunicación I2C, realizando actualizaciones periódicas de la variable de presión para su uso en el control del sistema.

**Prueba TC-05:**  
Verificar que el sistema obtiene lecturas periódicas del sensor y que estas son utilizadas correctamente por el sistema de control.

---

### RF-06: Detección de fallos en el sensor
El sistema debe identificar condiciones de fallo o pérdida de comunicación con el sensor BMP180 y generar una condición de error en el sistema.

**Prueba TC-06:**  
Desconectar el sensor durante la operación y verificar la detección del fallo y su indicación en la interfaz del sistema.

---

### RF-07: Detección de fugas
El sistema debe detectar condiciones de fuga cuando la presión no alcanza el valor objetivo dentro de un tiempo determinado con el sistema de vacío activo, activando una condición de alarma y deteniendo el proceso de vacío.

**Prueba TC-07:**  
Simular una condición de fuga y verificar la activación de la alarma correspondiente y la detención del sistema de vacío.

---

### RF-08: Control de rango de presión terapéutico
El sistema debe mantener la presión dentro de un rango terapéutico definido entre -80 mmHg y -125 mmHg mediante el control coordinado de la bomba de vacío y el mecanismo de liberación.

**Prueba TC-08:**  
Verificar que el sistema mantiene la presión dentro del rango establecido bajo condiciones normales de operación.

---

### RF-09: Alarma por presión fuera de rango
El sistema debe generar una alarma visual cuando la presión medida permanezca fuera del rango terapéutico durante un tiempo prolongado.

**Prueba TC-09:**  
Inducir una condición de presión fuera de rango y verificar la activación de la alarma del sistema.

---

### RF-10: Configuración de parámetros del sistema
El sistema debe permitir la configuración de parámetros operativos como presión objetivo y tiempos de control mediante la interfaz de usuario del sistema.

**Prueba TC-10:**  
Modificar los parámetros del sistema en tiempo de ejecución y verificar que el sistema adopta los nuevos valores en su comportamiento.

---

## Requerimientos no funcionales

### RNF-01: Carcasa funcional
El sistema debe estar integrado en una carcasa que proteja los componentes electrónicos y permita la correcta operación del sistema.

**Prueba TC-11:**  
Verificar la correcta integración física de los componentes dentro de la carcasa.

---

### RNF-02: Sistema de alimentación autónoma
El sistema debe operar con una fuente de alimentación externa de 12V, garantizando el funcionamiento continuo de todos los módulos.

**Prueba TC-12:**  
Verificar el funcionamiento del sistema utilizando únicamente la fuente externa.

---

### RNF-03: Ensamble en tarjeta universal
El sistema debe estar ensamblado en una tarjeta universal y contar con la documentación esquemática correspondiente.

**Prueba TC-13:**  
Verificar físicamente el ensamble y la existencia del esquema eléctrico en el repositorio del proyecto.

---

### RNF-04: Configurabilidad del sistema
El sistema debe permitir la modificación de parámetros de operación sin necesidad de recompilar el firmware, mediante la interfaz de configuración del sistema.

**Prueba TC-14:**  
Modificar parámetros del sistema en tiempo de ejecución y verificar su aplicación en el comportamiento del sistema.
## Plantilla requerimientos
![Foto plantilla](docs/images/plantilla_tc.png)
## Matriz
![Foto matriz](docs/images/mtriz_req.png)









