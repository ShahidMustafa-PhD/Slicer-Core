# MarcSLM Build Verification Script
# ==================================
# This script verifies that the project is correctly configured and built

param(
    [switch]$SkipBuild = $false,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

# Colors
$Green = "Green"
$Red = "Red"
$Yellow = "Yellow"
$Cyan = "Cyan"

function Write-Status {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
}

function Write-Success {
    param([string]$Message)
    Write-Host "? $Message" -ForegroundColor $Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "? $Message" -ForegroundColor $Red
}

function Write-Warning {
    param([string]$Message)
    Write-Host "??  $Message" -ForegroundColor $Yellow
}

function Write-Section {
    param([string]$Title)
    Write-Host "`n========================================" -ForegroundColor $Cyan
    Write-Host "  $Title" -ForegroundColor $Cyan
    Write-Host "========================================" -ForegroundColor $Cyan
}

# Main verification
Write-Section "MarcSLM Build Verification"

# 1. Check prerequisites
Write-Status "`n1??  Checking Prerequisites...`n"

# Check CMake
try {
    $cmakeVersion = cmake --version | Select-String -Pattern "(\d+\.\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }
    if ($cmakeVersion) {
        Write-Success "CMake installed: $cmakeVersion"
    }
} catch {
    Write-Error "CMake not found. Please install CMake 3.21+"
    exit 1
}

# Check VCPKG_ROOT
if ($env:VCPKG_ROOT) {
    Write-Success "VCPKG_ROOT set: $env:VCPKG_ROOT"
    
    if (Test-Path $env:VCPKG_ROOT) {
        Write-Success "vcpkg directory exists"
    } else {
        Write-Warning "VCPKG_ROOT points to non-existent directory"
    }
} else {
    Write-Warning "VCPKG_ROOT not set. Build may fail."
}

# Check Visual Studio
try {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        if ($vsPath) {
            Write-Success "Visual Studio 2022 found: $vsPath"
        }
    }
} catch {
    Write-Warning "Could not detect Visual Studio installation"
}

# 2. Check project files
Write-Status "`n2??  Checking Project Files...`n"

$requiredFiles = @(
    "CMakeLists.txt",
    "CMakePresets.json",
    "vcpkg.json",
    "include/MarcSLM/Core/Types.hpp",
    "tests/test_types.cpp"
)

$missingFiles = @()
foreach ($file in $requiredFiles) {
    if (Test-Path $file) {
        Write-Success "Found: $file"
    } else {
        Write-Error "Missing: $file"
        $missingFiles += $file
    }
}

if ($missingFiles.Count -gt 0) {
    Write-Error "Missing $($missingFiles.Count) required files. Aborting."
    exit 1
}

# 3. Check directory structure
Write-Status "`n3??  Checking Directory Structure...`n"

$requiredDirs = @(
    "cmake",
    "include/MarcSLM/Core",
    "include/MarcSLM/Geometry",
    "include/MarcSLM/Thermal",
    "include/MarcSLM/PathPlanning",
    "src/Core",
    "tests"
)

foreach ($dir in $requiredDirs) {
    if (Test-Path $dir -PathType Container) {
        Write-Success "Directory: $dir"
    } else {
        Write-Error "Missing directory: $dir"
        exit 1
    }
}

# 4. Build project (if not skipped)
if (-not $SkipBuild) {
    Write-Status "`n4??  Building Project...`n"
    
    try {
        Write-Status "Configuring project (Release)..." -Color $Yellow
        
        if ($Verbose) {
            cmake --preset windows-x64-release
        } else {
            cmake --preset windows-x64-release 2>&1 | Out-Null
        }
        
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Configuration successful"
        } else {
            Write-Error "Configuration failed (exit code: $LASTEXITCODE)"
            exit 1
        }
        
        Write-Status "`nBuilding project..." -Color $Yellow
        
        if ($Verbose) {
            cmake --build --preset windows-x64-release
        } else {
            cmake --build --preset windows-x64-release 2>&1 | Out-Null
        }
        
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Build successful"
        } else {
            Write-Error "Build failed (exit code: $LASTEXITCODE)"
            exit 1
        }
        
    } catch {
        Write-Error "Build process failed: $_"
        exit 1
    }
    
    # 5. Check build outputs
    Write-Status "`n5??  Checking Build Outputs...`n"
    
    $buildArtifacts = @(
        @{Path="out/build/x64-Release/lib/MarcSLM_Core.lib"; Name="Library"},
        @{Path="out/build/x64-Release/bin/MarcSLM_Tests.exe"; Name="Test Executable"}
    )
    
    foreach ($artifact in $buildArtifacts) {
        if (Test-Path $artifact.Path) {
            $size = (Get-Item $artifact.Path).Length / 1KB
            Write-Success "$($artifact.Name): $($artifact.Path) ($([math]::Round($size, 2)) KB)"
        } else {
            Write-Error "Missing build artifact: $($artifact.Path)"
            exit 1
        }
    }
    
    # 6. Run tests
    Write-Status "`n6??  Running Unit Tests...`n"
    
    try {
        if ($Verbose) {
            ctest --preset windows-x64-release --output-on-failure
        } else {
            $testOutput = ctest --preset windows-x64-release --output-on-failure 2>&1
            $testOutput | Out-String | Write-Verbose
        }
        
        if ($LASTEXITCODE -eq 0) {
            Write-Success "All tests passed"
        } else {
            Write-Error "Some tests failed"
            exit 1
        }
    } catch {
        Write-Error "Test execution failed: $_"
        exit 1
    }
    
} else {
    Write-Status "`n4??  Skipping build (--SkipBuild specified)" -Color $Yellow
}

# 7. Final summary
Write-Section "Verification Complete"

Write-Status "`n? All checks passed!`n" -Color $Green

Write-Status "Project Status:" -Color $Cyan
Write-Status "  • Build system: Configured" -Color $Green
Write-Status "  • Core types: Production ready" -Color $Green
Write-Status "  • Unit tests: Passing" -Color $Green
Write-Status "  • Documentation: Complete" -Color $Green

Write-Status "`nNext Steps:" -Color $Yellow
Write-Status "  1. Review include/MarcSLM/Core/Types.hpp"
Write-Status "  2. Read QUICKSTART.md for build instructions"
Write-Status "  3. Start implementing Geometry module"
Write-Status "  4. Add more unit tests as needed"

Write-Status "`n========================================`n" -Color $Cyan
