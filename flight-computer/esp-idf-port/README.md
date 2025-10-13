# ESP IDF port for flight computer code

The goal of this project is to make the flight computer code more extensible and maintainable.

## Development Setup

Open just this folder in VS Code, not the parent repository.

Make sure to install ESP-IDF v5.4.2. To get editor completions, install the ESP-IDF and C++ VS Code extensions (clangd won't work because the gramework uses a GCC toolchain). Then, run "ESP-IDF: Add VS Code configuration folder" from the command palette.

Optionally, you can also run "ESP-IDF: Add Docker Container configuration" which will create a devcontainer config.

### Wokwi Simulator

If you want to run Wokwi Simulator, it's just a VS Code extension and they have a free community license you can use to test and debug a bit (but they don't support most of the I2C sensors we use so you will get errors when our code tries to access them).

If you want to use an actual line-by-line debugger with breakpoints and everything in Wokwi, you will need to create a VS Code launch configuration like this:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Wokwi GDB",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/hodge-idf.elf",
      "cwd": "${workspaceFolder}",
      "MIMode": "gdb",
      // Change to your username / home directory
      "miDebuggerPath": "/Users/lewis/.espressif/tools/xtensa-esp-elf-gdb/14.2_20240403/xtensa-esp-elf-gdb/bin/xtensa-esp32-elf-gdb",
      "miDebuggerServerAddress": "localhost:3333"
    }
  ]
}
```

Then use the "Wokwi: Start Simulator and Wait for Debugger" command.

The files `diagram.json` and `wokwi.toml` are for this simulator.

## Project structure

There's a custom I2C class which wraps the ESP-IDF apis to make them a little higher-level. You can take control of the bus with `I2C::create()`, which will return a shared I2C instance. Then, you can use `I2C::get_device<T>()` to get a device from the I2C bus. Device objects hold strong references to their bus and are guaranteed to be unique to prevent 2 subsystems trying to write to the same device at the same time.

### Errors

Right now C++ thrown exceptions are disabled because of their performance and reliability issues (it can be difficult to reason about whether your function is propagating errors or not). Instead, functions should use the `std::expected` API to explicitly mark themselves as fallible. A `std::expected` value is either a value or an error.

Since there are plenty of cases where you *do* want to allow an exception to pass though, there is a `TRY` macro which can be used to explicitly propagate errors, and an `unwrap` function which can be used to crash on an unrecoverable error. However, you will have to decide what should happen if a function you call errors. The type `Result` is an alias for a `std::expected` with a dynamically-typed error.

### I2C device drivers

I2C device drivers can be implemented by having a static field `default_address` and a constructor that takes an I2CDevice (you will probably want to store it in the driver class for later use). This will allow them to be returned by `I2c::get_device<T>()`. Keep in mind that your driver is guaranteed exclusive access to the device passed in the constructor, so you are free to keep state without anything else messing with your device!
