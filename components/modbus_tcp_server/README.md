# Modbus TCP Server for ESPHome

This external component adds a basic **Modbus TCP slave/server** to ESPHome.

The current implementation is focused on **read-only register export** so ESPHome values can be polled by tools like:

- Node-RED
- Ignition
- PLCs
- SCADA systems
- Modbus TCP test clients

It is intended as an early MVP for exposing ESPHome state over Modbus TCP using concepts similar to ESPHome's existing Modbus RTU support.

## Current feature set

### Supported server behavior
- Modbus TCP server
- Configurable TCP port
- Configurable unit/slave ID
- Single client handling
- MBAP parsing
- Function code support:
  - **FC03** Read Holding Registers
  - **FC04** Read Input Registers

### Supported register types
- `holding`
- `input`

### Supported value types
- `u_word`
- `s_word`
- `u_dword`
- `s_dword`
- `fp32`
- `u_dword_r`
- `s_dword_r`
- `fp32_r`

### Supported ESPHome source entities
- `sensor_id`
- `number_id`

### Supported transforms
- `scale`
- `offset`

## Not supported yet

This is still an MVP. The following are **not implemented yet**:

- register writes
- coil support
- discrete input support
- switch export
- binary sensor export
- FC01 / FC02 / FC05 / FC06 / FC15 / FC16
- multiple simultaneous clients
- first-class constant/register literals
- advanced validation/clamping/rounding rules

## How values are exported

Each Modbus register entry maps one ESPHome entity to one Modbus register address.

The current transform formula is:

```text
exported_value = source_value * scale + offset
```

Where:
- `source_value` is the current state of the ESPHome sensor/number
- `exported_value` is encoded into the requested Modbus register format

For integer types, the transformed value is cast to the target integer type.

For float types (`fp32`, `fp32_r`), the transformed value is encoded as IEEE754 float32 across two registers.

## Signed vs unsigned

Use signed types when values may be negative.

Examples:
- freezer temperature
- vibration delta
- pressure differential
- bidirectional current/power

Use unsigned types when values are always non-negative.

Examples:
- counters
- uptime
- positive-only levels
- positive-only setpoints

### Example
For freezer temperature:

```yaml
value_type: s_word
scale: 10
offset: 0
```

A temperature of `-18.4` becomes:

```text
-18.4 * 10 = -184
```

and is stored as a signed 16-bit register value.

If a Modbus client reads the register as unsigned, it will display the raw two's-complement representation instead of the negative engineering value.

## Register type behavior

The component keeps **holding registers** and **input registers** separate.

That means:
- `register_type: holding` must be read with **FC03**
- `register_type: input` must be read with **FC04**

If a client tries to read an input register using FC03, or a holding register using FC04, the server will respond with an illegal address exception because that address does not exist in that register space.

## Example configuration

````yaml
esphome:
  name: ismfg-esphome-3
  friendly_name: ismfg-esphome-3

external_components:
  - source:
      type: local
      path: /components

esp32:
  board: esp32-c6-devkitc-1
  framework:
    type: esp-idf

logger:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

number:
  - platform: template
    id: counter_value
    name: "Counter Value"
    min_value: 0
    max_value: 1000
    step: 1
    optimistic: true

sensor:
  - platform: template
    id: negative_temp_test
    name: "Negative Temp Test"
    lambda: |-
      return -18.4;
    update_interval: 1s

modbus_tcp_server:
  id: mbsrv
  port: 502
  unit_id: 1

modbus_server_register:
  - modbus_tcp_server_id: mbsrv
    address: 100
    register_type: holding
    value_type: s_word
    sensor_id: negative_temp_test
    scale: 10
    offset: 0

  - modbus_tcp_server_id: mbsrv
    address: 101
    register_type: holding
    value_type: s_word
    number_id: counter_value
    scale: 1
    offset: 0
````

## Example result interpretation

If:
- `negative_temp_test = -18.4`
- `counter_value = 0`

Then reading holding registers `100..101` returns:

```text
100 = -184   (engineering value -18.4 with scale 10)
101 = 0
```

Some Modbus clients display all 16-bit registers as unsigned values.

In that case, `-184` may appear as:

```text
65352
```

To decode signed 16-bit manually:

```text
if raw > 32767:
  signed = raw - 65536
```

So:

```text
65352 - 65536 = -184
```

Then reverse scaling:

```text
engineering_value = (raw_value - offset) / scale
```

Example:

```text
(-184 - 0) / 10 = -18.4
```

## Configuration reference

### `modbus_tcp_server`

| Option | Required | Default | Description |
|---|---|---:|---|
| `id` | yes | - | Component ID |
| `port` | no | `502` | TCP listen port |
| `unit_id` | no | `1` | Modbus unit/slave ID |

### `modbus_server_register`

| Option | Required | Description |
|---|---|---|
| `modbus_tcp_server_id` | yes | Parent Modbus TCP server ID |
| `address` | yes | Register start address |
| `register_type` | yes | `holding` or `input` |
| `value_type` | yes | Register encoding type |
| `sensor_id` | yes* | Source ESPHome sensor |
| `number_id` | yes* | Source ESPHome number |
| `scale` | no | Multiplier, default `1.0` |
| `offset` | no | Additive offset, default `0.0` |

* Exactly one of `sensor_id` or `number_id` must be provided.

## Value type notes

### 16-bit
- `u_word` = unsigned 16-bit
- `s_word` = signed 16-bit

### 32-bit
- `u_dword` = unsigned 32-bit
- `s_dword` = signed 32-bit
- `fp32` = float32

### Reversed word order variants
- `u_dword_r`
- `s_dword_r`
- `fp32_r`

These reversed variants swap the two 16-bit words when encoding 32-bit values.

## Client recommendations

### Node-RED
- Use **Holding Registers** for `register_type: holding`
- Use **Input Registers** for `register_type: input`
- Be aware that signed values may appear unsigned unless explicitly interpreted as signed

### Ignition
- Configure the correct function/register type
- Match the datatype to the exported register:
  - signed 16-bit for `s_word`
  - unsigned 16-bit for `u_word`
  - float for `fp32`
- Apply inverse scaling in tag configuration if needed

## Known limitations

- Current implementation is read-only
- Only one client is handled at a time
- Register reads currently expect the request range to align with defined register starts
- No boolean/coils/discrete-input mapping yet
- No writeback into ESPHome entities yet

## Roadmap ideas

Possible next steps:
- boolean support as `0/1` registers
- writable holding registers
- switch/binary sensor mapping
- coils and discrete inputs
- FC06/FC16 write support
- better clamping and rounding behavior
- more ergonomic entity export options

## Summary

This component already provides a working Modbus TCP register server for ESPHome values.

Today it is best suited for:
- exposing telemetry to Modbus TCP clients
- testing integrations with Node-RED or Ignition
- exporting scaled sensor and number values as standard Modbus registers

It is a solid base for expanding toward a fuller Modbus TCP device model later.
