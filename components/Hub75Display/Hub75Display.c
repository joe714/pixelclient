// Copyright 2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "Hub75Display.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "i2s_parallel.h"
#include "val2pwm.h"

// This is example code to driver a p3(2121)64*32 -style RGB LED display. These
// types of displays do not have memory and need to be refreshed continuously.
// The display has 2 RGB inputs, 4 inputs to select the active line, a pixel
// clock input, a latch enable input and an output-enable input. The display can
// be seen as 2 64x16 displays consisting of the upper half and the lower half
// of the display. Each half has a separate RGB pixel input, the rest of the
// inputs are shared.
//
// Each display half can only show one line of RGB pixels at a time: to do this,
// the RGB data for the line is input by setting the RGB input pins to the
// desired value for the first pixel, giving the display a clock pulse, setting
// the RGB input pins to the desired value for the second pixel, giving a clock
// pulse, etc. Do this 64 times to clock in an entire row. The pixels will not
// be displayed yet: until the latch input is made high, the display will still
// send out the previously clocked in line. Pulsing the latch input high will
// replace the displayed data with the data just clocked in.
//
// The 4 line select inputs select where the currently active line is displayed:
// when provided with a binary number (0-15), the latched pixel data will
// immediately appear on this line. Note: While clocking in data for a line, the
// *previous* line is still displayed, and these lines should be set to the value
// to reflect the position the *previous* line is supposed to be on.
//
// Finally, the screen has an OE input, which is used to disable the LEDs when
// latching new data and changing the state of the line select inputs: doing so
// hides any artifacts that appear at this time. The OE line is also used to dim
// the display by only turning it on for a limited time every line.
//
// All in all, an image can be displayed by 'scanning' the display, say, 100
// times per second. The slowness of the human eye hides the fact that only one
// line is showed at a time, and the display looks like every pixel is driven at
// the same time.
//
// Now, the RGB inputs for these types of displays are digital, meaning each
// red, green and blue subpixel can only be on or off. This leads to a color
// palette of 8 pixels, not enough to display nice pictures. To get around this,
// we use binary code modulation.
//
// Binary code modulation is somewhat like PWM, but easier to implement in our
// case. First, we define the time we would refresh the display without binary
// code modulation as the 'frame time'. For, say, a four-bit binary code
// modulation, the frame time is divided into 15 ticks of equal length.
//
// We also define 4 subframes (0 to 3), defining which LEDs are on and which
// LEDs are off during that subframe. (Subframes are the same as a normal frame
// in non-binary-coded-modulation mode, but are showed faster.)  From our
// (non-monochrome) input image, we take the (8-bit: bit 7 to bit 0) RGB pixel
// values. If the pixel values have bit 7 set, we turn the corresponding LED on
// in subframe 3. If they have bit 6 set, we turn on the corresponding LED in
// subframe 2, if bit 5 is set subframe 1, if bit 4 is set in subframe 0.
//
// Now, in order to (on average within a frame) turn a LED on for the time
// specified in the pixel value in the input data, we need to weigh the
// subframes. We have 15 pixels: if we show subframe 3 for 8 of them, subframe 2
// for 4 of them, subframe 1 for 2 of them and subframe 1 for 1 of them, this
// 'automatically' happens. (We also distribute the subframes evenly over the
// ticks, which reduces flicker.)
//
//
// In this code, we use the I2S peripheral in parallel mode to achieve this.
// Essentially, first we allocate memory for all subframes. This memory contains
// a sequence of all the signals (2xRGB, line select, latch enable, output
// enable) that need to be sent to the display for that subframe. Then we ask
// the I2S-parallel driver to set up a DMA chain so the subframes are sent out
// in a sequence that satisfies the requirement that subframe x has to be sent
// out for (2^x) ticks. Finally, we fill the subframes with image data.
//
// We use a frontbuffer/backbuffer technique here to make sure the display is
// refreshed in one go and drawing artifacts do not reach the display. In
// practice, for small displays this is not really necessarily.
//
// Finally, the binary code modulated intensity of a LED does not correspond to
// the intensity as seen by human eyes. To correct for that, a luminance
// correction is used. See val2pwm.c for more info.
//
// Note: Because every subframe contains one bit of grayscale information, they
// are also referred to as 'bitplanes' by the code below.

#define matrixHeight 32
#define matrixWidth 64
#define matrixRowsInParallel 2

#define ESP32_NUM_FRAME_BUFFERS 2
#define ESP32_OE_OFF_CLKS_AFTER_LATCH 1

// This is the bit depth, per RGB subpixel, of the data that is sent to the
// display. The effective bit depth (in computer pixel terms) is less because of
// the PWM correction. With a bitplane count of 7, you should be able to
// reproduce an 16-bit image more or less faithfully, though.
#define BITPLANE_CNT 7

// LSBMSB_TRANSITION_BIT defines the color bit that is refreshed once per frame,
// with the same brightness as the bits above it when LSBMSB_TRANSITION_BIT is
// non-zero, all color bits below it will be be refreshed only once, with
// fractional brightness, saving RAM and speeding up refresh LSBMSB_TRANSITION_BIT
// must be < BITPLANE_CNT
#define LSBMSB_TRANSITION_BIT 1

// 64*32 RGB leds, 2 pixels per 16-bit value...
#define BITPLANE_SZ (matrixWidth * matrixHeight / matrixRowsInParallel)

// I2S Data Position Definitions
// Upper half RGB
#define BIT_R1 (1 << 0)
#define BIT_G1 (1 << 1)
#define BIT_B1 (1 << 2)
// Lower half RGB
#define BIT_R2 (1 << 3)
#define BIT_G2 (1 << 4)
#define BIT_B2 (1 << 5)

#define BIT_A (1 << 8)
#define BIT_B (1 << 9)
#define BIT_C (1 << 10)
#define BIT_D (1 << 11)
#define BIT_LAT (1 << 12)
#define BIT_OE (1 << 13)

// Pin Definitions
#define R1_PIN 21
#define G1_PIN 19
#define B1_PIN 18
#define R2_PIN 5
#define G2_PIN 17
#define B2_PIN 16

#define A_PIN 4
#define B_PIN 2
#define C_PIN 25
#define D_PIN 15
#define LAT_PIN 26
#define OE_PIN 27

#define CLK_PIN 22

// note: sizeof(data) must be multiple of 32 bits, as DMA linked list buffer
// address pointer must be word-aligned.
typedef struct rowBitStruct {
    uint16_t data[matrixWidth];
} rowBitStruct;

typedef struct rowDataStruct {
    rowBitStruct rowbits[BITPLANE_CNT];
} rowDataStruct;

typedef struct frameStruct {
    rowDataStruct rowdata[matrixHeight / matrixRowsInParallel];
} frameStruct;

// Get a pixel from the image at pix, assuming the image is a matrixWidth x
// matrixHeight 8R8G8B8A image. Returns it as an uint32 with the lower 24
// bits containing the RGB values.
static uint32_t
getpixel(const uint8_t* pix, int x, int y)
{
    int idx  = (y * matrixWidth * 4) + (x * 4);
    return ((uint32_t)valToPwm(pix[idx]) << 16) |
           ((uint32_t)valToPwm(pix[idx + 1]) << 8) |
           ((uint32_t)valToPwm(pix[idx + 2]) << 0);
}

int brightness = 16; // Change to set the global brightness of the display,
                     // range 1-matrixWidth Warning when set too high: Do not
                     // look into LEDs with remaining eye.

// TODO: find more efficient way of signaling the end of the buffer description
// than adding two extra rows
i2s_parallel_buffer_desc_t bufdesc[2][matrixHeight / matrixRowsInParallel + 1]
                                  [1 << (BITPLANE_CNT - LSBMSB_TRANSITION_BIT - 1)];

i2s_parallel_config_t cfg = {
    .gpio_bus = { R1_PIN,
                  G1_PIN,
                  B1_PIN,
                  R2_PIN,
                  G2_PIN,
                  B2_PIN,
                  -1,
                  -1,
                  A_PIN,
                  B_PIN,
                  C_PIN,
                  D_PIN,
                  LAT_PIN,
                  OE_PIN,
                  -1,
                  -1 },
    .gpio_clk = CLK_PIN,
    .clkspeed_hz = 20 * 1000 * 1000,
    .bits = I2S_PARALLEL_BITS_16,
    .bufa = bufdesc[0][0],
    .bufb = bufdesc[1][0],
};

// pixel data is organized from LSB to MSB sequentially by row, from row 0 to row
// matrixHeight/matrixRowsInParallel (two rows of pixels are refreshed in parallel)
frameStruct* bitplane;

void
hub75DisplaySetup()
{
    bitplane = (frameStruct*)heap_caps_malloc(sizeof(frameStruct) *
                                                  ESP32_NUM_FRAME_BUFFERS,
                                              MALLOC_CAP_DMA);
    assert("Can't allocate bitplane memory");

    for (int j = 0; j < matrixHeight / matrixRowsInParallel; j++) {
        // first set of data is LSB through MSB, single pass - all color bits
        // are displayed once, which takes care of everything below and
        // inlcluding LSBMSB_TRANSITION_BIT
        bufdesc[0][j][0].memory = bitplane[0].rowdata[j].rowbits[0].data;
        bufdesc[0][j][0].size = sizeof(rowBitStruct) * BITPLANE_CNT;
        bufdesc[1][j][0].memory = bitplane[1].rowdata[j].rowbits[0].data;
        bufdesc[1][j][0].size = sizeof(rowBitStruct) * BITPLANE_CNT;

        int nextBufdescIndex = 1;

        for (int i = LSBMSB_TRANSITION_BIT + 1; i < BITPLANE_CNT; i++) {
            // binary time division setup: we need 2 of bit
            // (LSBMSB_TRANSITION_BIT + 1) four of (LSBMSB_TRANSITION_BIT + 2),
            // etc because we sweep through to MSB each time, it divides the
            // number of times we have to sweep in half (saving linked list RAM)
            // we need 2^(i - LSBMSB_TRANSITION_BIT - 1) == 1 << (i -
            // LSBMSB_TRANSITION_BIT - 1) passes from i to MSB
            // printf("buffer %d: repeat %d times, size: %d, from %d - %d\r\n",
            // nextBufdescIndex, 1<<(i - LSBMSB_TRANSITION_BIT - 1),
            // (BITPLANE_CNT - i), i, BITPLANE_CNT-1);

            for (int k = 0; k < 1 << (i - LSBMSB_TRANSITION_BIT - 1); k++) {
                bufdesc[0][j][nextBufdescIndex].memory =
                    bitplane[0].rowdata[j].rowbits[i].data;
                bufdesc[0][j][nextBufdescIndex].size =
                    sizeof(rowBitStruct) * (BITPLANE_CNT - i);
                bufdesc[1][j][nextBufdescIndex].memory =
                    bitplane[1].rowdata[j].rowbits[i].data;
                bufdesc[1][j][nextBufdescIndex].size =
                    sizeof(rowBitStruct) * (BITPLANE_CNT - i);
                nextBufdescIndex++;
                // printf("i %d, j %d, k %d\r\n", i, j, k);
            }
        }
    }

    // End markers
    bufdesc[0][matrixHeight / matrixRowsInParallel][0].memory = NULL;
    bufdesc[1][matrixHeight / matrixRowsInParallel][0].memory = NULL;

    // Setup I2S
    i2s_parallel_setup(&I2S1, &cfg);
}

void
hub75DisplayDraw(const uint8_t* pix)
{
    static int backbuf_id = 0; // which buffer is the backbuffer, as in, which
                               // one is not active so we can write to it

    for (unsigned int y = 0; y < matrixHeight / matrixRowsInParallel; y++) {
        for (int pl = 0; pl < BITPLANE_CNT; pl++) {
            uint16_t* p =
                bitplane[backbuf_id].rowdata[y].rowbits[pl].data; // bitplane
                                                                  // location to
                                                                  // write to
            int mask =
                (1 << (8 - BITPLANE_CNT + pl)); // bitmask for pixel data in
                                                // input for this bitplane
            int lbits = 0; // Precalculate line bits of the current line, which
                           // is the one we're displaying now
            int rowAddress = y;

            // normally output current rows ADDX, special case for LSB, output
            // previous row's ADDX (as previous row is being displayed for one
            // latch cycle)
            if (pl == 0)
                rowAddress = y - 1;

            if (rowAddress & 1)
                lbits |= BIT_A;
            if (rowAddress & 2)
                lbits |= BIT_B;
            if (rowAddress & 4)
                lbits |= BIT_C;
            if (rowAddress & 8)
                lbits |= BIT_D;
            for (int fx = 0; fx < matrixWidth; fx++) {
                int x = fx;
                int v = lbits;
                // Do not show image while the line bits are changing
                if (fx < ESP32_OE_OFF_CLKS_AFTER_LATCH || fx >= (matrixWidth - 1))
                    v |= BIT_OE;

                // turn off OE after brightness value is reached - used for MSBs
                // and LSB MSBs always output normal brightness LSB outputs
                // normal brightness as MSB from previous row is being displayed
                if ((pl > LSBMSB_TRANSITION_BIT || !pl) &&
                    fx > brightness + ESP32_OE_OFF_CLKS_AFTER_LATCH)
                    v |= BIT_OE;

                // special case for the bits *after* LSB through
                // (LSBMSB_TRANSITION_BIT) - OE is output after data is shifted,
                // so need to set OE to fractional brightness
                if (pl && pl <= LSBMSB_TRANSITION_BIT) {
                    // divide brightness in half for each bit below
                    // LSBMSB_TRANSITION_BIT
                    int lsbBrightness =
                        brightness >> (LSBMSB_TRANSITION_BIT - pl + 1);
                    if (fx > lsbBrightness + ESP32_OE_OFF_CLKS_AFTER_LATCH)
                        v |= BIT_OE;
                }

                if (fx == matrixWidth - 1)
                    v |= BIT_LAT; // latch on last bit

                int c1, c2;
                c1 = getpixel(pix, x, y);
                c2 = getpixel(pix, x, y + (matrixHeight / 2));

                if (c1 & (mask << 16))
                    v |= BIT_R1;
                if (c1 & (mask << 8))
                    v |= BIT_G1;
                if (c1 & (mask << 0))
                    v |= BIT_B1;
                if (c2 & (mask << 16))
                    v |= BIT_R2;
                if (c2 & (mask << 8))
                    v |= BIT_G2;
                if (c2 & (mask << 0))
                    v |= BIT_B2;

                // Save the calculated value to the bitplane memory in reverse
                // order to account for I2S Tx FIFO mode1 ordering
                if (fx % 2) {
                    *p = v;
                    p += 2;
                } else {
                    *(p + 1) = v;
                }
            }
        }
    }

    // Show our work!
    i2s_parallel_flip_to_buffer(&I2S1, backbuf_id);
    backbuf_id ^= 1;
}
