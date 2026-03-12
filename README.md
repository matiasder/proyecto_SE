# proyecto_SE
## Intro y Alcances

Este proyecto propone el diseño de un sistema embebido para controlar una bomba de presión negativa utilizada en el tratamiento de heridas. La terapia de presión negativa (también conocida como VAC Therapy) consiste en aplicar un vacío controlado sobre una herida para favorecer la cicatrización, eliminar fluidos y mejorar la perfusión del tejido.
El sistema está basado en un microcontrolador que recibe información de un sensor de presión, el cual permite monitorear en tiempo real el nivel de vacío dentro del sistema. A partir de estas mediciones, el microcontrolador controla los actuadores, una bomba de vacío encargada de generar la presión negativa y una válvula solenoide que permite liberar presión cuando sea necesario.
El sistema cuenta con una pantalla para visualizar la presión en tiempo real, botones para que el usuario establezca la presión objetivo, y una interfaz gráfica  para configuración avanzada, visualización de estado y registro de eventos. El microcontrolador ejecuta un algoritmo de control que activa o desactiva la bomba para mantener la presión dentro de un rango seguro y terapéutico.

## Objetivo General:

Diseñar e implementar un prototipo de sistema embebido que controle una bomba de presión negativa, manteniendo automáticamente un nivel de vacío configurable dentro de un rango seguro para el uso terapeutico, con monitoreo en tiempo real y registro de eventos.

## objetivos especificos:

1. Adquirir datos del sensor de presión con detección de errores por timeout y valores fuera de rango.
   
2. Desarrollar un algoritmo de control que regule la bomba de vacío y válvula solenoide para mantener la presión en un rango seguro

3. Diseñar una GUI en PC para configurar parámetros, visualizar estado del sistema y consultar historial de eventos.
   
4. Implementar y documentar  los protocolos de comunicación.
   
5. Manejar condiciones de error críticas garantizando respuesta segura ante fallas del sistema.

## Roles

### Technical Lead
Matias Berrio

### Firmware Engineer
Simon Duarte

### Hardware Integration Engineer
Juan Jose Loor
