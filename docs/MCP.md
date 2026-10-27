# SimulIDE MCP server

SimulIDE embeds a [Model Context Protocol](https://modelcontextprotocol.io) server so an
AI assistant (for example Claude Code) can build and run circuits while you watch the
circuit view update live.

- Transport: MCP **Streamable HTTP** (JSON responses, no SSE), protocol version `2025-06-18`
- Endpoint: `http://127.0.0.1:7531/mcp` (bound to localhost only)
- All tool calls run on the GUI thread, so every change is visible immediately and undoable with Ctrl+Z.

## Connecting Claude Code

Start SimulIDE, then:

```sh
claude mcp add --transport http simulide http://127.0.0.1:7531/mcp
```

Ask Claude something like *"Build a 5V source, 220 ohm resistor and red LED circuit in SimulIDE and start it"*.

## Configuration

| How | Effect |
|-----|--------|
| `simulide --mcp-port 8000` (or `--mcp-port=8000`) | Listen on another port |
| `simulide --no-mcp` | Disable the server for this run |
| `mcp/port=8000` in `simulide.ini` | Default port |
| `mcp/enabled=false` in `simulide.ini` | Disable by default |

`simulide.ini` lives in the Qt data location (on Linux usually `~/.local/share/simulide/simulide.ini`).

## Security

The server listens on `127.0.0.1` only. Requests that carry an `Origin` header other than
`localhost` / `127.0.0.1` / `[::1]` are rejected with 403 to block DNS-rebinding attacks from web pages.
There is no authentication: any local process can drive the simulator (and the file tools can
read/write files with your user rights). Use `--no-mcp` if that is a concern.

## Protocol support

`POST /mcp` accepts single JSON-RPC 2.0 messages or batches. Supported methods:
`initialize`, `notifications/initialized` (and any other notification: `202 Accepted`),
`ping`, `tools/list`, `tools/call`. `GET /mcp` returns `405` (no server-initiated stream).

## Tools

| Tool | Arguments | Description |
|------|-----------|-------------|
| `list_component_types` | `category?` | All placeable component types (type, name, category) |
| `get_circuit` | `include_properties?` | Components (id, type, x, y, rotation, properties, pins and what they connect to) and wires |
| `add_component` | `type, x, y, rotation?, properties?` | Place a component; returns its id and pin names |
| `remove_component` | `id` | Remove a component (with its wires) or a single wire |
| `set_property` | `id, name, value` | Set a property; enums accept names (e.g. `Color: "red"`) |
| `connect_pins` | `compA, pinA, compB, pinB` | Draw a wire between two pins (pins hold one wire each; use a `Node` to branch) |
| `move_component` | `id, x, y, rotation?` | Move/rotate a component (positions snap to an 8px grid) |
| `clear_circuit` | | New empty circuit (unsaved changes discarded) |
| `load_circuit` | `path` | Open a `.simu` file |
| `save_circuit` | `path` | Save to a `.simu` file |
| `start_simulation` / `stop_simulation` | | Power the circuit on/off |
| `get_simulation_state` | | running, paused, step, time, rates |
| `get_component_state` | `id` | Pin voltages, voltage across 2-pin parts, properties, MCU firmware |
| `load_firmware` | `mcu_id, hex_path` | Load a firmware file into an AVR/PIC/Arduino component |
| `set_code` | `path, code` | Write a source file and open it in the code editor |
| `compile_code` | | Compile the current editor file; returns the compiler output |
| `screenshot_circuit` | `mode?` (`circuit`/`view`), `max_size?` | PNG of the circuit as MCP image content |
| `set_analog_engine` | `engine?` (`native`/`ngspice`) | Select the analog engine, report ngspice availability (see ANALOG_ENGINE.md) |
| `get_spice_netlist` | | SPICE netlist of the circuit as exported for ngspice |

Tool failures are returned as normal results with `isError: true` and a text explanation.

## Manual test with curl

```sh
curl -s localhost:7531/mcp -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"add_component","arguments":{"type":"Resistor","x":0,"y":0}}}'
```
