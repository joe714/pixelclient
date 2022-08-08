#define INCLUDE_vTaskDelay 1

#include <iterator>
#include <string>

#include "driver/gpio.h"
#include "rom/gpio.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"

#include <NetManager.h>
#include <HttpServer.h>

#include <Animation.h>
#include "Config.h"
#include "RootPage.h"
#include "Display.h"
#include "WebPImage.h"
#include "NvsFlash.h"

static const char* TAG = "PIXEL";
static const size_t CAPACITY = 68*1024;
static uint8_t imgbuf[CAPACITY];

extern const uint8_t nyan_webp_start[] asm("_binary_nyan_64x32_webp_start");
extern const uint8_t nyan_webp_end[] asm("_binary_nyan_64x32_webp_end");

StaticImageSource NyanWebPSrc(nyan_webp_start, nyan_webp_end);

const gpio_num_t LED_BUILTIN = GPIO_NUM_12;

std::unique_ptr<NetManager> netMgr;
std::unique_ptr<httpd::Server> httpServer;

// Simple fixed allocation buffer with a used size we can append chunks to as
// they come in from the network.
class ImageBuffer {
    public:
        ImageBuffer() : buf_(imgbuf) {};
        bool append(const uint8_t* buf, size_t len)
        {
            if (len && !overrun_) {
                if (size_ + len < CAPACITY) {
                    memcpy(buf_ + size_, buf, len);
                    size_ += len;
                } else {
                    overrun_ = true;
                }
            }
            return !overrun_;
        }

        void reset() {
            size_ = 0;
            overrun_ = false;
        }

        const uint8_t* begin() const { return buf_; }
        const uint8_t* end() const { return overrun_ ? buf_ : buf_ + size_; }
        size_t size() const { return size_; }

    private:
        uint8_t* buf_;
        size_t size_{0};
        bool overrun_{false};
};

struct user_ctx {
    ImageBuffer buffer;
    uint32_t delay = 10000;
    bool valid{true};

    void reset() {
        buffer.reset();
        valid = true;
    }
};

void wsEventHandler(void *args, esp_event_base_t base, int32_t eventId, void* eventData)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t*)eventData;
    user_ctx* ctx = (user_ctx*)data->user_context;

    switch (eventId) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Websocket connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Websocket disconnected");
            break;
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(TAG,
                     "WS Recv op_code: %X, fin: %d, payload_len: %u, "
                     "payload_offset: %u, data_len: %u",
                     data->op_code, data->fin, data->payload_len,
                     data->payload_offset, data->data_len);
            switch (data->op_code) {
                case 0x1: // Text
                        ctx->valid = false;
                        break;
                case 0x2: // BINARY
                    if (!data->payload_offset) {
                        // This stops the current animation on the current
                        // frame, and releases the underlying WebP buffers
                        // so we have as much RAM as possible to load the
                        // incoming animation into.
                        animationStart(nullptr);
                        ctx->reset();
                    }
                    /* fallthrough */
                case 0x0: // Continue
                    if (!ctx->valid) {
                        ESP_LOGE(TAG, "No buffer for incoming WS data");
                        return;
                    }
                    if (!ctx->buffer.append((uint8_t*)data->data_ptr, data->data_len)) {
                        ESP_LOGE(
                            TAG,
                            "Image buffer overrun for %u bytes at offset %u",
                            data->data_len, ctx->buffer.size());
                        ctx->valid = false;
                    } 
                    if (ctx->valid && data->fin) {
                        // This is really a hack to prove not reallocating the
                        // incoming buffer every time and should be cleaned up.
                        std::unique_ptr<Image> img =
                            std::make_unique<WebPImage>(
                                std::make_unique<StaticImageSource>(
                                    ctx->buffer.begin(), ctx->buffer.end()));
                        if (img->valid()) {
                            animationStart(std::move(img));
                        }
                        ctx->valid = false;
                    }
                    break;
                case 0x9: // Ping
                case 0xA: // Pong
                    break;
                default:
                    ESP_LOGE(TAG, "Invalid frame opcode %X", data->op_code);
                    // If we're in the middle of a payload, it's likely garbage now.
                    ctx->reset();
                    break;
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGI(TAG, "Websocket error");
            break;
    }
}

extern "C" {
void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    animationSetup();

    auto nyan = std::make_unique<WebPImage>(
        std::make_unique<StaticImageSource>(NyanWebPSrc));
    if (nyan->valid()) {
        animationStart(std::move(nyan));
    }


    gpio_pad_select_gpio(LED_BUILTIN);
    gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_BUILTIN, 0);

    netMgr = std::make_unique<NetManager>();
    netMgr->start();

    ESP_LOGI(TAG, "WiFi up");

    // Note - Config::instance() depends on WiFi being initialized already for the RNG.
    const auto uri = Config::instance().imageUri();

    RootPage root;

    httpServer = std::make_unique<httpd::Server>();
    httpServer->registerHandler("/", root);

    gpio_set_level(LED_BUILTIN, 1);
    user_ctx *ctx = new user_ctx();
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate WS Context");
    }

    esp_websocket_client_config_t wsCfg = {};
    wsCfg.uri = uri.c_str();
    wsCfg.user_context = ctx;
    wsCfg.buffer_size = 1024;

    ESP_LOGI(TAG, "Websocket Connect %s", uri.c_str());
    esp_websocket_client_handle_t client = esp_websocket_client_init(&wsCfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, wsEventHandler,
                                  client);
    esp_websocket_client_start(client);

    // This method must not return!
    for (;;) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

} // extern "C"
