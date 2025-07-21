//  IAMF Eclipsa Integration
//  Copyright © 2024 Mach1. All rights reserved.

#pragma once

#ifdef HAVE_LIBIAMF
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct Mach1Point3D Mach1Point3D;

// Forward declaration - note: Mach1AudioObject is actually defined as a class
#ifdef __cplusplus
class Mach1AudioObject;
#else
typedef struct Mach1AudioObject Mach1AudioObject;
#endif

// IAMF/Eclipsa integration structures
typedef struct {
    uint32_t sample_rate;
    uint32_t num_channels;
    uint32_t bit_depth;
    uint32_t frame_duration_ms;  // Usually 10ms for IAMF
} IAMFAudioConfig;

typedef struct {
    uint32_t mix_presentation_id;
    uint32_t num_sub_mixes;
    float loudness_info_db;      // In dB (e.g., -23.0 for broadcast)
    bool enable_peak_limiter;
    float peak_threshold_db;     // Peak threshold in dB
} IAMFMixPresentationConfig;

typedef struct {
    uint32_t audio_element_id;
    uint32_t num_layers;
    uint32_t num_channels_per_layer;
    bool is_scene_based;         // false for channel-based, true for scene-based
    uint32_t ambisonics_mode;    // 0 = mono, 1 = projection, 3 = scene
} IAMFAudioElementConfig;

typedef struct {
    IAMFAudioConfig audio_config;
    IAMFMixPresentationConfig mix_config;
    IAMFAudioElementConfig element_config;
    
    // Internal state (opaque pointers)
    void* iamf_encoder_ctx;
    void* iamf_writer_ctx;
    
    // Output buffer management
    uint8_t* output_buffer;
    size_t output_buffer_size;
    size_t output_data_size;
} IAMFEclipsaContext;

// Core IAMF integration functions

/**
 * Initialize IAMF/Eclipsa context for writing IAMF files after Mach1 transcoding
 * @param ctx Pointer to context structure to initialize
 * @param audio_config Audio format configuration
 * @param mix_config Mix presentation configuration
 * @param element_config Audio element configuration
 * @return 0 on success, negative on error
 */
int iamf_eclipsa_init(IAMFEclipsaContext* ctx,
                      const IAMFAudioConfig* audio_config,
                      const IAMFMixPresentationConfig* mix_config,
                      const IAMFAudioElementConfig* element_config);

/**
 * Write IAMF header and initialization data
 * @param ctx Initialized IAMF context
 * @param output_path Output file path for .iamf file
 * @return 0 on success, negative on error
 */
int iamf_eclipsa_write_header(IAMFEclipsaContext* ctx, const char* output_path);

/**
 * Process audio frame from Mach1 transcode and encode to IAMF
 * @param ctx IAMF context
 * @param audio_data Interleaved audio samples (float or int16_t based on config)
 * @param num_samples Number of samples per channel
 * @param spatial_metadata Optional spatial metadata from Mach1AudioObject
 * @return 0 on success, negative on error
 */
int iamf_eclipsa_encode_frame(IAMFEclipsaContext* ctx,
                              const void* audio_data,
                              uint32_t num_samples,
                              const Mach1AudioObject* spatial_metadata);

/**
 * Finalize IAMF file and write footer
 * @param ctx IAMF context
 * @return 0 on success, negative on error
 */
int iamf_eclipsa_finalize(IAMFEclipsaContext* ctx);

/**
 * Clean up IAMF context and free resources
 * @param ctx IAMF context to clean up
 */
void iamf_eclipsa_cleanup(IAMFEclipsaContext* ctx);

// Helper functions for integration with existing Mach1 workflow

/**
 * Convert Mach1 spatial audio format to IAMF audio element configuration
 * @param mach1_format Input format (e.g., "M1Spatial-4", "M1Spatial-8", "M1Spatial-14", "5.1.4", etc.)
 * @param element_config Output IAMF element configuration
 * @return 0 on success, negative if format not supported
 */
int mach1_to_iamf_element_config(const char* mach1_format, 
                                 IAMFAudioElementConfig* element_config);

/**
 * Convert Mach1AudioObject to IAMF spatial parameters
 * @param audio_obj Mach1 audio object with spatial metadata
 * @param iamf_params Output buffer for IAMF parameter data
 * @param max_size Maximum size of output buffer
 * @return Size of written data, or negative on error
 */
int mach1_audio_object_to_iamf_params(const Mach1AudioObject* audio_obj,
                                      uint8_t* iamf_params,
                                      size_t max_size);

/**
 * Get recommended IAMF configuration for common Mach1 transcode outputs
 * @param output_format Mach1 output format string
 * @param audio_config Output audio configuration
 * @param mix_config Output mix presentation configuration
 * @param element_config Output audio element configuration
 * @return 0 on success, negative if format not supported
 */
int get_recommended_iamf_config(const char* output_format,
                                IAMFAudioConfig* audio_config,
                                IAMFMixPresentationConfig* mix_config,
                                IAMFAudioElementConfig* element_config);

// Example usage workflow function
/**
 * Complete workflow: Mach1 transcode to IAMF/Eclipsa
 * This demonstrates the full integration between m1-transcode and IAMF
 * 
 * @param input_file Input audio file path
 * @param input_format Mach1 input format (e.g., "M1Spatial-4", "M1Spatial-8", "M1Spatial-14", "5.1", etc.)
 * @param output_format Mach1 output format (e.g., "M1Spatial-4", "M1Spatial-8", "M1Spatial-14", "5.1.4", etc.)
 * @param output_iamf_file Output IAMF file path
 * @return 0 on success, negative on error
 */
int mach1_to_iamf_complete_workflow(const char* input_file,
                                    const char* input_format,
                                    const char* output_format,
                                    const char* output_iamf_file);

#ifdef __cplusplus
}
#endif

#endif // HAVE_LIBIAMF 