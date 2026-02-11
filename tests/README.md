# MarcSLM Unit Tests

This directory contains Google Test-based unit tests for all modules.

## Test Organization

Tests are organized by module:
- `test_types.cpp` - Core primitive types
- `test_geometry.cpp` - Geometry conversion and slicing
- `test_thermal.cpp` - BuildStyle classification
- `test_pathplanning.cpp` - Scan vector generation

## Running Tests

```powershell
# Using CTest
ctest --preset windows-x64-release --output-on-failure

# Directly run test executable
.\out\build\x64-Release\bin\MarcSLM_Tests.exe
```

## Test Coverage Goals

- Unit tests for all public APIs
- Edge case validation (empty slices, degenerate geometry)
- Performance benchmarks for critical paths
- Integration tests for end-to-end workflows
