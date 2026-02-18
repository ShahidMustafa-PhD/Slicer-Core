# ==============================================================================
# MarcSLM - Mesh I/O Setup Script
# ==============================================================================
# Automates the installation of Eigen3 and rebuilds the project.
# ==============================================================================

param(
    [string]$VcpkgPath = "C:\vcpkg",
    [switch]$SkipBuild = $false
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  MarcSLM - Mesh I/O Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if vcpkg exists
if (-not (Test-Path $VcpkgPath)) {
    Write-Host "Error: vcpkg not found at: $VcpkgPath" -ForegroundColor Red
    Write-Host "Please install vcpkg or specify the correct path with -VcpkgPath" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "To install vcpkg:" -ForegroundColor Yellow
    Write-Host "  git clone https://github.com/microsoft/vcpkg.git C:\vcpkg" -ForegroundColor White
    Write-Host "  cd C:\vcpkg" -ForegroundColor White
    Write-Host "  .\bootstrap-vcpkg.bat" -ForegroundColor White
    exit 1
}

Write-Host "Using vcpkg at: $VcpkgPath" -ForegroundColor Green
Write-Host ""

# Step 1: Install Eigen3
Write-Host "Step 1: Installing Eigen3..." -ForegroundColor Yellow
$vcpkgExe = Join-Path $VcpkgPath "vcpkg.exe"

if (-not (Test-Path $vcpkgExe)) {
    Write-Host "Error: vcpkg.exe not found. Run bootstrap-vcpkg.bat first." -ForegroundColor Red
    exit 1
}

& $vcpkgExe install eigen3:x64-windows

if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Failed to install Eigen3" -ForegroundColor Red
    exit 1
}

Write-Host "? Eigen3 installed successfully" -ForegroundColor Green
Write-Host ""

# Step 2: List installed packages (verification)
Write-Host "Step 2: Verifying installation..." -ForegroundColor Yellow
& $vcpkgExe list eigen3

Write-Host ""

# Step 3: Reconfigure CMake
if (-not $SkipBuild) {
    Write-Host "Step 3: Reconfiguring CMake..." -ForegroundColor Yellow
    
    $toolchainFile = Join-Path $VcpkgPath "scripts\buildsystems\vcpkg.cmake"
    
    cmake --preset=default
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Warning: CMake preset failed. Trying manual configuration..." -ForegroundColor Yellow
        cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$toolchainFile
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: CMake configuration failed" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "? CMake configured successfully" -ForegroundColor Green
    Write-Host ""
    
    # Step 4: Build project
    Write-Host "Step 4: Building project..." -ForegroundColor Yellow
    cmake --build build --config Release
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Build failed" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "? Build completed successfully" -ForegroundColor Green
    Write-Host ""
    
    # Step 5: Run tests
    Write-Host "Step 5: Running tests..." -ForegroundColor Yellow
    Push-Location build
    ctest --output-on-failure --build-config Release
    $testResult = $LASTEXITCODE
    Pop-Location
    
    if ($testResult -eq 0) {
        Write-Host "? All tests passed" -ForegroundColor Green
    } else {
        Write-Host "? Some tests failed (exit code: $testResult)" -ForegroundColor Yellow
    }
    Write-Host ""
}

# Summary
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Setup Complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "  1. Test the CLI tool:" -ForegroundColor White
Write-Host "     .\build\bin\io_example.exe <your_mesh.stl>" -ForegroundColor Gray
Write-Host ""
Write-Host "  2. Run unit tests:" -ForegroundColor White
Write-Host "     cd build" -ForegroundColor Gray
Write-Host "     ctest --output-on-failure" -ForegroundColor Gray
Write-Host ""
Write-Host "  3. See documentation:" -ForegroundColor White
Write-Host "     docs\MeshIO_README.md" -ForegroundColor Gray
Write-Host "     docs\MeshIO_QUICKSTART.md" -ForegroundColor Gray
Write-Host ""
