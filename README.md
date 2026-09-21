# DataLogger ESP32

Sistema de adquisición, almacenamiento y visualización de datos ambientales desarrollado con ESP32.

El equipo registra:

- Temperatura
- Humedad
- Presión atmosférica

Las mediciones son obtenidas mediante un sensor BME280, fechadas utilizando un RTC DS3231, almacenadas en una tarjeta microSD y mostradas mediante una interfaz web alojada en el propio ESP32.

---

# Quick Start Guide

Esta guía contiene solamente los pasos necesarios para comenzar a utilizar el DataLogger por primera vez.

## 1. Preparar la conexión WiFi

Antes de cargar el programa en el ESP32, configurar localmente los datos de la red:

```cpp
const char* WIFI_SSID = "NOMBRE_DE_TU_WIFI";
const char* WIFI_PASSWORD = "CONTRASENA_DE_TU_WIFI";
```

Las credenciales reales no deben subirse al repositorio.

Se recomienda utilizar una red WiFi de **2.4 GHz**.

También puede utilizarse el hotspot de un celular.

---

## 2. Encender el DataLogger

Conectar el ESP32 a una fuente de alimentación.

Al iniciar, el sistema configurará automáticamente:

- BME280
- RTC DS3231
- Pantalla OLED
- microSD
- WiFi
- Servidor web

---

## 3. Obtener la dirección IP

Abrir el Monitor Serie a:

```text
115200 baudios
```

Buscar la dirección IP asignada al ESP32.

Ejemplo:

```text
WiFi conectado
IP: 192.168.x.x
Servidor HTTP iniciado
```

---

## 4. Abrir el panel

Desde una computadora o celular conectado a la misma red, abrir:

```text
http://IP_DEL_ESP32/
```

Por ejemplo:

```text
http://192.168.45.27/
```

La dirección IP puede cambiar cada vez que el dispositivo se conecta a una red.

---

## 5. Acciones principales

Una vez abierto el panel se pueden realizar las acciones básicas del sistema:

### Ver mediciones actuales

Consultar:

- Temperatura
- Humedad
- Presión atmosférica

### Consultar el historial

Acceder a los registros almacenados en la microSD.

### Exportar mediciones

Descargar los datos registrados para utilizarlos posteriormente.

Con estos pasos el DataLogger ya se encuentra listo para su uso básico.

---

# Manual de usuario

## 1. Descripción general

El DataLogger es un sistema diseñado para registrar variables ambientales y permitir su consulta mediante una interfaz web.

Su funcionamiento general es:

```text
BME280
   │
   ▼
 ESP32
   │
   ├── OLED
   │
   ├── RTC DS3231
   │
   ├── microSD
   │
   └── Servidor web
             │
             ▼
        Navegador web
```

El ESP32 funciona como unidad central del sistema.

Recibe las mediciones del BME280, obtiene la fecha y hora del RTC, almacena la información en la microSD y pone los datos a disposición del frontend mediante una API HTTP.

---

## 2. Componentes del sistema

### ESP32

Es el controlador principal.

Se encarga de:

- Leer los sensores.
- Obtener fecha y hora.
- Guardar información.
- Gestionar la conexión WiFi.
- Ejecutar el servidor web.
- Proporcionar la API.
- Servir el frontend.

### BME280

Sensor encargado de medir:

- Temperatura.
- Humedad.
- Presión atmosférica.

### DS3231

Módulo RTC utilizado para mantener fecha y hora.

Permite asignar una marca temporal a cada medición aunque el ESP32 sea reiniciado.

### OLED SSD1306

Pantalla utilizada para mostrar información directamente desde el equipo.

### microSD

Permite:

- Almacenar el historial.
- Guardar archivos CSV.
- Alojar los archivos del frontend.

---

## 3. Inicio del sistema

Cuando se enciende el ESP32, se inicializan los distintos módulos.

El proceso incluye:

```text
Inicialización del BME280
↓
Inicialización del RTC
↓
Inicialización de la OLED
↓
Inicialización de la microSD
↓
Conexión WiFi
↓
Inicio del servidor HTTP
↓
Inicio de las mediciones
```

Si todos los dispositivos son detectados correctamente, el sistema comienza a registrar información.

---

## 4. Configuración WiFi

El ESP32 necesita estar conectado a una red para que otros dispositivos puedan acceder al panel.

En el firmware se encuentran los parámetros:

```cpp
const char* WIFI_SSID = "NOMBRE_DE_TU_WIFI";
const char* WIFI_PASSWORD = "CONTRASENA_DE_TU_WIFI";
```

Estos valores deben modificarse de forma local antes de cargar el firmware.

Por seguridad, nunca deben publicarse contraseñas reales en GitHub.

---

## 5. Uso mediante hotspot

No es necesario disponer de un router.

Un celular puede utilizarse como punto de acceso:

```text
             CELULAR
             Hotspot
            /       \
           /         \
        ESP32       Notebook
```

Ambos dispositivos deben conectarse al mismo hotspot.

Para mejorar la compatibilidad con el ESP32 se recomienda utilizar:

```text
2.4 GHz
```

Una vez conectado, el teléfono asignará una dirección IP al ESP32.

---

## 6. Acceso al frontend

El ESP32 ejecuta un servidor HTTP.

Para entrar al sistema utilizar:

```text
http://IP_DEL_ESP32/
```

El frontend se encuentra almacenado en la microSD y es enviado al navegador cuando se accede a la dirección del dispositivo.

---

## 7. Panel principal

El panel permite consultar las mediciones actuales.

Se muestran datos como:

- Temperatura.
- Humedad.
- Presión.
- Fecha.
- Hora.
- Estado del sistema.

Los valores provienen del ESP32 y no necesitan cargarse manualmente.

---

## 8. Historial de mediciones

Las mediciones se almacenan periódicamente en la microSD.

Los registros incluyen:

```text
timestamp,temperatura,humedad,presion
```

Cada registro contiene:

- Fecha y hora.
- Temperatura.
- Humedad.
- Presión.

Esto permite consultar información obtenida anteriormente.

---

## 9. Exportación

El sistema permite obtener los datos almacenados para analizarlos fuera del DataLogger.

La API dispone de una ruta de exportación:

```text
/api/export?days=1
```

El número de días puede modificarse según la cantidad de información que se necesite consultar.

---

## 10. API del sistema

El frontend se comunica con el ESP32 mediante una API HTTP.

### Medición actual

```text
/api/actual
```

Ejemplo:

```text
http://IP_DEL_ESP32/api/actual
```

Respuesta:

```json
{
  "created_at": "2026-09-21T14:28:23-03:00",
  "temperatura": 24.07,
  "humedad": 50.53,
  "presion": 100704.52,
  "estado": "Normal",
  "bme280": true,
  "rtc": true,
  "sd": true,
  "wifi": true
}
```

---

### Historial

```text
/api/historial?days=1
```

Devuelve las mediciones almacenadas durante el período solicitado.

---

### Exportación

```text
/api/export?days=1
```

Permite obtener registros almacenados para utilizarlos externamente.

---

## 11. Estado del sistema

La API también informa si los principales componentes fueron inicializados correctamente.

Ejemplo:

```json
{
  "bme280": true,
  "rtc": true,
  "sd": true,
  "wifi": true
}
```

Cada valor representa:

| Campo | Significado |
|---|---|
| `bme280` | Estado del sensor ambiental |
| `rtc` | Estado del reloj |
| `sd` | Estado de la microSD |
| `wifi` | Estado de la conexión WiFi |

Cuando aparece `true`, el componente correspondiente se encuentra disponible.

---

## 12. Almacenamiento en microSD

La tarjeta posee dos funciones principales.

### Datos

Los registros de las mediciones se almacenan dentro de:

```text
/data/
```

### Interfaz web

Los archivos utilizados por el navegador se encuentran dentro de:

```text
/www/
```

Ejemplo:

```text
/
├── data/
│
└── www/
    ├── index.html
    ├── styles.css
    ├── app.js
    └── ...
```

---

## 13. Actualización del frontend

El sistema puede disponer de una página de administración:

```text
http://IP_DEL_ESP32/admin
```

Desde esta sección es posible cargar archivos del frontend en la microSD sin retirar físicamente la tarjeta.

Esto permite actualizar la interfaz incluso si no se dispone de un lector microSD.

---

## 14. Comprobación de conectividad

### Probar comunicación con el ESP32

Desde Windows:

```powershell
ping IP_DEL_ESP32
```

Si aparecen respuestas, existe comunicación entre ambos dispositivos.

---

### Comprobar el servidor HTTP

Desde PowerShell:

```powershell
Test-NetConnection IP_DEL_ESP32 -Port 80
```

Una conexión correcta muestra:

```text
TcpTestSucceeded : True
```

---

### Comprobar la API

También puede utilizarse:

```powershell
curl.exe http://IP_DEL_ESP32/api/actual
```

Si devuelve un JSON con las mediciones, el ESP32, el servidor HTTP y la API se encuentran funcionando.

---

## 15. Resolución de problemas

### El ESP32 no encuentra la red

Comprobar:

- Que la red esté encendida.
- Que el SSID sea correcto.
- Que la contraseña sea correcta.
- Que el hotspot utilice 2.4 GHz.
- Que el ESP32 esté correctamente alimentado.

---

### El ESP32 está conectado pero la página no abre

Primero comprobar:

```powershell
ping IP_DEL_ESP32
```

Después:

```powershell
Test-NetConnection IP_DEL_ESP32 -Port 80
```

Finalmente:

```powershell
curl.exe http://IP_DEL_ESP32/api/actual
```

Esto permite determinar si el problema corresponde a:

- La red.
- El servidor HTTP.
- La API.
- El frontend.

---

### La API funciona pero el frontend no aparece

Si:

```text
/api/actual
```

funciona correctamente pero:

```text
/
```

no muestra el panel, revisar que exista:

```text
/www/index.html
```

dentro de la microSD.

---

### No aparecen nuevas mediciones

Revisar el estado del BME280.

También se puede consultar `/api/actual` para comprobar:

```json
"bme280": true
```

---

### La fecha o la hora son incorrectas

Comprobar el módulo RTC DS3231.

La API debería indicar:

```json
"rtc": true
```

---

### No se guardan registros

Comprobar:

```json
"sd": true
```

Si aparece `false`, revisar la microSD, el módulo y sus conexiones.

---

# FAQ

## ¿Necesito Internet para utilizar el DataLogger?

No.

El ESP32 y el dispositivo utilizado para consultar el panel solamente necesitan estar conectados a la misma red local.

Algunas funciones externas del frontend podrían requerir Internet si utilizan servicios externos.

---

## ¿Puedo utilizar el hotspot de mi celular?

Sí.

El celular puede funcionar como router entre el ESP32 y una notebook.

Se recomienda utilizar un hotspot de 2.4 GHz.

---

## ¿Por qué la dirección IP cambia?

La dirección normalmente es asignada automáticamente mediante DHCP.

Al desconectar y volver a conectar el ESP32, la red puede entregarle otra IP.

---

## ¿Cómo puedo saber la IP actual?

La dirección puede verse desde el Monitor Serie.

Ejemplo:

```text
IP: 192.168.x.x
```

---

## ¿Puedo acceder al sistema desde un celular?

Sí.

El celular debe encontrarse en la misma red que el ESP32.

Después solamente debe abrirse:

```text
http://IP_DEL_ESP32/
```

---

## ¿Cómo sé si los sensores funcionan?

Consultar:

```text
/api/actual
```

El sistema informa el estado de cada módulo.

Por ejemplo:

```json
"bme280": true
```

indica que el sensor fue detectado.

---

## ¿Dónde quedan almacenadas las mediciones?

En la tarjeta microSD.

Los datos son almacenados en archivos CSV dentro del directorio destinado al historial.

---

## ¿Qué ocurre si reinicio el ESP32?

El sistema vuelve a inicializar todos los módulos y comienza nuevamente a registrar mediciones.

Los datos previamente guardados en la microSD permanecen almacenados.

---

## ¿La interfaz muestra datos reales?

Sí.

La versión integrada obtiene sus mediciones desde la API del ESP32.

Puede comprobarse directamente entrando a:

```text
/api/actual
```

---

## ¿Tengo que retirar la microSD para cambiar el frontend?

No necesariamente.

Si está habilitada la página de administración, los archivos pueden cargarse desde:

```text
/admin
```

---

## ¿Por qué funciona `/api/actual` pero no la página principal?

Esto normalmente indica que:

- El ESP32 está conectado.
- El servidor HTTP funciona.
- La API funciona.

En ese caso se deben revisar los archivos almacenados dentro de `/www/`, especialmente:

```text
/www/index.html
```

---

## ¿Se deben subir las credenciales WiFi a GitHub?

No.

Las contraseñas y credenciales privadas deben configurarse únicamente en el equipo local.

El repositorio debe contener solamente valores de ejemplo.

---

# Seguridad

Nunca publicar:

- Contraseñas WiFi.
- Tokens privados.
- Claves secretas.
- Credenciales personales.

Utilizar siempre valores de ejemplo dentro del repositorio público.

---

# Proyecto

**Proyecto Final DataLogger**  
**7.º año**
