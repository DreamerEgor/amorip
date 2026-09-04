# Amorip - CS:GO Cheat Fixes

This project fixes four critical systems in a CS:GO cheat codebase:

1. **Recoil/RCS Debugging** - Proper horizontal and vertical recoil compensation with weapon profiles
2. **Spectator-list Logic** - Accurate spectator detection without false positives
3. **Aim Target Visibility Logic** - Correct occlusion detection for target selection
4. **ESP Visible/Occluded State** - Proper visibility rendering with separate color configuration

## Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

## Files Changed

- `src/core/recoil_system.h/cpp` - Recoil compensation with weapon profiles
- `src/core/spectator_detector.h/cpp` - Spectator list management
- `src/core/visibility_checker.h/cpp` - Reusable visibility detection
- `src/aim/target_selector.h/cpp` - Aim target selection with visibility
- `src/esp/renderer.h/cpp` - ESP rendering with proper visibility states
- `src/debug/logger.h/cpp` - Debug output system
