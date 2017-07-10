#!/bin/sh

cp ../libs/armeabi/libavfmedia.so ../../../../transee/prebuild/libavf/android/
cp ../libs/armeabi/libvdbnative.so ../../../../transee/prebuild/libavf/android/
cp ../../include/avf_std_media.h ../../../../transee/prebuild/libavf/android/
cp ../../source/vdb/vdb_native.h ../../../../transee/prebuild/libavf/android/
cd ../../../../transee/prebuild/libavf/android/