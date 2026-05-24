# lokomoti/esphome

External ESPHome components maintained in this repository.

## Components

### `adxl345_spi`
Custom ADXL345 SPI component.

### `modbus_tcp_server`
A Modbus TCP slave/server external component for ESPHome.

Current MVP capabilities:
- Modbus TCP server
- FC03 Read Holding Registers
- FC04 Read Input Registers
- export ESPHome `sensor` and `number` values as Modbus registers
- register data types including signed/unsigned 16-bit, 32-bit, and float32 variants
- per-register `scale` and `offset`

### `modbus_server_register`
Register mapping component used with `modbus_tcp_server` to expose ESPHome entities at Modbus register addresses.

## Repository purpose

This repository is used to develop and test custom ESPHome components outside the main ESPHome project.

## Modbus TCP quick start

Use the local components from this repository:

````yaml
external_components:
  - source:
      type: local
      path: /components
````

Then configure the server and exported registers. See `components/modbus_tcp_server/README.md` for detailed documentation and examples.
