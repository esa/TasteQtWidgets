# TASTE Qt widgets

This hosts a collection of Qt based widgets for the [TASTE](https://taste.tools) project by ESA.

## C++

The C++ implementation is in the directory `tastewidgets`. It can be used by including this `CMakeLists.txt` in this directory.

## Python / PySide binding

The Python bindings are in the directory `pytastewidgets`. To build the binding, check out the `README.md` there.

## Version

The version is set in the file `VERSION` only. Cmake does read it there and configure the rest.

## Documentation

Developer documentation can be created, using the tool Doxygen. The HTML based output serves as documentation for C++ and for Python.
