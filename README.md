# Escucha EDDN de Caballeros de Sion en ESP32-S3

Reserva del escucha de la Orange Pi: una **ESP32-S3 N16R8** escucha EDDN en directo y mantiene
los datos de la facción **Caballeros de Sion (CDSN)** con el mismo formato que `faccion.json`
(el que leen el panel de 10" y el radar).

- Al arrancar baja `faccion.json` de [cdsn-datos](https://github.com/ortemedder/cdsn-datos) como punto de partida.
- Escucha `tcp://eddn.edcd.io:9500` (ZeroMQ) y actualiza influencia, estados, conflictos, facciones,
  «desde ayer», histórico y visitas de cada sistema.
- **No** sube a GitHub ni avisa a Discord (eso lo hace la Orange Pi).

## Web
- `http://IP:8090/` página detallada: todos los sistemas, ordenar/filtrar, y al tocar uno su detalle
  (facciones, tick, visitas, Powerplay, zonas de conflicto, portanaves, gráfica de influencia).
- `http://IP:8090/faccion.json` los datos · `http://IP:8090/estado.json` estado del escucha.

## LED RGB
| Color | Significado |
|---|---|
| verde respirando | todo bien |
| amarillo respirando | avisos (bajada, expansión pendiente) |
| rojo respirando | alerta grave (guerra, elecciones, retirada, ventaja baja) |
| rosa respirando | tick en menos de 30 min |
| morado, 3 destellos | alerta nueva |
| destello blanco / cian | dato de un sistema tuyo / sistema actualizado |
| rojo rápido | sin WiFi o sin EDDN |

## Montaje
1. Copia `escucha_s3/mi_wifi.h.ejemplo` como `escucha_s3/mi_wifi.h` y pon tu WiFi.
2. Arduino IDE, paquete **esp32 2.0.17**, librería **ArduinoJson 7**.
3. Herramientas: **ESP32S3 Dev Module** · Flash **16MB** · PSRAM **OPI PSRAM** ·
   Partition **16M Flash (3MB APP/9.9MB FATFS)** (lo pone solo `sketch.yaml` en IDE recientes).
4. Subir y abrir el Monitor Serie a 115200.

## Archivos
`escucha_s3.ino` arranque y web · `zmtp.h` conexión a EDDN · `faccion.h` datos · `web.h` página ·
`led.h` LED · `inflar.h/.c` descompresor zlib (tinfl de [miniz](https://github.com/richgel999/miniz), licencia MIT).

## Versiones
- s3-v1.0: escucha EDDN, datos de partida de GitHub, web básica.
- s3-v1.0b/c: descompresor incluido; descarga de partida robusta con reintento.
- s3-v1.1: web detallada con todos los datos.
- s3-v1.2: LED de estado de la facción.

Creado por CMDR ORTEM ED DER (ortemedder@gmail.com) con Claude.
