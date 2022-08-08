#pragma once
#include <functional>
#include <vector>

// TODO: This needs to be project-wide and pushed into the Hub75 Driver.
static const uint32_t IMAGE_WIDTH = 64;
static const uint32_t IMAGE_HEIGHT = 32;

// Abstract base class representing an image (of fixed 
// IMAGE_WIDTH * IMAGE_HEIGHT) that can be rendered.
class Image {
public:
    virtual ~Image() = default;

    virtual bool valid() const = 0;

    typedef std::function<void(const uint8_t*)> DrawFunction;
    static const uint32_t ANIMATION_END = 0xFFFFFFFF;

    // If a frame is available, render the image by passing a bitmap
    // in R8G8B8A8 format to the supplied draw function. Returns the
    // delay (in ms) until the next call to render(), or ANIMATION_END
    // if this image is complete and should be disposed of.
    virtual uint32_t render(DrawFunction fn) = 0;
};

// Abstract base class representing a bitstream of encoded image data.
class ImageSource {
public:
    virtual ~ImageSource() = default;

    virtual const uint8_t* data() const = 0;
    virtual size_t size() const = 0;
};

// Image bitstream data read directly from flash that does not require
// lifecycle management.
class StaticImageSource : public ImageSource {
public:
    constexpr StaticImageSource(const uint8_t* start, const uint8_t* end): data_(start), size_(end - start) {}
    virtual ~StaticImageSource() = default;

    virtual const uint8_t* data() const override { return data_; }
    virtual size_t size() const override { return size_; }
private:
    const uint8_t* data_;
    size_t size_;
};

// Image bitstream data on the heap that must be freed when no longer
// referenced by an image.
class HeapImageSource : public ImageSource {
public:
    HeapImageSource(size_t resv): overrun_(false) { vec_.reserve(resv); }
    virtual ~HeapImageSource() = default;

    virtual const uint8_t* data() const override { return vec_.data(); }
    virtual size_t size() const override { return vec_.size(); }
    size_t capacity() const { return vec_.capacity(); }

    bool append(const uint8_t* buf, size_t len)
    {
        if (len && !overrun_) {
            if (vec_.size() + len < vec_.capacity()) {
                vec_.insert(vec_.end(), buf, buf + len);
            } else {
                overrun_ = true;
            }
        }
        return !overrun_;
    }

    std::vector<uint8_t>& vec() { return vec_; }

private:
    bool overrun_;
    std::vector<uint8_t> vec_;
};

