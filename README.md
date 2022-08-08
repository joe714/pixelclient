PixelClient: Firmware for an ESP-32 board to drive a 32x64 HUB 75 display,
by subscribing to a websocket and streaming pushed images. The corresponding
server is located at https://github.com/joe714/pixelgw

Mainter: Joe Sunday sunday@csh.rit.edu
GitHub: https://github.com/joe714/pixelfirmware

# Build and Install

Pinout for the display is set in components/Hub75Display/Hub75Display.c,
note this is not the same pinout as official Tidbyt hardware and needs
to be set for particular devices. (TODO, make this more configurable at
build time) . Reference schematic / board coming soon.

To build, you'll need a working Linux machine with Docker installed and
your user in the docker group. After cloning the repository, fetch the
required submodules:

    $ git submodule init
    $ git submodule update

To compile the firmware:

    $ ./build.sh build

For first setup, copy nvs.csv.sample to nvs.csv and set your WiFi credentials
and endpoint / pixelgw IP address on the appropriate lines, and generate the
initial NVS partion data:

    $ ./build.sh nvs

To flash, connect the device via USB and flash everything. Note that you
should only flash the NVS data on the first install. Once the device has
registered with the gateway, reflashing NVS will cause the device UUID to
change.

    $ ./build.sh flash
    $ ./build.sh flash_nvs

To do OTA updates once already installed, start a temporary web server:
 
    $ ./build.sh ota-server

Then navigate to http://*device-ip*/, verify OTA URL listed, and click "Update Firmware".
Currently it assumes you're running the OTA update from the same machine as the
gateway endpoint, otherwise you'll need to change the IP in the firmware URL.

# FAQ
- What hardware does this need?

  An ESP-32 board with the HUB75 display wired according to the pin
  definitions in components/Hub75Display/Hub75Display.c.

  I'm working on a V1.5 board revision, but I'll publish the
  v1 board design when I can.

- Do you sell hardware?

  Not currently, but inquire and if there's demand I'll see what I can do.
  
- Can I run this on Tidbyt Hardware?

  Theoretically, yes but I have not tried it or determined the right
  compile options, or determined how to reset it back to factory firmware.
  At a minimum, the pin definitions need to be tweaked. If you want to try,
  feel free and give me a pull request with changes and instructions, but
  I take no responsibility if you brick it.

# TODO
- Integrate firmware management into the server.
- Web interface to bootstrap / modify configuration
- More device support (including pre-canned config for Tidbyt 1 devices)


