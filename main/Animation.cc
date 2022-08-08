#include "Animation.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
//#include "esp_log.h"

#include "Hub75Display.h"

//const char TAG[] = "Anim";

struct QueueEntry {
    Image* image;
    TaskHandle_t callingTask;
};

static QueueHandle_t imageQueue;

static void
animationTask(void*)
{
    hub75DisplaySetup();
    
    std::unique_ptr<Image> image;
    auto delay = portMAX_DELAY;
    while (true) {
        QueueEntry next{};
        bool qr = xQueueReceive(imageQueue, &next, delay);
        if (qr) {
            image.reset(next.image);
            // Tell the calling task we've released the old buffer
            xTaskNotifyGive(next.callingTask);
        }
        if (image) {
            uint32_t nextFrame = image->render(&hub75DisplayDraw);
            if (nextFrame == Image::ANIMATION_END) {
                delay = portMAX_DELAY;
                image.reset();
            } else {
                delay = nextFrame / portTICK_PERIOD_MS;
            }
        }
    }
}

void animationSetup()
{
    imageQueue = xQueueCreate(1, sizeof(QueueEntry));
    xTaskCreate(animationTask, "animationTask", 4096, nullptr, 10, nullptr);
}

void animationStart(std::unique_ptr<Image>&& image)
{
    // Setup so the animation thread can send a direct to task notification 
    // back to us once it's released any buffers from the previously running
    // animation. This way we can try to maximize the amount of free RAM for
    // the incoming animation before allocating it.
    QueueEntry e{ image.get(), xTaskGetCurrentTaskHandle() };
    xTaskNotifyStateClear(e.callingTask);
    if (xQueueSendToBack(imageQueue, &e, 0)) {
        // Successfully enqueued the work item so the animation thread will
        // take ownership of the passed Image, so release our ownership of it
        // and block until we get the signal from the Animation thread
        // any previous buffers are freed.
        image.release();
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}
