# Path Planning Module

This module generates laser scan vectors and hatching patterns for metal powder bed fusion.

## Planned Components

- `HatchGenerator.hpp` - Generate parallel line infill patterns
- `ScanVectorOptimizer.hpp` - Minimize laser jump distance
- `StripeDecomposition.hpp` - Divide layers into thermal stripes
- `ContourGenerator.hpp` - Generate perimeter scan paths

## Hatching Strategies

1. **Unidirectional** - All lines in one direction (fast, thermal gradient)
2. **Bidirectional** - Alternating directions (balanced stress)
3. **Crosshatch** - Orthogonal layers (isotropic properties)
4. **Hexagonal** - 60° rotation per layer (optimal isotropy)

## Optimization Goals

- Minimize laser travel distance
- Avoid sharp direction changes
- Balance thermal distribution
- Ensure proper overlap between scan vectors
