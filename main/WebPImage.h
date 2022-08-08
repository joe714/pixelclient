#pragma once
#include <stdint.h>
#include <inttypes.h>

#include "Animation.h"
#include "Image.h"

#include <webp/demux.h>

class WebPImage : public Image
{
public:
    WebPImage(std::unique_ptr<ImageSource>&& buf);
    virtual ~WebPImage();

    virtual bool valid() const override { return dec_; }
    virtual uint32_t render(Image::DrawFunction fn) override;

private:
    std::unique_ptr<ImageSource> data_;
    WebPAnimDecoder* dec_;

    uint32_t loop_;
    int frame_;
    int lastFrameTime_;
};

