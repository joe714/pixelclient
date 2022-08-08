#include "WebPImage.h"
#include <cstdio>
#include "esp_log.h"
#include "esp_rom_md5.h"
#include "esp_system.h"

const char TAG[] = "WebPImage";

WebPImage::WebPImage(std::unique_ptr<ImageSource>&& buf)
    : dec_(nullptr), loop_(0), frame_(0), lastFrameTime_(0)
{
    WebPData data {
        .bytes = buf->data(),
        .size = buf->size()
    };

    dec_ = WebPAnimDecoderNew(&data, nullptr);
    if (!dec_) {
        ESP_LOGE(TAG, "Failed to create decoder");
        return;
    }

    WebPAnimInfo info;
    int rv = WebPAnimDecoderGetInfo(dec_, &info);
    ESP_LOGI(TAG,
             "WebPAnimDecoderGetInfo: %d, Frames: %" PRIu32 " Loop: %" PRIu32
             " Width: %" PRIu32 " Height: %" PRIu32,
             rv, info.frame_count, info.loop_count, info.canvas_width,
             info.canvas_height);

    if (IMAGE_WIDTH != info.canvas_width || IMAGE_HEIGHT != info.canvas_height) {
        WebPAnimDecoderDelete(dec_);
        dec_ = nullptr;
    }

    loop_ = info.loop_count;
    data_ = std::move(buf);
}

WebPImage::~WebPImage()
{
    if (dec_) {
        WebPAnimDecoderDelete(dec_);
    }
}

uint32_t
WebPImage::render(Image::DrawFunction draw)
{
    if (!valid()) {
        return ANIMATION_END;
    }

    for ( ; ; ) {
        if (WebPAnimDecoderHasMoreFrames(dec_)) {
            uint8_t* pix;
            int timestamp;

            if (!WebPAnimDecoderGetNext(dec_, &pix, &timestamp)) {
                MD5Context md5;
                unsigned char digest[16];
                char hash[33];
                esp_rom_md5_init(&md5);
                esp_rom_md5_update(&md5, (uint8_t*)data_->data(), data_->size());
                esp_rom_md5_final(digest, &md5);
                for (int i = 0; i < 16; ++i) {
                    sprintf(hash + (i*2), "%.2x", digest[i]);
                }
                ESP_LOGE(TAG,
                         "WebPAnimDecoderGetNext error on frame %d (len: %u, "
                         "md5: %s)",
                         frame_, data_->size(), hash);
                ESP_LOGE(TAG, "Free Heap: %lu, Min Free Heap: %lu",
                         esp_get_free_heap_size(),
                         esp_get_minimum_free_heap_size());
                return ANIMATION_END;
            }
            draw(pix);
            int rv = timestamp - lastFrameTime_;
            lastFrameTime_ = timestamp;
            ++frame_;
            return rv;
        }

        if (1 == loop_) {
            // Done animating
            return ANIMATION_END;
        }

        if (loop_) {
           --loop_; 
        }
        WebPAnimDecoderReset(dec_);
        frame_ = 0;
        lastFrameTime_ = 0;
    }
    return ANIMATION_END;
}

