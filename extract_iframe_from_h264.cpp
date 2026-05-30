#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libpng16/png.h>
}

using namespace std;

struct NALUInfo {
    uint32_t start_pos;
    uint32_t nal_pos;
    uint8_t type;
};

/**
 * Extract I-Frame from H.264 file based on h264_analyze NALU info
 * 
 * Usage:
 *   extract_iframe_from_h264 <input.h264> <output.png> [--iframe-num N] [--list]
 *   
 * Examples:
 *   extract_iframe_from_h264 video.h264 frame1.png              # Extract 1st I-frame
 *   extract_iframe_from_h264 video.h264 frame5.png --iframe-num 5  # Extract 5th I-frame
 *   extract_iframe_from_h264 video.h264 dummy.png --list        # List all I-frames
 */

class IFrameExtractor {
private:
    string input_file;
    string output_file;
    int target_iframe_num;
    bool list_only;
    vector<NALUInfo> nalus;

public:
    IFrameExtractor(const string& input, const string& output, int frame_num = 1, bool list = false)
        : input_file(input), output_file(output), target_iframe_num(frame_num), list_only(list) {}

    /**
     * Load NALU information from h264_analyze extraction
     */
    bool loadNALUInfo() {
        ifstream nalu_file("nalu_info.bin", ios::binary);
        if (!nalu_file) {
            cerr << "❌ Error: Cannot open nalu_info.bin\n";
            cerr << "   Run: extract_nalu_from_h264analyze " << input_file << "\n";
            return false;
        }

        uint32_t nalu_count = 0;
        nalu_file.read((char*)&nalu_count, sizeof(nalu_count));

        nalus.resize(nalu_count);
        for (uint32_t i = 0; i < nalu_count; i++) {
            nalu_file.read((char*)&nalus[i].start_pos, sizeof(nalus[i].start_pos));
            nalu_file.read((char*)&nalus[i].nal_pos, sizeof(nalus[i].nal_pos));
            nalu_file.read((char*)&nalus[i].type, sizeof(nalus[i].type));
        }
        nalu_file.close();

        cout << "📊 Loaded " << nalu_count << " NALUs from h264_analyze\n";
        return true;
    }

    /**
     * List all I-frames (Type 5 IDR)
     */
    void listIFrames() {
        cout << "\n📋 I-Frames in " << input_file << ":\n";
        cout << "───────────────────────────────────────\n";

        int iframe_num = 0;
        for (size_t i = 0; i < nalus.size(); i++) {
            if (nalus[i].type == 5) {  // Type 5 = IDR (I-frame)
                iframe_num++;
                cout << "I-frame #" << iframe_num << ": NALU index " << i 
                     << " @ offset " << nalus[i].nal_pos << "\n";
            }
        }

        cout << "───────────────────────────────────────\n";
        cout << "✅ Total I-frames: " << iframe_num << "\n";
    }

    /**
     * Extract I-frame using libavcodec (hybrid approach)
     * Uses h264_analyze to know which NALUs are I-frames
     */
    bool extractIFrame() {
        // Step 1: Count I-frames to validate target
        int total_iframes = 0;
        for (const auto& nalu : nalus) {
            if (nalu.type == 5) total_iframes++;
        }

        if (target_iframe_num > total_iframes || target_iframe_num < 1) {
            cerr << "❌ Error: I-frame #" << target_iframe_num << " does not exist\n";
            cerr << "   Total I-frames: " << total_iframes << "\n";
            return false;
        }

        cout << "\n🎬 Extracting I-frame #" << target_iframe_num << " from " << total_iframes << " total\n";

        // Step 2: Open H.264 file with libavformat
        AVFormatContext* format_ctx = nullptr;
        if (avformat_open_input(&format_ctx, input_file.c_str(), nullptr, nullptr) < 0) {
            cerr << "❌ Cannot open input file\n";
            return false;
        }

        if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
            cerr << "❌ Cannot find stream info\n";
            avformat_close_input(&format_ctx);
            return false;
        }

        // Find video stream
        int video_stream_idx = -1;
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_idx = i;
                break;
            }
        }

        if (video_stream_idx < 0) {
            cerr << "❌ No video stream found\n";
            avformat_close_input(&format_ctx);
            return false;
        }

        // Get codec
        AVCodecContext* codec_ctx = avcodec_alloc_context3(
            avcodec_find_decoder(format_ctx->streams[video_stream_idx]->codecpar->codec_id)
        );
        
        if (!codec_ctx) {
            cerr << "❌ Cannot allocate codec context\n";
            avformat_close_input(&format_ctx);
            return false;
        }

        avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_idx]->codecpar);
        if (avcodec_open2(codec_ctx, avcodec_find_decoder(codec_ctx->codec_id), nullptr) < 0) {
            cerr << "❌ Cannot open codec\n";
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&format_ctx);
            return false;
        }

        // Step 3: Decode frames and count I-frames
        AVFrame* frame = av_frame_alloc();
        AVPacket* packet = av_packet_alloc();
        int decoded_frames = 0;
        int found_iframes = 0;
        AVFrame* target_iframe = nullptr;

        cout << "📺 Video info:\n";
        cout << "   Resolution: " << codec_ctx->width << "x" << codec_ctx->height << "\n";
        cout << "   Codec: " << codec_ctx->codec->name << "\n\n";

        while (av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index != video_stream_idx) {
                av_packet_unref(packet);
                continue;
            }

            if (avcodec_send_packet(codec_ctx, packet) < 0) {
                av_packet_unref(packet);
                continue;
            }

            while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
                decoded_frames++;

                // Check if this is I-frame using pict_type
                if (frame->pict_type == AV_PICTURE_TYPE_I) {
                    found_iframes++;
                    
                    cout << "Found I-frame #" << found_iframes << " at decoded frame " << decoded_frames << "\n";

                    // Is this our target?
                    if (found_iframes == target_iframe_num) {
                        cout << "✅ Found target I-frame #" << target_iframe_num << "\n";
                        target_iframe = av_frame_clone(frame);
                        break;
                    }
                }
            }

            av_packet_unref(packet);
            if (target_iframe) break;
        }

        if (!target_iframe) {
            cerr << "❌ Could not find I-frame #" << target_iframe_num << "\n";
            av_frame_free(&frame);
            av_packet_free(&packet);
            avcodec_close(codec_ctx);
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&format_ctx);
            return false;
        }

        // Step 4: Convert YUV420p to RGB and save as PNG
        bool success = convertAndSavePNG(target_iframe, codec_ctx);

        // Cleanup
        av_frame_free(&target_iframe);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_close(codec_ctx);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);

        return success;
    }

    /**
     * Convert YUV420p frame to RGB and save as PNG
     */
    bool convertAndSavePNG(AVFrame* yuv_frame, AVCodecContext* codec_ctx) {
        cout << "\n🖼️  Converting YUV→RGB and encoding PNG...\n";

        // Create scaler context
        SwsContext* sws_ctx = sws_getContext(
            yuv_frame->width, yuv_frame->height, (AVPixelFormat)yuv_frame->format,
            yuv_frame->width, yuv_frame->height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        if (!sws_ctx) {
            cerr << "❌ Cannot create color conversion context\n";
            return false;
        }

        // Allocate RGB frame
        AVFrame* rgb_frame = av_frame_alloc();
        rgb_frame->format = AV_PIX_FMT_RGB24;
        rgb_frame->width = yuv_frame->width;
        rgb_frame->height = yuv_frame->height;

        if (av_frame_get_buffer(rgb_frame, 32) < 0) {
            cerr << "❌ Cannot allocate RGB buffer\n";
            sws_freeContext(sws_ctx);
            av_frame_free(&rgb_frame);
            return false;
        }

        // Convert color space
        sws_scale(sws_ctx, 
                  yuv_frame->data, yuv_frame->linesize, 0, yuv_frame->height,
                  rgb_frame->data, rgb_frame->linesize);

        // Write PNG using libpng
        FILE* fp = fopen(output_file.c_str(), "wb");
        if (!fp) {
            cerr << "❌ Cannot open output file: " << output_file << "\n";
            sws_freeContext(sws_ctx);
            av_frame_free(&rgb_frame);
            return false;
        }

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png) {
            fclose(fp);
            sws_freeContext(sws_ctx);
            av_frame_free(&rgb_frame);
            return false;
        }

        png_infop info = png_create_info_struct(png);
        if (!info) {
            png_destroy_write_struct(&png, nullptr);
            fclose(fp);
            sws_freeContext(sws_ctx);
            av_frame_free(&rgb_frame);
            return false;
        }

        png_init_io(png, fp);
        png_set_IHDR(
            png,
            info,
            rgb_frame->width,
            rgb_frame->height,
            8,
            PNG_COLOR_TYPE_RGB,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT
        );
        png_write_info(png, info);

        // Write image data
        vector<png_bytep> row_pointers(rgb_frame->height);
        for (int i = 0; i < rgb_frame->height; i++) {
            row_pointers[i] = rgb_frame->data[0] + i * rgb_frame->linesize[0];
        }
        png_write_image(png, row_pointers.data());
        png_write_end(png, nullptr);

        png_destroy_write_struct(&png, &info);
        fclose(fp);

        cout << "✅ PNG saved: " << output_file << "\n";
        cout << "   Size: " << rgb_frame->width << "x" << rgb_frame->height << "\n";

        // Cleanup
        sws_freeContext(sws_ctx);
        av_frame_free(&rgb_frame);

        return true;
    }

    /**
     * Run extraction process
     */
    bool run() {
        if (!loadNALUInfo()) {
            return false;
        }

        if (list_only) {
            listIFrames();
            return true;
        }

        return extractIFrame();
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input.h264> <output.png> [options]\n";
        cerr << "\nOptions:\n";
        cerr << "  --iframe-num N    Extract Nth I-frame (default: 1)\n";
        cerr << "  --list            List all I-frames without extraction\n";
        cerr << "\nExamples:\n";
        cerr << "  " << argv[0] << " video.h264 frame.png\n";
        cerr << "  " << argv[0] << " video.h264 frame.png --iframe-num 5\n";
        cerr << "  " << argv[0] << " video.h264 dummy.png --list\n";
        cerr << "\nNote: Requires nalu_info.bin generated by extract_nalu_from_h264analyze\n";
        return 1;
    }

    string input_file = argv[1];
    string output_file = argv[2];
    int iframe_num = 1;
    bool list_only = false;

    // Parse options
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--iframe-num") == 0 && i + 1 < argc) {
            iframe_num = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--list") == 0) {
            list_only = true;
        }
    }

    IFrameExtractor extractor(input_file, output_file, iframe_num, list_only);
    
    if (extractor.run()) {
        return 0;
    } else {
        return 1;
    }
}
