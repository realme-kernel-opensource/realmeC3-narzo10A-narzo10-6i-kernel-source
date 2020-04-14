#!/bin/bash
if [ $# -gt 2 ]; then
        echo "  usage: toSDK.sh <kernel-built-dir> <kernel-sdk-dir>"
        exit -1
fi
SRC=${1:-${OUT}/obj/KERNEL_OBJ}
DST=${2:-${OUT}}
if [ ! -d "${SRC}" ]; then
        echo ${SRC} not find
        exit -1
fi
if [ ! -d "${DST}" ]; then
        echo ${DST} not find
        exit -1
fi

DST_SDK=$DST/hotfix.sdk
rm -rf ${DST_SDK}
mkdir -p ${DST_SDK}

if [ ! -d "${DST_SDK}" ]; then
        echo create ${DST_SDK} error
        exit -1
fi

rsync -aHS --exclude="*.o" --exclude="*.su" --exclude="*.cmd" --exclude=".tmp*" --exclude="*.tmp" ${SRC}/ ${DST_SDK}/
rm -rf ${DST_SDK}/"source"
mkdir -p ${DST_SDK}/"source"
rsync -aHS --exclude=".git" --exclude="*.c" --exclude="*.S" ${SRC}/"source"/ ${DST_SDK}/"source"/

tar zcf $DST/hotfix.sdk.tar.gz -C $DST hotfix.sdk/
rm -rf ${DST_SDK}
