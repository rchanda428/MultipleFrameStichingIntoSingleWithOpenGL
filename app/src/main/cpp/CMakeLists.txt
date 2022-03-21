cmake_minimum_required(VERSION 3.4.1)

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
include_directories(.)
add_library( lib_opencv SHARED IMPORTED)
set_target_properties(lib_opencv PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libopencv_java4.so )
add_library(gl2jni SHARED
            gl_code.cpp cl_wrapper.cpp libopencl.c util.cpp )

# add lib dependencies
target_link_libraries(gl2jni
                      android
                      jnigraphics
                      log 
                      EGL
                      GLESv2
                        lib_opencv
        )

