# chiller~ - Real-time Spectral Freezing External for Max

A Max/MSP external that performs real-time spectral freezing of audio buffer content, creating evolving drones and textural sounds through FFT-based granular synthesis.

## Overview

chiller~ captures the spectral content of any position within an audio buffer and "freezes" it into a continuous, evolving drone. The frozen spectrum is continuously resynthesized with configurable phase randomization and amplitude variation, creating rich, organic textures from the original audio material.

## Key Features

- **Real-time spectral analysis** with configurable FFT sizes (512-8192)
- **Automatic spectrum capture** on position changes
- **Phase randomization** for evolving spectral character
- **Amplitude variation** for dynamic textural changes  
- **Rate limiting** prevents noise artifacts from rapid position changes
- **Stereo output** with slight channel spread
- **Universal binary** support (Intel + Apple Silicon)

## Installation

1. Copy `chiller~.mxo` to your Max externals folder
2. Restart Max if necessary
3. Create a `chiller~` object in any patcher

## Basic Usage

```
[chiller~ 2048 mybuffer]
|
[dac~]
```

### Quick Start
1. Load audio into a buffer~ object
2. Create `chiller~ buffername` 
3. Send `position 0.5` to analyze the middle of the buffer
4. Adjust parameters and explore different positions

## Object Arguments

- **FFT Size** (optional): Power of 2 from 512-8192. Default: 2048
  - `chiller~` → FFT size 2048
  - `chiller~ 4096` → FFT size 4096 (higher CPU, more detail)
  - `chiller~ 1024` → FFT size 1024 (lower CPU, less detail)

- **Buffer Name** (optional): Name of buffer~ to analyze
  - `chiller~ mybuffer` → FFT size 2048, buffer "mybuffer"
  - `chiller~ 4096 mybuffer` → FFT size 4096, buffer "mybuffer"

## Messages

### Core Functions
- `set <buffername>` - Set buffer to analyze
- `position <0.0-1.0>` - Set analysis position in buffer (auto-captures spectrum)
- `freeze` - Manually capture spectrum at current position

### Parameters
- `rate <0.1-4.0>` - Grain generation rate (default: 1.0)
- `phaserand <0.0-1.0>` - Phase randomization amount (default: 0.1)
- `ampvar <0.0-0.5>` - Amplitude variation amount (default: 0.1)
- `overlap <1.0-8.0>` - Overlap factor for synthesis (default: 4.0)

### Debugging
- `bang` - Output comprehensive debug information to Max console

## Parameters Explained

### Position (0.0-1.0)
Controls where in the buffer to extract spectral content:
- `0.0` = beginning of buffer
- `0.5` = middle of buffer  
- `1.0` = end of buffer

**Note**: Position changes are rate-limited to prevent noise artifacts. Changes faster than 500ms are ignored.

### Grain Rate (0.1-4.0)
Controls how frequently new grains are generated:
- `0.5` = half speed (longer, more sustained grains)
- `1.0` = normal speed
- `2.0` = double speed (shorter, more active grains)

### Phase Randomization (0.0-1.0)
Amount of random phase variation applied to each grain:
- `0.0` = no randomization (static, can sound robotic)
- `0.1` = subtle variation (default, natural evolution)
- `0.5` = moderate randomization (more active textures)
- `1.0` = maximum randomization (chaotic, noisy)

### Amplitude Variation (0.0-0.5)
Random amplitude scaling applied to spectral bins:
- `0.0` = no variation (static amplitude)
- `0.1` = subtle variation (default, gentle fluctuation)
- `0.3` = moderate variation (noticeable amplitude changes)
- `0.5` = strong variation (dramatic amplitude fluctuations)

## Technical Details

### FFT Processing
- **Window**: Hann window for analysis and synthesis
- **Overlap**: 4:1 overlap-add synthesis for smooth output
- **Hop Size**: FFT_size/4 for optimal overlap
- **Normalization**: Automatic spectrum energy normalization prevents magnitude explosion

### Performance Notes
- **FFT Size vs CPU**: Larger FFT = higher CPU usage but more frequency detail
- **2048**: Good balance for most applications
- **4096**: Higher detail, ~4x CPU usage
- **1024**: Lower CPU, suitable for multiple instances

### Audio Quality
- **Sample Rate**: Supports any sample rate Max provides
- **Bit Depth**: 64-bit internal processing
- **Latency**: ~43ms at 2048 FFT size (at 48kHz)

## Creative Applications

### Drone Generation
Use chiller~ to create sustained drones from any audio source:
```
- Load percussive sounds for metallic drones
- Use vocal recordings for formant-rich textures  
- Process field recordings for ambient soundscapes
```

### Textural Processing
Layer multiple chiller~ instances at different positions:
```
[chiller~ 2048 buffer1] → position 0.2
[chiller~ 2048 buffer1] → position 0.7
[chiller~ 2048 buffer2] → position 0.5
```

### Real-time Performance
Map position to controllers for expressive real-time exploration:
```
[ctlin] → [scale 0. 127. 0. 1.] → [chiller~]
```

## Troubleshooting

### No Sound Output
1. Check buffer~ is loaded with audio
2. Verify buffer name matches `set` message
3. Send `bang` for debug info - check "Spectrum Captured: YES"

### Noise/Artifacts
1. Check debug output for magnitude explosion (values >1000)
2. Avoid rapid position changes (<500ms apart)
3. Restart Max if normalization fails

### High CPU Usage
1. Reduce FFT size: `chiller~ 1024` instead of `chiller~ 4096`
2. Increase grain rate for fewer overlapping grains
3. Use fewer simultaneous instances

### Buffer Errors
- "Buffer too small": Ensure buffer has at least FFT_size samples
- "Buffer not found": Check buffer~ object exists and has matching name

## Debug Information

Send `bang` to chiller~ to output detailed state information:
- Configuration (FFT size, sample rate)
- Buffer status and audio content info
- Analysis state and timing
- Synthesis parameters
- Spectrum energy levels (watch for magnitude explosion)
- Overlap buffer states

## Version History

### v1.0.0
- Initial release with configurable FFT sizes
- Real-time spectral freezing
- Phase randomization and amplitude variation
- Rate limiting for noise prevention
- Comprehensive debug system
- Spectrum normalization for stability

## Credits

Developed using the Max SDK. Part of the max-sdk-main project collection.

## License

See project license file.