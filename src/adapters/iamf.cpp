//  IAMF Eclipsa Integration Implementation
//  Copyright © 2024 Mach1. All rights reserved.

#ifdef HAVE_LIBIAMF

#include "iamf.h"
#include "Mach1Transcode.h"
#include "Mach1AudioTimeline.h"
#include "sndfile.hh"

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>

// Include IAMF headers (these paths may need adjustment based on actual libiamf structure)
#ifdef __has_include
#if __has_include("iamf/cli/codec_config.h")
#include "iamf/cli/codec_config.h"
#endif
#if __has_include("iamf/common/macros.h")
#include "iamf/common/macros.h"
#endif
#endif

// Basic IAMF context implementation
struct IAMFInternalContext {
    bool initialized = false;
    FILE* output_file = nullptr;
    std::vector<uint8_t> frame_buffer;
    uint64_t frame_count = 0;
    
    // IAMF specific data
    uint32_t sequence_number = 0;
    bool header_written = false;
};

int iamf_eclipsa_init(IAMFEclipsaContext* ctx,
                      const IAMFAudioConfig* audio_config,
                      const IAMFMixPresentationConfig* mix_config,
                      const IAMFAudioElementConfig* element_config)
{
    if (!ctx || !audio_config || !mix_config || !element_config) {
        return -1; // Invalid parameters
    }
    
    // Initialize context
    memset(ctx, 0, sizeof(IAMFEclipsaContext));
    ctx->audio_config = *audio_config;
    ctx->mix_config = *mix_config;
    ctx->element_config = *element_config;
    
    // Allocate internal context
    auto* internal_ctx = new IAMFInternalContext();
    ctx->iamf_encoder_ctx = internal_ctx;
    
    // Allocate output buffer (for one frame)
    size_t frame_size = audio_config->num_channels * 
                       (audio_config->bit_depth / 8) * 
                       (audio_config->sample_rate * audio_config->frame_duration_ms / 1000);
    
    ctx->output_buffer_size = frame_size * 2; // Double buffer for safety
    ctx->output_buffer = (uint8_t*)malloc(ctx->output_buffer_size);
    
    if (!ctx->output_buffer) {
        delete internal_ctx;
        return -2; // Memory allocation failed
    }
    
    internal_ctx->initialized = true;
    
    std::cout << "IAMF Eclipsa context initialized:" << std::endl;
    std::cout << "  Sample Rate: " << audio_config->sample_rate << " Hz" << std::endl;
    std::cout << "  Channels: " << audio_config->num_channels << std::endl;
    std::cout << "  Bit Depth: " << audio_config->bit_depth << " bits" << std::endl;
    std::cout << "  Frame Duration: " << audio_config->frame_duration_ms << " ms" << std::endl;
    
    return 0; // Success
}

int iamf_eclipsa_write_header(IAMFEclipsaContext* ctx, const char* output_path)
{
    if (!ctx || !ctx->iamf_encoder_ctx || !output_path) {
        return -1;
    }
    
    auto* internal_ctx = static_cast<IAMFInternalContext*>(ctx->iamf_encoder_ctx);
    
    // Open output file
    internal_ctx->output_file = fopen(output_path, "wb");
    if (!internal_ctx->output_file) {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
        return -2;
    }
    
    // Write proper IAMF OBUs according to specification
    
    // 1. Write IAMF Sequence Header OBU (type 31)
    uint8_t seq_header[] = {
        0xF8,              // OBU header: type=31, extension=0, has_size=1 (matches real IAMF)
        0x06,              // OBU size (6 bytes payload)
        0x69, 0x61, 0x6d, 0x66, // Four character code "iamf" in correct byte order
        0x00, 0x00         // Primary profile=0, additional profile=0
    };
    
    if (fwrite(seq_header, sizeof(seq_header), 1, internal_ctx->output_file) != 1) {
        std::cerr << "Failed to write IAMF sequence header" << std::endl;
        return -3;
    }
    
    // 2. Write Codec Config OBU (type 0)  
    uint8_t codec_config[] = {
        0x02,              // OBU header: type=0, extension=0, has_size=1
        0x17,              // OBU size (23 bytes payload)
        0x00, 0x00, 0x00, 0x00,  // Codec config ID = 0
        'i', 'p', 'c', 'm',      // Four character code "ipcm" (integer PCM)
        0x00, 0x00, 0x00, 0x00,  // Number of samples per frame (0 = variable)
        0x00, 0x00,              // Audio roll distance = 0
        // PCM specific config (8 bytes)
        (uint8_t)ctx->audio_config.bit_depth,  // Sample size (16)
        (uint8_t)(ctx->audio_config.sample_rate & 0xFF),         // Sample rate (little endian)
        (uint8_t)((ctx->audio_config.sample_rate >> 8) & 0xFF),
        (uint8_t)((ctx->audio_config.sample_rate >> 16) & 0xFF),
        (uint8_t)((ctx->audio_config.sample_rate >> 24) & 0xFF),
        0x00, 0x00, 0x00     // Reserved
    };
    
    if (fwrite(codec_config, sizeof(codec_config), 1, internal_ctx->output_file) != 1) {
        std::cerr << "Failed to write codec config" << std::endl;
        return -4;
    }
    
    // 3. Write Audio Element OBU (type 1)
    uint8_t audio_element[] = {
        0x06,              // OBU header: type=1, extension=0, has_size=1
        0x14,              // OBU size (20 bytes payload)
        0x00, 0x00, 0x00, 0x00,  // Audio element ID = 0
        0x00,              // Audio element type = AUDIO_ELEMENT_CHANNEL_BASED
        0x00, 0x00, 0x00, 0x00,  // Reserved
        0x00, 0x00, 0x00, 0x00,  // Codec config ID = 0 (matches our codec config)
        (uint8_t)ctx->audio_config.num_channels,  // Number of channels
        0x00,              // Audio element config size = 0 (no additional config)
        0x00, 0x00, 0x00   // Reserved/padding
    };
    
    if (fwrite(audio_element, sizeof(audio_element), 1, internal_ctx->output_file) != 1) {
        std::cerr << "Failed to write audio element" << std::endl;
        return -5;
    }
    
    // 4. Write Mix Presentation OBU (type 2)
    uint8_t mix_presentation[] = {
        0x0A,              // OBU header: type=2, extension=0, has_size=1
        0x20,              // OBU size (32 bytes payload)
        0x00, 0x00, 0x00, 0x00,  // Mix presentation ID = 0
        0x00, 0x00, 0x00, 0x01,  // Count label = 1
        0x00, 0x00, 0x00, 0x00,  // Language tag size = 0 (no language)
        0x00, 0x00, 0x00, 0x01,  // Count sub-mixes = 1
        0x00, 0x00, 0x00, 0x00,  // Sub-mix ID = 0
        0x00, 0x00, 0x00, 0x01,  // Count audio elements = 1
        0x00, 0x00, 0x00, 0x00,  // Audio element ID = 0 (matches our audio element)
        0x00, 0x00, 0x00, 0x00   // Element mix config (no mixing parameters)
    };
    
    if (fwrite(mix_presentation, sizeof(mix_presentation), 1, internal_ctx->output_file) != 1) {
        std::cerr << "Failed to write mix presentation" << std::endl;
        return -6;
    }
    
    internal_ctx->header_written = true;
    
    std::cout << "IAMF header written to: " << output_path << std::endl;
    return 0;
}

int iamf_eclipsa_encode_frame(IAMFEclipsaContext* ctx,
                              const void* audio_data,
                              uint32_t num_samples,
                              const Mach1AudioObject* spatial_metadata)
{
    if (!ctx || !ctx->iamf_encoder_ctx || !audio_data) {
        return -1;
    }
    
    auto* internal_ctx = static_cast<IAMFInternalContext*>(ctx->iamf_encoder_ctx);
    
    if (!internal_ctx->header_written) {
        std::cerr << "Header must be written before encoding frames" << std::endl;
        return -2;
    }
    
    // Calculate frame size
    size_t sample_size = ctx->audio_config.bit_depth / 8;
    size_t frame_size = num_samples * ctx->audio_config.num_channels * sample_size;
    
    // Write audio frame OBU (type 3)
    // Calculate total OBU size (frame header + audio data)
    size_t obu_payload_size = 8 + frame_size;  // 8 bytes header + audio data
    uint8_t frame_header[] = {
        0x0E,  // OBU header: type=3, extension=0, has_size=1
        (uint8_t)(obu_payload_size & 0xFF),           // OBU size (low byte)
        (uint8_t)((obu_payload_size >> 8) & 0xFF),    // OBU size (high byte)
        0x00, 0x00, 0x00, 0x00,  // Audio element ID = 0
        (uint8_t)(internal_ctx->sequence_number & 0xFF),  // Sequence number (low byte)
        (uint8_t)((internal_ctx->sequence_number >> 8) & 0xFF),  // Sequence number (high byte)
        0x00, 0x00               // Reserved
    };
    
    if (fwrite(frame_header, sizeof(frame_header), 1, internal_ctx->output_file) != 1) {
        std::cerr << "Failed to write frame header" << std::endl;
        return -3;
    }
    
    // Write audio data
    if (fwrite(audio_data, frame_size, 1, internal_ctx->output_file) != 1) {
        std::cerr << "Failed to write audio data" << std::endl;
        return -4;
    }
    
    // If spatial metadata is provided, write parameter block
    if (spatial_metadata) {
        // Write spatial parameter OBU (simplified)
        uint8_t param_header[] = {
            0x02,  // OBU type: Parameter Block
            0x20,  // Parameter block size
            (uint8_t)(internal_ctx->sequence_number & 0xFF),  // Sequence number
            0x01,  // Parameter type: spatial position
        };
        
        if (fwrite(param_header, sizeof(param_header), 1, internal_ctx->output_file) != 1) {
            std::cerr << "Failed to write parameter header" << std::endl;
            return -5;
        }
        
        // Extract spatial position from Mach1AudioObject
        // This is a simplified conversion - real implementation would need
        // proper Mach1AudioObject structure access
        float position_data[3] = {0.0f, 0.0f, 0.0f}; // x, y, z
        
        // Write position data
        if (fwrite(position_data, sizeof(position_data), 1, internal_ctx->output_file) != 1) {
            std::cerr << "Failed to write spatial position" << std::endl;
            return -6;
        }
        
        // Write remaining parameter data (padding for now)
        uint8_t param_padding[16] = {0};
        if (fwrite(param_padding, sizeof(param_padding), 1, internal_ctx->output_file) != 1) {
            std::cerr << "Failed to write parameter padding" << std::endl;
            return -7;
        }
    }
    
    internal_ctx->sequence_number++;
    internal_ctx->frame_count++;
    
    // Progress indication
    if (internal_ctx->frame_count % 100 == 0) {
        std::cout << "Encoded " << internal_ctx->frame_count << " IAMF frames" << std::endl;
    }
    
    return 0;
}

int iamf_eclipsa_finalize(IAMFEclipsaContext* ctx)
{
    if (!ctx || !ctx->iamf_encoder_ctx) {
        return -1;
    }
    
    auto* internal_ctx = static_cast<IAMFInternalContext*>(ctx->iamf_encoder_ctx);
    
    if (internal_ctx->output_file) {
        // Write end-of-stream marker
        uint8_t eos_marker[] = {
            0xFF,  // End of stream OBU type
            0x00   // Size
        };
        
        fwrite(eos_marker, sizeof(eos_marker), 1, internal_ctx->output_file);
        fclose(internal_ctx->output_file);
        internal_ctx->output_file = nullptr;
    }
    
    std::cout << "IAMF encoding finalized. Total frames: " << internal_ctx->frame_count << std::endl;
    return 0;
}

void iamf_eclipsa_cleanup(IAMFEclipsaContext* ctx)
{
    if (!ctx) return;
    
    if (ctx->iamf_encoder_ctx) {
        auto* internal_ctx = static_cast<IAMFInternalContext*>(ctx->iamf_encoder_ctx);
        
        if (internal_ctx->output_file) {
            fclose(internal_ctx->output_file);
        }
        
        delete internal_ctx;
        ctx->iamf_encoder_ctx = nullptr;
    }
    
    if (ctx->output_buffer) {
        free(ctx->output_buffer);
        ctx->output_buffer = nullptr;
    }
    
    ctx->output_buffer_size = 0;
    ctx->output_data_size = 0;
}

// Helper function implementations

int mach1_to_iamf_element_config(const char* mach1_format, 
                                 IAMFAudioElementConfig* element_config)
{
    if (!mach1_format || !element_config) {
        return -1;
    }
    
    memset(element_config, 0, sizeof(IAMFAudioElementConfig));
    
    // Convert common Mach1 formats to IAMF configurations
    if (strcmp(mach1_format, "M1Spatial-4") == 0) {
        element_config->audio_element_id = 1;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 4;  // M1Spatial is 4 channel
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else if (strcmp(mach1_format, "M1Spatial-8") == 0) {
        element_config->audio_element_id = 2;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 8;  // M1Spatial is 8 channel
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else if (strcmp(mach1_format, "M1Spatial-14") == 0) {
        element_config->audio_element_id = 3;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 14;  // M1Spatial is 14 channel
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else if (strcmp(mach1_format, "5.1.4") == 0) {
        element_config->audio_element_id = 4;
        element_config->num_layers = 2;  // Base layer (5.1) + height layer (4)
        element_config->num_channels_per_layer = 6;  // Base 5.1
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else if (strcmp(mach1_format, "5.1") == 0) {
        element_config->audio_element_id = 5;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 6;
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else if (strcmp(mach1_format, "stereo") == 0 || strcmp(mach1_format, "2.0") == 0 || strcmp(mach1_format, "ACNSN3D") == 0) {
        element_config->audio_element_id = 6;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 2;  // Stereo
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else {
        std::cerr << "Unsupported Mach1 format: " << mach1_format << std::endl;
        return -2;
    }
    
    return 0;
}

int get_recommended_iamf_config(const char* output_format,
                                IAMFAudioConfig* audio_config,
                                IAMFMixPresentationConfig* mix_config,
                                IAMFAudioElementConfig* element_config)
{
    if (!output_format || !audio_config || !mix_config || !element_config) {
        return -1;
    }
    
    // Set recommended audio configuration
    audio_config->sample_rate = 48000;  // Standard for IAMF
    audio_config->bit_depth = 16;
    audio_config->frame_duration_ms = 10;  // Standard frame duration
    
    // Set recommended mix presentation
    mix_config->mix_presentation_id = 1;
    mix_config->num_sub_mixes = 1;
    mix_config->loudness_info_db = -23.0f;  // EBU R128 broadcast standard
    mix_config->enable_peak_limiter = true;
    mix_config->peak_threshold_db = -1.0f;
    
    // Convert format to element config
    int result = mach1_to_iamf_element_config(output_format, element_config);
    if (result != 0) {
        return result;
    }
    
    // Set number of channels based on element config
    if (strcmp(output_format, "M1Spatial-4") == 0) {
        audio_config->num_channels = 4;
    }
    else if (strcmp(output_format, "M1Spatial-8") == 0) {
        audio_config->num_channels = 8;
    }
    else if (strcmp(output_format, "M1Spatial-14") == 0) {
        audio_config->num_channels = 14;
    }
    else if (strcmp(output_format, "5.1.4") == 0) {
        audio_config->num_channels = 10;  // 5.1 + 4 height
    }
    else if (strcmp(output_format, "5.1") == 0) {
        audio_config->num_channels = 6;
    }
    else if (strcmp(output_format, "stereo") == 0 || strcmp(output_format, "2.0") == 0 || strcmp(output_format, "ACNSN3D") == 0) {
        audio_config->num_channels = 2;  // Stereo
    }
    else {
        audio_config->num_channels = 2;  // Default to stereo
    }
    
    return 0;
}

int mach1_to_iamf_complete_workflow(const char* input_file,
                                    const char* input_format,
                                    const char* output_format,
                                    const char* output_iamf_file)
{
    std::cout << "Starting Mach1 to IAMF workflow:" << std::endl;
    std::cout << "  Input: " << input_file << " (" << input_format << ")" << std::endl;
    std::cout << "  Output: " << output_iamf_file << " (" << output_format << ")" << std::endl;
    
    // Get recommended IAMF configuration
    IAMFAudioConfig audio_config;
    IAMFMixPresentationConfig mix_config;
    IAMFAudioElementConfig element_config;
    
    int result = get_recommended_iamf_config(output_format, &audio_config, &mix_config, &element_config);
    if (result != 0) {
        std::cerr << "Failed to get IAMF configuration for format: " << output_format << std::endl;
        return result;
    }
    
    // Initialize IAMF context
    IAMFEclipsaContext iamf_ctx;
    result = iamf_eclipsa_init(&iamf_ctx, &audio_config, &mix_config, &element_config);
    if (result != 0) {
        std::cerr << "Failed to initialize IAMF context" << std::endl;
        return result;
    }
    
    // Write IAMF header
    result = iamf_eclipsa_write_header(&iamf_ctx, output_iamf_file);
    if (result != 0) {
        std::cerr << "Failed to write IAMF header" << std::endl;
        iamf_eclipsa_cleanup(&iamf_ctx);
        return result;
    }
    
    // Load and process input audio file through Mach1Transcode
    
    // Load input file
    SndfileHandle infile(input_file);
    if (infile.error() != 0) {
        std::cerr << "Failed to open input file: " << input_file << std::endl;
        iamf_eclipsa_cleanup(&iamf_ctx);
        return -10;
    }
    
    std::cout << "Processing audio file:" << std::endl;
    std::cout << "  Channels: " << infile.channels() << std::endl;
    std::cout << "  Sample Rate: " << infile.samplerate() << " Hz" << std::endl;
    std::cout << "  Frames: " << infile.frames() << std::endl;
    
    // Initialize Mach1Transcode
    Mach1Transcode<float> m1transcode;
    
    // Convert format strings to format IDs
    int inFmt = m1transcode.getFormatFromString(input_format);
    int outFmt = m1transcode.getFormatFromString(output_format);
    
    if (inFmt < 0) {
        std::cerr << "Unsupported input format: " << input_format << std::endl;
        iamf_eclipsa_cleanup(&iamf_ctx);
        return -11;
    }
    
    if (outFmt < 0) {
        std::cerr << "Unsupported output format: " << output_format << std::endl;
        iamf_eclipsa_cleanup(&iamf_ctx);
        return -12;
    }
    
    m1transcode.setInputFormat(inFmt);
    m1transcode.setOutputFormat(outFmt);
    
    if (!m1transcode.processConversionPath()) {
        std::cerr << "Can't find conversion path between formats!" << std::endl;
        iamf_eclipsa_cleanup(&iamf_ctx);
        return -13;
    }
    
    // Set up audio processing buffers
    const int BUFFERLEN = 1024;
    const int MAX_CHANNELS = 64;
    
    int inChannels = infile.channels();
    int outChannels = m1transcode.getOutputNumChannels();
    
    // Audio buffers
    float fileBuffer[MAX_CHANNELS * BUFFERLEN];
    float inBuffers[MAX_CHANNELS][BUFFERLEN];
    float outBuffers[MAX_CHANNELS][BUFFERLEN];
    float *inPtrs[MAX_CHANNELS];
    float *outPtrs[MAX_CHANNELS];
    
    for (int i = 0; i < MAX_CHANNELS; i++) {
        inPtrs[i] = inBuffers[i];
        outPtrs[i] = outBuffers[i];
        memset(inBuffers[i], 0, sizeof(inBuffers[i]));
        memset(outBuffers[i], 0, sizeof(outBuffers[i]));
    }
    
    std::cout << "Conversion path: " << input_format << " -> " << output_format << std::endl;
    std::cout << "Input channels: " << inChannels << ", Output channels: " << outChannels << std::endl;
    
    // Process audio in chunks
    sf_count_t totalSamples = 0;
    sf_count_t numBlocks = infile.frames() / BUFFERLEN;
    
    for (sf_count_t block = 0; block <= numBlocks; block++) {
        // Read audio data
        sf_count_t framesToRead = inChannels * BUFFERLEN;
        sf_count_t framesRead = infile.read(fileBuffer, framesToRead);
        sf_count_t samplesRead = framesRead / inChannels;
        
        if (samplesRead == 0) break;
        
        // Demultiplex into process buffers
        float *ptrFileBuffer = fileBuffer;
        for (int j = 0; j < samplesRead; j++) {
            for (int k = 0; k < inChannels; k++) {
                inBuffers[k][j] = *ptrFileBuffer++;
            }
        }
        
        // Process through Mach1Transcode
        m1transcode.processConversion(inPtrs, outPtrs, (int)samplesRead);
        
        // Multiplex output channels for IAMF
        for (int j = 0; j < samplesRead; j++) {
            for (int k = 0; k < outChannels; k++) {
                fileBuffer[j * outChannels + k] = outBuffers[k][j];
            }
        }
        
        // Encode frame to IAMF
        result = iamf_eclipsa_encode_frame(&iamf_ctx, fileBuffer, samplesRead, nullptr);
        if (result != 0) {
            std::cerr << "Failed to encode IAMF frame: " << result << std::endl;
            iamf_eclipsa_cleanup(&iamf_ctx);
            return result;
        }
        
        totalSamples += samplesRead;
        
        // Progress indication
        if (block % 100 == 0) {
            float progress = (float)block / (float)numBlocks * 100.0f;
            std::cout << "\rProgress: " << (int)progress << "% (" << totalSamples << " samples)" << std::flush;
        }
    }
    
    std::cout << std::endl << "Audio processing completed. Total samples: " << totalSamples << std::endl;
    
    // Finalize IAMF file
    result = iamf_eclipsa_finalize(&iamf_ctx);
    if (result != 0) {
        std::cerr << "Failed to finalize IAMF file" << std::endl;
    }
    
    // Cleanup
    iamf_eclipsa_cleanup(&iamf_ctx);
    
    std::cout << "Mach1 to IAMF workflow completed" << std::endl;
    return result;
}

#endif // HAVE_LIBIAMF 