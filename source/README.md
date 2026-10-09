# Block Scheme

A native Windows block-diagram editor written in C++17, managed by CMake.
The GUI uses Win32 and GDI+; no external GUI packages are required.

## Requirements

- Windows 10 version 1703 or newer, or Windows 11.
- Visual Studio 2022 with **Desktop development with C++** and a Windows SDK.
- CMake 3.21 or newer.

## Build and Run

From the workspace root in PowerShell or a developer terminal:

```powershell
cd source
cmake --preset windows-msvc
cmake --build --preset release
ctest --preset release
cmake --install ../build --config Release
..\install\bin\BlockScheme.exe
```

Run preset commands from `source`. Build and install outputs remain in the
workspace's `build` and `install` folders. For debugging, use
`cmake --build --preset debug` and run `../build/Debug/BlockScheme.exe`.
In VS Code CMake Tools, set `cmake.sourceDirectory` to `${workspaceFolder}/source`
to use the same presets.

## Editing

- Click a palette block, then click the canvas to place it; dragging from the palette also works.
- Drag a block to move it. Its connected wires follow automatically.
- Click an output port on the right, then an input port on the left, or drag between them.
- Each input accepts one connection. Outputs support multiple destinations.
- Click a block or wire to select it; press Delete to remove it. Right-click also offers Delete.
- Deleting a block removes its connections. Escape cancels placement or a pending connection.
- Use the mouse wheel to zoom around the cursor and middle-button drag to pan.
- Home or **View > Fit scheme** fits the diagram. Ctrl+Plus and Ctrl+Minus zoom.
- File commands provide New (Ctrl+N), Open (Ctrl+O), Save (Ctrl+S), and Save As (Ctrl+Shift+S).
- Unsaved changes prompt before New, Open, or closing the window.

Open the workspace's `examples/starter.scheme` using **File > Open** for a sample multi-input diagram.

Available blocks are Source, Gain, Add, Multiply, Split, and Sink. These are diagram
elements, not a simulation engine: the application edits topology but does not
evaluate signals or configure numerical parameters. Self-connections are rejected;
connections forming cycles between distinct blocks are allowed.

## Project Layout

Paths below are relative to the workspace root:

- `source/CMakeLists.txt`, `source/CMakePresets.json`: build, test, and install configuration.
- `source/README.md`: build and usage instructions.
- `source/main.cpp`: native window, rendering, hit testing, and editor interactions.
- `source/scheme_model.h`, `source/scheme_model.cpp`: block types, graph rules, and persistence.
- `source/scheme_model_tests.cpp`: focused graph and file round-trip tests.
- `examples/starter.scheme`: example document.
- `build/`: generated build files; `install/`: installed executable.

To add a block type, extend `scheme::blockTypes()` and the matching color table in
`main.cpp`. Input and output labels determine the port count and block height.

## Tests

From `source`, `ctest --preset debug` or `ctest --preset release` runs graph tests
and a GUI smoke test. The latter briefly opens a real window, sends mouse/keyboard
events through its handlers, checks rendering pixels, and writes
`BlockScheme-smoke.png` next to the tested executable. An interactive Windows
desktop is required for the GUI test.

The graph alone can be built on other platforms, from the workspace root:

```sh
cmake -S source -B build-model -DPSL_BUILD_GUI=OFF
cmake --build build-model
ctest --test-dir build-model --output-on-failure
```

Documents use a versioned `.scheme` text format. Loading validates block IDs,
types, port indices, and connection rules before replacing the current document.