# fxtui-playground

A playground for experimenting with the [FTXUI](https://github.com/ArthurSonzogni/FTXUI) terminal UI library in C++23.

## Structure

```
fxtui-playground/
├── CMakeLists.txt
└── src/
    ├── hello_world/       # each subdirectory is a separate executable
    │   └── main.cpp
    └── my_experiment/
        └── main.cpp
```

Each subdirectory under `src/` is automatically discovered by CMake and built as a separate executable named after the directory. All `.cpp` files within a subdirectory are compiled together into that executable.

## Dependencies

- CMake 3.22+
- A C++23-capable compiler (GCC 13+, Clang 16+, or MSVC 19.35+)
- Git (for FetchContent to download FTXUI)

FTXUI is fetched automatically via CMake FetchContent — no manual installation needed.

## Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all executables
cmake --build build

# Or build a specific executable (e.g. hello_world)
cmake --build build --target hello_world
```

For a debug build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Run

After building, executables are placed in the `build/` directory:

```bash
./build/hello_world
```

## Adding a New Experiment

1. Create a new subdirectory under `src/`:
   ```bash
   mkdir src/my_experiment
   ```
2. Add one or more `.cpp` files to it:
   ```bash
   # src/my_experiment/main.cpp
   ```
3. Re-run CMake configure and build:
   ```bash
   cmake -B build
   cmake --build build --target my_experiment
   ```

No changes to `CMakeLists.txt` are needed — new directories are auto-discovered.

## FTXUI Libraries

Each executable is linked against all three FTXUI modules:

| Library | Purpose |
|---|---|
| `ftxui::screen` | Screen buffer and rendering |
| `ftxui::dom` | Declarative element tree (layout, text, borders) |
| `ftxui::component` | Interactive components (menus, inputs, checkboxes) |
