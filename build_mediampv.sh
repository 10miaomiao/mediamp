#!/bin/bash
set -e
MPV_INSTALL="D:/MyCode/AndroidStudioProjects/mediamp/mediamp-mpv/build/mpv/WindowsX64/install"
SRC_DIR="D:/MyCode/AndroidStudioProjects/mediamp/mediamp-mpv/src/cpp"
OUT_DLL="D:/MyCode/AndroidStudioProjects/mediamp/mediamp-mpv/build/mpv/WindowsX64/jni/mediampv.dll"

# Find JAVA_HOME - try to locate jni.h
JNI_H=""
for jh in "D:/Program Files/jdk-17.0.2" "D:/Program Files/Java/jdk-17.0.2" "/c/Program Files/jdk-17.0.2"; do
    if [ -f "$jh/include/jni.h" ]; then
        JNI_H="$jh/include"
        JNI_H_WIN="$jh/include/win32"
        break
    fi
done

if [ -z "$JNI_H" ]; then
    echo "ERROR: jni.h not found"
    exit 1
fi

# ANGLE includes and libs (from MSYS2 UCRT64)
ANGLE_INCLUDE="/ucrt64/include"
ANGLE_LIB="/ucrt64/lib"

echo "JNI include: $JNI_H"
echo "MPV install: $MPV_INSTALL"
echo "ANGLE include: $ANGLE_INCLUDE"
echo "Source: $SRC_DIR"

rm -f "$OUT_DLL"
rm -rf /tmp/mediamp_objs
mkdir -p /tmp/mediamp_objs

for f in compatible_thread event_listener jni main method_cache mpv_handle_t render_context_t angle_render_context_t window_capture_t; do
    echo "Compiling ${f}.cpp..."
    g++ -std=c++17 -D_WIN32_WINNT=0x0A00 \
        -I"$JNI_H" -I"$JNI_H_WIN" \
        -I"${SRC_DIR}/include" -I"${MPV_INSTALL}/include" \
        -I"${ANGLE_INCLUDE}" \
        -c -o "/tmp/mediamp_objs/${f}.o" "${SRC_DIR}/${f}.cpp"
done

echo "Linking..."
g++ -shared -o "$OUT_DLL" \
    /tmp/mediamp_objs/*.o \
    "${MPV_INSTALL}/lib/libmpv.dll.a" \
    -L"${ANGLE_LIB}" -lEGL -lGLESv2 \
    -lopengl32 -lgdi32 -ld3d11 -ld3d12 -ldxgi -ldxguid

echo "Done!"
ls -la "$OUT_DLL"
