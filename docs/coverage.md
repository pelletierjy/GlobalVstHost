# Code Coverage Guide

## Overview

GlobalVstHost supports code coverage instrumentation for MSVC builds. Coverage is enabled via the `JYGLOBALVST_ENABLE_COVERAGE` CMake option.

## Enabling Coverage

### Build with Coverage

```bash
cmake -B build -A x64 -DJYGLOBALVST_BUILD_TESTS=ON -DJYGLOBALVST_ENABLE_COVERAGE=ON
cmake --build build --config Release --parallel
```

### Running Tests with Coverage

```bash
ctest --test-dir build -C Release --output-on-failure
```

## CI Integration

Coverage is automatically enabled in the CI pipeline when running PR checks. The coverage data is collected and uploaded as a build artifact.

## Coverage Data Format

- **Windows (MSVC)**: Uses Visual Studio's `/PROFILE` flag for coverage instrumentation
- Coverage data files (`.coverage`) are generated in the build directory
- Data can be viewed using Visual Studio's Code Coverage tool or third-party tools

## Viewing Results

1. Download the `coverage-data` artifact from CI
2. Open `.coverage` files in Visual Studio: Test > Analyze Code Coverage
3. Or convert to cobertura format using ReportGenerator

## Notes

- Coverage instrumentation adds overhead to test execution
- Not recommended for production builds
- Requires Visual Studio 2022 17.9+ for `/PROFILE` support
