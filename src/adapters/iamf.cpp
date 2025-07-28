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

// LEB128 encoding utilities for IAMF compliance
static int write_leb128(uint8_t* buffer, uint64_t value) {
    int bytes_written = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }
        buffer[bytes_written++] = byte;
    } while (value != 0);
    return bytes_written;
}

static int write_uleb128(FILE* file, uint64_t value) {
    int bytes_written = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }
        fputc(byte, file);
        bytes_written++;
    } while (value != 0);
    return bytes_written;
}

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
        return -1;
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
    
    // Write proper IAMF OBUs according to specification v1.0.0
    
    // 1. IA Sequence Header OBU (type 31, 0x1F)
    uint8_t obu_header = (31 << 3) | 0x02;  // type=31, extension_flag=0, has_size_flag=1
    fputc(obu_header, internal_ctx->output_file);
    write_uleb128(internal_ctx->output_file, 6);  // payload size
    
    // Write "iamf" fourcc
    fwrite("iamf", 4, 1, internal_ctx->output_file);
    
    // Primary and additional profiles
    fputc(0x00, internal_ctx->output_file);  // primary_profile = Simple
    fputc(0x00, internal_ctx->output_file);  // additional_profile = Simple
    
    // 2. Codec Config OBU (type 0)
    obu_header = (0 << 3) | 0x02;  // type=0, has_size_flag=1
    fputc(obu_header, internal_ctx->output_file);
    
    // Calculate exact payload size for codec config
    // codec_config_id (1) + fourcc (4) + samples_per_frame (1) + audio_roll_distance (1) + 
    // sample_format_flags (1) + sample_size (1) + sample_rate (3) + reserved (1) = 13 bytes
    size_t codec_config_size = 1 + 4 + 1 + 1 + 1 + 1 + 3 + 1;  // = 13 bytes
    write_uleb128(internal_ctx->output_file, codec_config_size);
    
    // Codec config payload
    write_uleb128(internal_ctx->output_file, 0);     // codec_config_id = 0
    fwrite("ipcm", 4, 1, internal_ctx->output_file); // fourcc = "ipcm"
    write_uleb128(internal_ctx->output_file, 0);     // num_samples_per_frame = 0 (variable)
    write_uleb128(internal_ctx->output_file, 0);     // audio_roll_distance = 0
    
    // LPCM decoder config
    fputc(0x01, internal_ctx->output_file);          // sample_format_flags (little endian)
    fputc(ctx->audio_config.bit_depth, internal_ctx->output_file);  // sample_size
    write_uleb128(internal_ctx->output_file, ctx->audio_config.sample_rate);  // sample_rate
    fputc(0x00, internal_ctx->output_file);          // reserved
    
    // 3. Audio Element OBU (type 1)
    obu_header = (1 << 3) | 0x02;  // type=1, has_size_flag=1
    fputc(obu_header, internal_ctx->output_file);
    
    // Calculate payload size based on element type
    size_t audio_element_size;
    bool is_scene_based = ctx->element_config.is_scene_based;
    
    if (is_scene_based) {
        // Scene-based (ambisonics) element size calculation
        // audio_element_id (1) + audio_element_type (1) + reserved (1) + codec_config_id (1) + 
        // num_substreams (1) + substream_ids (num_channels) + num_parameters (1) +
        // ambisonics_config: ambisonics_mode (1) + output_channel_count (1) + substream_count (1) +
        // coupled_substream_count (1) + demixing_matrix (if projection mode, skip for now)
        audio_element_size = 1 + 1 + 1 + 1 + 1 + ctx->audio_config.num_channels + 1 + 1 + 1 + 1 + 1;
    } else {
        // Channel-based element size calculation  
        // audio_element_id (1) + audio_element_type (1) + reserved (1) + codec_config_id (1) + 
        // num_substreams (1) + substream_ids (num_channels) + num_parameters (1) + 
        // scalable_channel_layout_config: num_layers (1) + reserved (1) + 
        // channel_audio_layer_config: loudspeaker_layout (1) + output_gain_is_present (1) + 
        // recon_gain_is_present (1) + reserved (1) + substream_count (1) + coupled_substream_count (1)
        audio_element_size = 1 + 1 + 1 + 1 + 1 + ctx->audio_config.num_channels + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1;
    }
    write_uleb128(internal_ctx->output_file, audio_element_size);
    
    // Audio element payload
    write_uleb128(internal_ctx->output_file, ctx->element_config.audio_element_id);  // audio_element_id
    
    if (is_scene_based) {
        fputc(0x01, internal_ctx->output_file);      // audio_element_type = SCENE_BASED
    } else {
        fputc(0x00, internal_ctx->output_file);      // audio_element_type = CHANNEL_BASED
    }
    
    fputc(0x00, internal_ctx->output_file);          // reserved
    write_uleb128(internal_ctx->output_file, 0);     // codec_config_id = 0
    write_uleb128(internal_ctx->output_file, ctx->audio_config.num_channels);  // num_substreams
    
    // Write substream IDs
    for (uint32_t i = 0; i < ctx->audio_config.num_channels; i++) {
        write_uleb128(internal_ctx->output_file, i);
    }
    
    write_uleb128(internal_ctx->output_file, 0);     // num_parameters = 0
    
    if (is_scene_based) {
        // Ambisonics config for scene-based elements
        fputc(ctx->element_config.ambisonics_mode, internal_ctx->output_file);  // ambisonics_mode (0=mono, 1=projection)
        write_uleb128(internal_ctx->output_file, ctx->audio_config.num_channels);  // output_channel_count  
        fputc(ctx->audio_config.num_channels, internal_ctx->output_file);  // substream_count
        fputc(0x00, internal_ctx->output_file);      // coupled_substream_count = 0
        
        std::cout << "Scene-based audio element configured for " << ctx->audio_config.num_channels << "-channel ambisonics" << std::endl;
    } else {
        // Scalable channel layout config for channel-based elements
        fputc(0x01, internal_ctx->output_file);      // num_layers = 1
        fputc(0x00, internal_ctx->output_file);      // reserved
        
        // Channel audio layer config - set appropriate loudspeaker layout
        uint8_t loudspeaker_layout = 0xFF;  // Default: RESERVED for custom layouts
        
        if (ctx->audio_config.num_channels == 2) {
            loudspeaker_layout = 0x01;  // STEREO
        } else if (ctx->audio_config.num_channels == 6) {
            loudspeaker_layout = 0x03;  // 5.1
        } else if (ctx->audio_config.num_channels == 4) {
            loudspeaker_layout = 0xFF;  // RESERVED for M1Spatial-4
        } else if (ctx->audio_config.num_channels == 8) {
            loudspeaker_layout = 0xFF;  // RESERVED for M1Spatial-8  
        } else if (ctx->audio_config.num_channels == 14) {
            loudspeaker_layout = 0xFF;  // RESERVED for M1Spatial-14
        }
        
        fputc(loudspeaker_layout, internal_ctx->output_file);  // loudspeaker_layout
        fputc(0x00, internal_ctx->output_file);      // output_gain_is_present_flag = 0
        fputc(0x00, internal_ctx->output_file);      // recon_gain_is_present_flag = 0
        fputc(0x00, internal_ctx->output_file);      // reserved
        fputc(ctx->audio_config.num_channels, internal_ctx->output_file);  // substream_count
        fputc(0x00, internal_ctx->output_file);      // coupled_substream_count = 0
        
        std::cout << "Channel-based audio element configured for " << ctx->audio_config.num_channels << "-channel layout" << std::endl;
    }
    
    // 4. Mix Presentation OBU (type 2) - Simplified
    obu_header = (2 << 3) | 0x02;  // type=2, has_size_flag=1
    fputc(obu_header, internal_ctx->output_file);
    
    // Calculate exact mix presentation size
    // mix_presentation_id (1) + count_label (1) + lang_label_len (1) + annotations (1) + 
    // num_sub_mixes (1) + num_audio_elements (1) + audio_element_id (1) + annotations (1) +
    // rendering_mode (1) + mix_gain_flag (1) + output_gain_flag (1) + num_layouts (1) +
    // layout_type (1) + sound_system (3) + reserved (1) + loudness_info_type (1) + 
    // integrated_loudness (2) + digital_peak (2) = 22 bytes
    size_t mix_presentation_size = 22;
    write_uleb128(internal_ctx->output_file, mix_presentation_size);
    
    // Mix presentation payload
    write_uleb128(internal_ctx->output_file, 1);     // mix_presentation_id = 1
    write_uleb128(internal_ctx->output_file, 1);     // count_label = 1
    
    // Language label (empty)
    write_uleb128(internal_ctx->output_file, 0);     // language_label length = 0
    
    // Mix presentation annotations
    write_uleb128(internal_ctx->output_file, 0);     // No annotations
    
    write_uleb128(internal_ctx->output_file, 1);     // num_sub_mixes = 1
    
    // Sub-mix (simplified - no parameters)
    write_uleb128(internal_ctx->output_file, 1);     // num_audio_elements = 1
    write_uleb128(internal_ctx->output_file, ctx->element_config.audio_element_id);  // audio_element_id
    write_uleb128(internal_ctx->output_file, 0);     // No annotations
    
    // Rendering config
    fputc(0x00, internal_ctx->output_file);          // headphones_rendering_mode = STEREO
    
    // No element mix config (set to 0)
    write_uleb128(internal_ctx->output_file, 0);     // No mix gain parameter
    
    // No output mix config  
    write_uleb128(internal_ctx->output_file, 0);     // No output mix gain parameter
    
    // Layout configurations
    write_uleb128(internal_ctx->output_file, 1);     // num_layouts = 1
    
    // Layout - choose appropriate layout type and sound system
    if (is_scene_based) {
        // For ambisonics, use binaural layout type
        fputc(0x03, internal_ctx->output_file);      // layout_type = BINAURAL
        fputc(0x00, internal_ctx->output_file);      // reserved (6 bits for binaural)
        fputc(0x00, internal_ctx->output_file);      // reserved  
    } else {
        // For channel-based, use loudspeaker convention
        fputc(0x02, internal_ctx->output_file);      // layout_type = LOUDSPEAKERS_SS_CONVENTION
        
        // Choose sound system based on channel count
        uint32_t sound_system = 0x020200;  // Default: A_0_2_0 (stereo)
        
        if (ctx->audio_config.num_channels == 2) {
            sound_system = 0x020200;  // A_0_2_0 (stereo)
        } else if (ctx->audio_config.num_channels == 6) {
            sound_system = 0x050300;  // A_0_5_1 (5.1)  
        } else if (ctx->audio_config.num_channels == 4) {
            sound_system = 0x020200;  // Default to stereo for M1Spatial-4 
        } else if (ctx->audio_config.num_channels == 8) {
            sound_system = 0x050300;  // Default to 5.1 for M1Spatial-8
        } else if (ctx->audio_config.num_channels == 14) {
            sound_system = 0x050300;  // Default to 5.1 for M1Spatial-14 
        }
        
        write_uleb128(internal_ctx->output_file, sound_system);
        fputc(0x00, internal_ctx->output_file);      // reserved
    }
    
    // Loudness info
    write_uleb128(internal_ctx->output_file, 0);     // No info_type_bit_masks
    
    // Integrated loudness (-23 LUFS in Q7.8 format)
    int16_t integrated_loudness = (int16_t)(-23.0f * 256);
    fputc(integrated_loudness & 0xFF, internal_ctx->output_file);
    fputc((integrated_loudness >> 8) & 0xFF, internal_ctx->output_file);
    
    // Digital peak (-1 dBFS in Q7.8 format)
    int16_t digital_peak = (int16_t)(-1.0f * 256);
    fputc(digital_peak & 0xFF, internal_ctx->output_file);
    fputc((digital_peak >> 8) & 0xFF, internal_ctx->output_file);
    
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
        return -2;
    }
    
    // Calculate frame size
    size_t sample_size = ctx->audio_config.bit_depth / 8;
    size_t frame_size = num_samples * ctx->audio_config.num_channels * sample_size;
    
    // Write audio frame OBU (type 5, 0x05) - CORRECTED TO OFFICIAL IAMF SPEC
    uint8_t obu_header = (5 << 3) | 0x02;  // type=5, has_size_flag=1
    fputc(obu_header, internal_ctx->output_file);
    
    // Calculate OBU payload size: 1 byte for audio_element_id + frame_size
    size_t obu_payload_size = 1 + frame_size;  // audio_element_id (ULEB128, 1 byte for 0) + audio_data
    write_uleb128(internal_ctx->output_file, obu_payload_size);
    
    // Audio frame payload
    write_uleb128(internal_ctx->output_file, 0);  // audio_element_id = 0
    
    // Write audio data
    if (fwrite(audio_data, frame_size, 1, internal_ctx->output_file) != 1) {
        return -3;
    }
    
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
}

// Include the rest of the helper functions from the original implementation
int mach1_to_iamf_element_config(const char* mach1_format, 
                                 IAMFAudioElementConfig* element_config)
{
    if (!mach1_format || !element_config) {
        return -1;
    }
    
    memset(element_config, 0, sizeof(IAMFAudioElementConfig));
    
    // Mach1 Spatial formats (channel-based)
    if (strcmp(mach1_format, "M1Spatial-4") == 0 || strcmp(mach1_format, "M1Horizon") == 0) {
        element_config->audio_element_id = 1;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 4;
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else if (strcmp(mach1_format, "M1Spatial-8") == 0 || strcmp(mach1_format, "M1Spatial") == 0) {
        element_config->audio_element_id = 2;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 8;
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else if (strcmp(mach1_format, "M1Spatial-14") == 0) {
        element_config->audio_element_id = 3;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 14;
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    // 3rd Order Ambisonics formats (scene-based)
    else if (strcmp(mach1_format, "ACNSN3DO3A") == 0 || strcmp(mach1_format, "ACNSN3DmaxRE3oa") == 0) {
        element_config->audio_element_id = 10;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 16; // 3rd order = (3+1)^2 = 16 channels
        element_config->is_scene_based = true;
        element_config->ambisonics_mode = 1; // AMBISONICS_PROJECTION
    }
    // 2nd Order Ambisonics formats (scene-based)
    else if (strcmp(mach1_format, "ACNSN3DO2A") == 0 || strcmp(mach1_format, "ACNSN3DmaxRE2oa") == 0) {
        element_config->audio_element_id = 11;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 9; // 2nd order = (2+1)^2 = 9 channels
        element_config->is_scene_based = true;
        element_config->ambisonics_mode = 1; // AMBISONICS_PROJECTION
    }
    // 1st Order Ambisonics formats (scene-based)
    else if (strcmp(mach1_format, "ACNSN3DmaxRE1oa") == 0 || strcmp(mach1_format, "ACNSN3DYorkBasic1oa") == 0 || 
             strcmp(mach1_format, "ACNSN3DYorkmaxRE1oa") == 0 || strcmp(mach1_format, "ACNSN3D") == 0) {
        element_config->audio_element_id = 12;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 4; // 1st order = (1+1)^2 = 4 channels
        element_config->is_scene_based = true;
        element_config->ambisonics_mode = 1; // AMBISONICS_PROJECTION
    }
    // Traditional surround formats (channel-based)
    else if (strcmp(mach1_format, "5.1") == 0) {
        element_config->audio_element_id = 5;
        element_config->num_layers = 1;
        element_config->num_channels_per_layer = 6;
        element_config->is_scene_based = false;
        element_config->ambisonics_mode = 0;
    }
    else {
        return -2; // Unsupported format
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
    
    audio_config->sample_rate = 48000;
    audio_config->bit_depth = 16;
    audio_config->frame_duration_ms = 10;
    
    mix_config->mix_presentation_id = 1;
    mix_config->num_sub_mixes = 1;
    mix_config->loudness_info_db = -23.0f;
    mix_config->enable_peak_limiter = true;
    mix_config->peak_threshold_db = -1.0f;
    
    int result = mach1_to_iamf_element_config(output_format, element_config);
    if (result != 0) {
        return result;
    }
    
    // Set channel count based on format
    if (strcmp(output_format, "M1Spatial-4") == 0 || strcmp(output_format, "M1Horizon") == 0) {
        audio_config->num_channels = 4;
    }
    else if (strcmp(output_format, "M1Spatial-8") == 0 || strcmp(output_format, "M1Spatial") == 0) {
        audio_config->num_channels = 8;
    }
    else if (strcmp(output_format, "M1Spatial-14") == 0) {
        audio_config->num_channels = 14;
    }
    else if (strcmp(output_format, "ACNSN3DO3A") == 0 || strcmp(output_format, "ACNSN3DmaxRE3oa") == 0) {
        audio_config->num_channels = 16; // 3rd order ambisonics
    }
    else if (strcmp(output_format, "ACNSN3DO2A") == 0 || strcmp(output_format, "ACNSN3DmaxRE2oa") == 0) {
        audio_config->num_channels = 9; // 2nd order ambisonics
    }
    else if (strcmp(output_format, "ACNSN3DmaxRE1oa") == 0 || strcmp(output_format, "ACNSN3DYorkBasic1oa") == 0 || 
             strcmp(output_format, "ACNSN3DYorkmaxRE1oa") == 0 || strcmp(output_format, "ACNSN3D") == 0) {
        audio_config->num_channels = 4; // 1st order ambisonics
    }
    else if (strcmp(output_format, "5.1") == 0) {
        audio_config->num_channels = 6;
    }
    else {
        audio_config->num_channels = 2; // Default stereo fallback
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
    
    IAMFAudioConfig audio_config;
    IAMFMixPresentationConfig mix_config;
    IAMFAudioElementConfig element_config;
    
    int result = get_recommended_iamf_config(output_format, &audio_config, &mix_config, &element_config);
    if (result != 0) {
        return result;
    }
    
    IAMFEclipsaContext iamf_ctx;
    result = iamf_eclipsa_init(&iamf_ctx, &audio_config, &mix_config, &element_config);
    if (result != 0) {
        return result;
    }
    
    result = iamf_eclipsa_write_header(&iamf_ctx, output_iamf_file);
    if (result != 0) {
        iamf_eclipsa_cleanup(&iamf_ctx);
        return result;
    }
    
    // Load and process input audio file
    SndfileHandle infile(input_file);
    if (infile.error() != 0) {
        iamf_eclipsa_cleanup(&iamf_ctx);
        return -10;
    }
    
    std::cout << "Processing audio file:" << std::endl;
    std::cout << "  Channels: " << infile.channels() << std::endl;
    std::cout << "  Sample Rate: " << infile.samplerate() << " Hz" << std::endl;
    std::cout << "  Frames: " << infile.frames() << std::endl;
    
    // Simple pass-through for now (no transcoding in this test)
    const int BUFFERLEN = 1024;
    std::vector<int16_t> buffer(infile.channels() * BUFFERLEN);
    
    sf_count_t total_frames = 0;
    sf_count_t frames_read;
    
    while ((frames_read = infile.read(buffer.data(), buffer.size())) > 0) {
        sf_count_t samples_per_channel = frames_read / infile.channels();
        
        result = iamf_eclipsa_encode_frame(&iamf_ctx, buffer.data(), samples_per_channel, nullptr);
        if (result != 0) {
            iamf_eclipsa_cleanup(&iamf_ctx);
            return result;
        }
        
        total_frames += samples_per_channel;
    }
    
    std::cout << "Audio processing completed. Total frames: " << total_frames << std::endl;
    
    result = iamf_eclipsa_finalize(&iamf_ctx);
    iamf_eclipsa_cleanup(&iamf_ctx);
    
    std::cout << "Mach1 to IAMF workflow completed" << std::endl;
    return result;
}

#endif // HAVE_LIBIAMF 