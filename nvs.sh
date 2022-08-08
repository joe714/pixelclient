#/bin/bash
# Build script to create the an NVS partition data. Copy nvs.csv.sample to
# nvs.csv and replace the following values:
# ssid: WIFI_SSID_HERE
# password: WIFI_PASSWD_HERE
# endpoint: 192.168.1.100 ## This should be the IP of your pixelgw server
# port: 8080              ## Replace with 8081 for the test server deployment
# This is meant to be run under docker via 'build.sh nvs'.

NVS=nvs.csv
if [ ! -z $1 ] ; then
    NVS=$1
fi

if [ ! -f $NVS ] ; then
    echo file $NVS not found
    exit 1
fi

PINFO=`/opt/esp/idf/components/partition_table/parttool.py -f partitions.csv get_partition_info --partition-name nvs`
OFFSET=`echo $PINFO | awk '{print $1}'`
SIZE=`echo $PINFO | awk '{print $2}'`

/opt/esp/idf/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate \
       	$NVS build/nvs.bin $SIZE
head -1 build/flash_args > build/flash_nvs_args
echo $OFFSET nvs.bin >> build/flash_nvs_args

