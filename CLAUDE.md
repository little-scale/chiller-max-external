# Chiller~ - Real-time Spectral Freezing External

## Project Overview

A Max/MSP external that performs real-time spectral freezing of audio buffer content, creating evolving drones and textural sounds through FFT-based granular synthesis.

### Original Prompt
Create a Max external that implements real-time spectral freezing of an audio buffer:
- Accept audio buffer name and position parameter (0.0-1.0) 
- Extract 50-200ms windows for FFT analysis
- Maintain magnitude spectrum while randomizing phase spectrum
- Use overlap-add synthesis for continuous output
- Create evolving drones with spectral character of original audio

## Technical Implementation

### Core Architecture
- **Type**: MSP Audio External (`t_pxobject`)
- **FFT Implementation**: Custom radix-2 Cooley-Tukey FFT/IFFT
- **Processing**: Real-time overlap-add granular synthesis
- **Threading**: Single-threaded with rate limiting for safety

### Key Components
1. **Spectrum Capture**: Windowed FFT analysis of buffer content
2. **Spectral Freezing**: Magnitude preservation with phase randomization
3. **Grain Synthesis**: Overlap-add IFFT resynthesis
4. **Normalization**: Energy-based magnitude control

### FFT Configuration
- **Configurable Sizes**: 512, 1024, 2048, 4096, 8192 (powers of 2)
- **Default Size**: 2048 (good balance of quality vs performance)
- **Hop Size**: FFT_size/4 for 4:1 overlap
- **Window**: Hann window for analysis and synthesis

### Processing Pipeline
```
Buffer Audio → Windowed FFT → Magnitude Extraction → Phase Randomization → IFFT → Overlap-Add → Output
```

## Development History

### Phase 1: Core Implementation ✅
- Basic FFT/IFFT spectral processing
- Buffer~ integration and access
- Real-time grain generation
- Parameter control system

### Phase 2: Stability & Performance ✅
- Auto-freeze on position changes
- Rate limiting (500ms minimum between position changes)
- Concurrent capture protection
- Configurable FFT sizes at instantiation

### Phase 3: Quality & Debugging ✅
- Comprehensive debug system (`bang` message)
- **CRITICAL FIX**: Spectrum magnitude normalization
- Amplitude variation implementation fix
- Universal binary support

## Critical Bug Fixes

### Magnitude Explosion Issue ⚠️ MAJOR FIX
**Problem**: Spectrum magnitudes growing exponentially (values reaching 334,635+)
**Root Cause**: FFT normalization feedback loop causing energy accumulation
**Solution**: Energy-based spectrum normalization in capture phase
```cpp
double target_energy = x->fft_size * 0.1;
double normalization_factor = sqrt(target_energy / spectrum_energy);
```
**Impact**: Eliminated "incredibly noisy and loud" artifacts completely

### Rate Limiting Protection
**Problem**: Rapid position changes caused noise artifacts
**Solution**: 500ms minimum interval between position changes
```cpp
if (current_time - x->last_position_change_time < 500.0) return;
```

### Amplitude Variation Fix
**Problem**: `ampvar` parameter had no audible effect
**Root Cause**: Incorrect random distribution and formula
**Solution**: Fixed distribution range and calculation formula

## Build System

### CMake Configuration
```cmake
cmake_minimum_required(VERSION 3.19)
project(chiller~)
include(../../source/max-sdk-base/script/max-pretarget.cmake)
```

### Build Commands
```bash
cd projects/chiller
mkdir build && cd build
cmake -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..
cmake --build .
```

### Output Location
- Built external: `/Users/a1106632/Documents/Max 8/Packages/externals/chiller.mxo`
- Universal binary (Intel + Apple Silicon)

## Parameters & Messages

### Core Functions
- `set <buffername>` - Set buffer to analyze
- `position <0.0-1.0>` - Set analysis position (auto-captures spectrum)
- `freeze` - Manually capture spectrum at current position
- `bang` - Output comprehensive debug information

### Synthesis Parameters
- `rate <0.1-4.0>` - Grain generation rate (default: 1.0)
- `phaserand <0.0-1.0>` - Phase randomization amount (default: 0.1)
- `ampvar <0.0-0.5>` - Amplitude variation amount (default: 0.1)
- `overlap <1.0-8.0>` - Overlap factor for synthesis (default: 4.0)

### Object Arguments
- **FFT Size**: `chiller~ 2048` (512, 1024, 2048, 4096, 8192)
- **Buffer Name**: `chiller~ mybuffer` or `chiller~ 2048 mybuffer`

## Performance Characteristics

### CPU Usage by FFT Size
- **512**: Very low CPU, basic frequency resolution
- **1024**: Low CPU, good for multiple instances  
- **2048**: Moderate CPU, recommended default
- **4096**: High CPU, detailed frequency analysis (caused >100% CPU)
- **8192**: Very high CPU, maximum detail

### Memory Usage
- 6 vectors × FFT_size × 8 bytes per instance
- 2048 FFT: ~98KB per instance
- 4096 FFT: ~196KB per instance

## Debug System

### Comprehensive State Monitoring
Send `bang` to output detailed information:
- Configuration (FFT size, sample rate, buffer info)
- Analysis state (position, spectrum capture status)
- Timing (position change intervals - critical for noise diagnosis)
- Synthesis parameters
- **Spectrum energy levels** (watch for magnitude explosion)
- Overlap buffer states and sample values

### Diagnostic Examples
**Healthy State**:
```
Spectrum Energy: 88633.919625
Max Magnitude: 94.018975
```

**Noisy State (before fix)**:
```
Spectrum Energy: 871190700897.731689
Max Magnitude: 334635.621325
```

## Development Commands

### Standard Build Process
```bash
# Navigate to projects directory
cd "/Users/a1106632/Documents/Max 8/Packages/max-sdk-main/projects/chiller"

# Create and configure build
mkdir build && cd build
cmake -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..

# Build external
cmake --build .

# Verify output
ls "/Users/a1106632/Documents/Max 8/Packages/externals/chiller.mxo"
```

### Debugging & Testing
```bash
# Check architecture
file "/Users/a1106632/Documents/Max 8/Packages/externals/chiller.mxo/Contents/MacOS/chiller"

# Output should show: Mach-O universal binary with 2 architectures
```

## Creative Applications

### Sound Design Techniques
- **Drone Creation**: Freeze percussive attacks for sustained textures
- **Spectral Morphing**: Sequence through different buffer positions
- **Textural Layers**: Multiple instances at different positions/parameters
- **Real-time Performance**: Map position to controllers for expressive control

### Recommended Sources
- Percussive sounds (metallic textures)
- Vocal recordings (formant-rich drones)
- Field recordings (ambient soundscapes)
- Synthesized tones (harmonic content)

## Documentation Status ✅

- **README.md**: Complete user documentation with installation, usage, parameters
- **CLAUDE.md**: Complete technical documentation for developers
- **Max Help Patch**: Interactive documentation with examples
- **Code Comments**: Comprehensive inline documentation

## Testing & Validation ✅

- Universal binary builds tested on M2 MacBook Air
- Multiple FFT sizes validated (512-8192)
- Rate limiting prevents noise artifacts
- Magnitude normalization eliminates spectrum explosion
- Debug system provides comprehensive state monitoring
- All core functionality verified working

## Known Issues & Limitations

### Resolved Issues ✅
- ~~Magnitude explosion causing extreme noise~~ → Fixed with normalization
- ~~Rapid position changes causing artifacts~~ → Fixed with rate limiting
- ~~Amplitude variation not working~~ → Fixed implementation
- ~~High CPU with large FFT sizes~~ → User configurable, documented

### Current Limitations
- Single buffer support (no crossfading between buffers)
- No preset system for parameter combinations
- Position changes limited to 500ms intervals (by design)

## Future Development Opportunities

### Potential Enhancements
- **Multiple Buffer Support**: Crossfade between different buffers
- **Spectral Filtering**: Frequency-selective freezing
- **Preset System**: Save/recall parameter combinations
- **MIDI Integration**: Direct MIDI control of parameters

### Performance Optimizations
- **FFT Library Integration**: Replace custom FFT with vDSP or FFTW
- **Multi-threading**: Separate capture and synthesis threads
- **SIMD Optimization**: Vector processing for FFT operations

This project demonstrates advanced Max SDK techniques including real-time FFT processing, buffer~ integration, parameter management, robust error handling, and comprehensive debugging systems.