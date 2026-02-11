# Thermal Module

This module handles BuildStyle region classification for DMLS/SLM thermal management.

## Planned Components

- `RegionClassifier.hpp` - Classify slice regions by thermal strategy
- `BuildStyleMap.hpp` - Map geometry to BuildStyleID
- `ThermalAnalysis.hpp` - Layer-to-layer heat accumulation analysis

## BuildStyle Categories

1. **Contour** - Outer perimeter (high precision, slower speed)
2. **Skin** - Top/bottom surface layers (cosmetic finish)
3. **Infill** - Internal solid regions (optimized for speed)
4. **Support** - Sacrificial structures (low energy density)

## Classification Algorithm

- Detect outer perimeters (offset from boundary)
- Identify top/bottom layers (layer-to-layer comparison)
- Separate infill from contour regions
- Mark support structures (if present)
