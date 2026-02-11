# Geometry Module

This module contains the bridge layer between:
- **Assimp** (3D model loading)
- **Manifold** (mesh slicing and boolean operations)
- **Clipper2** (2D polygon processing)

## Planned Components

- `MeshLoader.hpp` - Assimp-based 3D model import
- `Slicer.hpp` - Manifold-based mesh ? slice conversion
- `GeometryBridge.hpp` - Coordinate system transformations

## Responsibilities

1. Load 3D meshes from STL, 3MF, OBJ formats
2. Validate mesh manifoldness
3. Perform Z-plane intersection slicing
4. Convert 3D contours to 2D Clipper2 paths
