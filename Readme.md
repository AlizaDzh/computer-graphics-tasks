# asr

_asr_ is a simple renderer written in C++ for creative coding and data visualization purposes.

## Prerequisites

Install all the prerequisites first.

- MSVC (with Latest Visual Studio), Clang (with Latest Xcode), GCC (any version with support of C++17)
- CMake (version `3.17.0` or higher)
- Conan (version `1.32.1` or higher)
- GPU drivers (latest version with stable support of OpenGL 2 or ES 2.0)

## Building

Create a `build` directory and set it as a current working directory:

```bash
mkdir build
cd build
```

Install dependencies with the Conan package manager once:

```bash
conan profile new default --detect # ignore error messages about existing profiles
```bash
```bash
conan remote add bincrafters "https://api.bintray.com/conan/bincrafters/public-conan"
conan install .. --build sdl2 # may also need --build=glew
```

Configure the project to generate IDE or Makefiles:

```bash
# On Windows for Visual Studio
cmake .. -G "Visual Studio 16 2019"

# On macOS for Xcode
cmake .. -G "Xcode"

# on macOS or GNU/Linux to generate Makefiles
cmake .. -G "Unix Makefiles"
```

Build the project:

```bash
cmake --build .
```

Run the test program from the `asr/build/bin/` directory:

```bash
cd .. # Ensure that the current working directory is set to the root asr folder.
./build/bin/<name of the graphics test executable>
```

You may have to set the Working Directory (CWD) in your IDE for some test
targets to be able to open image files.
