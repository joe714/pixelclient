#/bin/bash
# Run the build in a docker image
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-docker-image.html#
#
# Run HTTP server for OTA update:
#   python -m http.server --directory build

ESPTOOL="python /mnt/code/esp32/esptool/esptool.py"
if [ -z "$IDF_DOCKER_IMAGE" ]; then
    IDF_DOCKER_IMAGE="espressif/idf:release-v5.2"
fi
echo "Using esp-idf image: $IDF_DOCKER_IMAGE"

if [ -z "$ESPTOOL_PORT" ] ; then
    ESPTOOL_PORT=/dev/ttyUSB0
fi
echo "Using serial port: $ESPTOOL_PORT"

DOCKER_ARGS="--rm -v ${PWD}:/project -e HOME=/tmp"

# Build we run as the user to not trash permissions in build/
DOCKER_BUILD_ARGS="${DOCKER_ARGS} -w /project -u `id -u`:`id -g`"

# Flash we just run as root to use the serial port without a bunch of permissions issues
DOCKER_FLASH_ARGS="${DOCKER_ARGS} -w /project/build --device=${ESPTOOL_PORT}"

if [ "$1" == "build" ]; then
    docker run ${DOCKER_BUILD_ARGS} ${IDF_DOCKER_IMAGE} idf.py build
elif [ $1 == "nvs" ]; then
    docker run ${DOCKER_BUILD_ARGS} ${IDF_DOCKER_IMAGE} ./nvs.sh
elif [ "$1" == "interactive" ]; then
    docker run ${DOCKER_BUILD_ARGS} -it ${IDF_DOCKER_IMAGE} 
elif [ "$1" == "flash" ]; then
    docker run ${DOCKER_FLASH_ARGS} ${IDF_DOCKER_IMAGE} \
	esptool.py -p $ESPTOOL_PORT --chip esp32 -b 460800 --before default_reset --after hard_reset \
	write_flash "@flash_args"
elif [ "$1" == "flash_app" ]; then
    docker run ${DOCKER_FLASH_ARGS} ${IDF_DOCKER_IMAGE} \
	esptool.py -p $ESPTOOL_PORT --chip esp32 -b 460800 --before default_reset --after hard_reset \
	write_flash "@flash_app_args"
elif [ "$1" == "flash_nvs" ]; then
    docker run ${DOCKER_FLASH_ARGS} ${IDF_DOCKER_IMAGE} \
	esptool.py -p $ESPTOOL_PORT --chip esp32 -b 460800 --before default_reset --after hard_reset \
	write_flash "@flash_nvs_args"
elif [ "$1" == "ota-server" ] ; then
    python -m http.server --directory build
elif [ "$1" == "monitor" ] ; then
     python -m serial.tools.miniterm --rts 0 --dtr 0 --filter direct $ESPTOOL_PORT 115200
else
    echo "Usage: $0 [build|nvs|interactive|ota-server|flash|flash-app|flash-nvs|monitor]"
    exit 1;
fi

