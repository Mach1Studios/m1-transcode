# IAMF/Eclipsa Integration with m1-transcode

This document describes the IAMF (Immersive Audio Model and Formats) integration added to m1-transcode, enabling you to author Eclipsa files correctly after transcoding.

## Overview

The IAMF integration allows you to:
- Convert Mach1 spatial audio formats to IAMF-compliant files
- Preserve spatial metadata during transcoding
- Generate Eclipsa files compatible with modern immersive audio standards
- Support multiple output formats including channel-based and scene-based audio

## Build Configuration

### Enable IAMF Support (Default: ON)
```bash
cmake . -DENABLE_IAMF_SUPPORT=ON
make
```

### Disable IAMF Support
```bash
cmake . -DENABLE_IAMF_SUPPORT=OFF
make
```

## Supported Workflow

### Input Formats
- M1Spatial-4
- M1Spatial-8 
- M1Spatial-14
- 5.1 surround
- 5.1.4 (5.1 + 4 height channels)

### Output Formats (IAMF/Eclipsa)
- IAMF files (.iamf) with spatial metadata
- Compatible with Eclipsa players and renderers
- Supports both channel-based and scene-based audio elements
- Mix presentations with loudness normalization

## Usage Examples

### Basic Usage in Code

```cpp
#include "iamf_eclipsa_integration.h"

// Example: Convert M1Spatial to IAMF
int result = mach1_to_iamf_complete_workflow(
    "input.wav",           // Input file
    "M1Spatia-14",          // Input format
    "M1Spatial-14",          // Output format  
    "output.iamf"         // Output IAMF file
);
```

### Step-by-Step Integration

```cpp
#include "iamf_eclipsa_integration.h"

// 1. Get recommended IAMF configuration
IAMFAudioConfig audio_config;
IAMFMixPresentationConfig mix_config;
IAMFAudioElementConfig element_config;

get_recommended_iamf_config("M1Horizon", &audio_config, &mix_config, &element_config);

// 2. Initialize IAMF context
IAMFEclipsaContext iamf_ctx;
iamf_eclipsa_init(&iamf_ctx, &audio_config, &mix_config, &element_config);

// 3. Write IAMF header
iamf_eclipsa_write_header(&iamf_ctx, "output.iamf");

// 4. Process audio frames (integrate with your existing Mach1Transcode loop)
// For each audio frame:
iamf_eclipsa_encode_frame(&iamf_ctx, audio_data, num_samples, spatial_metadata);

// 5. Finalize and cleanup
iamf_eclipsa_finalize(&iamf_ctx);
iamf_eclipsa_cleanup(&iamf_ctx);
```

### Command Line Usage (Future Enhancement)

```bash
# Basic transcode with IAMF output
./m1-transcode -in-file input.wav -in-fmt M1Spatial-8 -out-fmt M1Spatial-4 -out-file output.iamf -enable-iamf

# With custom IAMF parameters
./m1-transcode -in-file input.wav -in-fmt M1Spatial-8 -out-fmt 5.1.4 -out-file output.iamf -enable-iamf -iamf-loudness -23.0 -iamf-peak-limiter
```

## Integration with Existing Workflow

The IAMF integration is designed to work alongside your existing m1-transcode workflow:

1. **Load audio file** using your existing audio loading code
2. **Set up Mach1Transcode** with desired input → output format conversion
3. **Initialize IAMF context** with matching output format
4. **Process frames** through both Mach1Transcode and IAMF encoder
5. **Generate outputs** in both traditional format and IAMF/Eclipsa

### Example Integration Point

In your main processing loop, after Mach1Transcode processing:

```cpp
// Existing Mach1Transcode processing
float* processedAudio = mach1Transcode.processBuffer(inputBuffer, bufferSize);

// Add IAMF encoding
if (iamfEnabled) {
    iamf_eclipsa_encode_frame(&iamf_ctx, processedAudio, bufferSize, &audioObjects[i]);
}

// Continue with existing output writing
writeToOutputFile(processedAudio, bufferSize);
```

## Configuration Options

### Audio Configuration
- **Sample Rate**: 48000 Hz (IAMF standard)
- **Bit Depth**: 16-bit (configurable)
- **Frame Duration**: 10ms (IAMF standard)
- **Channels**: Automatically determined from format

### Mix Presentation Configuration
- **Loudness**: -23.0 LKFS (EBU R128 broadcast standard)
- **Peak Limiter**: Enabled by default
- **Peak Threshold**: -1.0 dB

### Audio Element Configuration
- **Element Types**: Channel-based, Scene-based
- **Layering**: Support for base + height layer configurations
- **Ambisonics**: Support for higher-order ambisonics (HOA)

## Format Mapping

| Mach1 Format | IAMF Configuration | Channels | Layers |
|--------------|-------------------|----------|---------|
| M1Spatial-4  | Channel-based     | 4        | 1       |
| M1Spatial-8  | Channel-based     | 8        | 1       |
| 5.1          | Channel-based     | 6        | 1       |
| 5.1.4        | Channel-based     | 10       | 2       |

## Spatial Metadata Preservation

The integration preserves spatial metadata from Mach1AudioObject:
- **Position data** (X, Y, Z coordinates)
- **Orientation** (if available)
- **Animation keyframes** (temporal changes)
- **Object-based parameters** (gain, spread, etc.)

## Output Files

### IAMF File Structure
- **Header**: File identification and version
- **Codec Config**: Audio format specifications
- **Audio Elements**: Channel or scene configuration
- **Mix Presentation**: Playback configuration
- **Audio Frames**: Encoded audio data with timing
- **Parameter Blocks**: Spatial metadata per frame

### Compatibility
Generated IAMF files are compatible with:
- Eclipsa-compliant players
- AOM libiamf reference decoder
- IAMF-enabled media players
- Web browsers with IAMF support

## Future Enhancements

Planned improvements include:
- **Command-line flag integration** with existing m1-transcode CLI
- **Batch processing** support for multiple files
- **Real-time streaming** IAMF encoding
- **Quality metrics** and validation tools

## Related Documentation

- [IAMF Specification](https://aomediacodec.github.io/iamf/)
- [libiamf Reference](https://github.com/AOMediaCodec/libiamf)
- [iamf-tools Documentation](https://github.com/AOMediaCodec/iamf-tools)
- [Mach1 Spatial Documentation](https://github.com/Mach1Studios/m1-sdk)