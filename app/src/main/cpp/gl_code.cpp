/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include "cl_code.h"
#include "speckle_utils.h"
#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/core/ocl.hpp"

namespace abc {}

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

inline unsigned char saturate_cast_uchar(float val) {
    //val += 0.5; // to round the value
    return static_cast<unsigned char>(val < 0 ? 0 : (val > 0xff ? 0xff : val));
}



auto gVertexShader =
        "attribute vec4 aPosition;\n"
            "attribute vec2 aTexCoord;\n"
            "varying vec2 vTexCoord;\n"
            "void main() {\n"
            "   gl_Position = vec4(aPosition, 0.0, 1.0);\n"
            "   vTexCoord = aTexCoord;\n"
            "}";

auto gFragmentShader =
        "precision mediump float;\n"
            "uniform sampler2D rubyTexture;\n"
            "varying vec2 vTexCoord;\n"
            "void main() {\n"
            "   vec4 color = texture2D(rubyTexture, vTexCoord);\n"
            "   gl_FragColor = color;\n"
            "}";


GLuint loadShader(GLenum shaderType, const char *pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char *buf = (char *) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n",
                         shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }

    return shader;
}

GLuint createProgram(const char *pVertexSource, const char *pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char *buf = (char *) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

GLuint programId;
GLuint aPosition;
GLuint aTexCoord;
GLuint rubyTexture1;
GLuint rubyTexture2;
GLuint rubyTexture3;
GLuint rubyTexture4;
//GLuint lut;
GLuint rubyTextureSize;
GLuint rubyInputSize;
GLuint rubyOutputSize;

GLuint texture_map1;
//GLuint lut_map;
GLuint texture_map2;
GLuint texture_map3;
GLuint texture_map4;

GLuint iFrameBuffObject;

int scnw, scnh, vw, vh;
char *gVs, *gFs;
unsigned char * rawData = NULL;
FILE *fp = NULL;
int freadbw, freadbh;
int fread_buf_size;
bool initProgram() {
//    LOGI("initProgram vs=%s fs=%s", gVs, gFs);
    if(!gVs)
        gVs = (char *)gVertexShader;
    if(!gFs)
        gFs = (char *)gFragmentShader;
    programId = createProgram(gVs, gFs);
    if (!programId) {
        LOGE("Could not create program.");
        return false;
    }

    aPosition = glGetAttribLocation(programId, "aPosition");
    aTexCoord = glGetAttribLocation(programId, "aTexCoord");
    rubyTexture1 = glGetUniformLocation(programId, "rubyTexture1");
    rubyTexture2 = glGetUniformLocation(programId, "rubyTexture2");

    rubyTexture3 = glGetUniformLocation(programId, "rubyTexture3");
    rubyTexture4 = glGetUniformLocation(programId, "rubyTexture4");

    rubyTextureSize = glGetUniformLocation(programId, "rubyTextureSize");
    rubyInputSize = glGetUniformLocation(programId, "rubyInputSize");
    rubyOutputSize = glGetUniformLocation(programId, "rubyOutputSize");

    return true;
}

bool setupGraphics(int w, int h) {
    scnw = w;
    scnh = h;
    LOGI("setupGraphics(%d, %d)", w, h);
    glViewport(0, 0, scnw, scnh);
    return true;
}
/* for two its working
const GLfloat gTriangleVertices[] = {
        -1.0f, -1.0f,
        0.0f, -1.0f,
        -1.0f, 1.0f,

        -1.0f, 1.0f,
        0.0f,-1.0f,
        0.0f,1.0f,

        -0.0f, -1.0f,
        1.0f, -1.0f,
        -0.0f, 1.0f,

        -0.0f, 1.0f,
        1.0f,-1.0f,
        1.0f,1.0f

};

const GLfloat gTexVertices[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,

        0.0f, 0.0f,
        1.0f,1.0f,
        1.0f,0.0f,

        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,

        0.0f, 0.0f,
        1.0f,1.0f,
        1.0f,0.0f
};
*/

const GLfloat gTriangleVertices[] = {
        -1.0f, 0.0f,
        0.0f, -0.0f,
        -1.0f, 1.0f,

        -1.0f, 1.0f,
        0.0f,-0.0f,
        0.0f,1.0f,
//1st img end
        -0.0f, -0.0f,
        1.0f, -0.0f,
        -0.0f, 1.0f,

        -0.0f, 1.0f,
        1.0f,-0.0f,
        1.0f,1.0f,
//2nd img end
        -1.0f, -1.0f,
        0.0f, -1.0f,
        -1.0f, 0.0f,

        -1.0f, 0.0f,
        0.0f,-1.0f,
        0.0f,0.0f,
//3rd image end
        -0.0f, -1.0f,
        1.0f, -1.0f,
        -0.0f, 0.0f,

        -0.0f, 0.0f,
        1.0f,-1.0f,
        1.0f,0.0f
//4th image end
};

const GLfloat gTexVertices[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,

        0.0f, 0.0f,
        1.0f,1.0f,
        1.0f,0.0f,
//1st img end
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,

        0.0f, 0.0f,
        1.0f,1.0f,
        1.0f,0.0f,
//2nd img end
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,

        0.0f, 0.0f,
        1.0f,1.0f,
        1.0f,0.0f,
//3rd img end
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,

        0.0f, 0.0f,
        1.0f,1.0f,
        1.0f,0.0f
        //4th img end
};
std::chrono::high_resolution_clock::time_point glReadStartTime;
std::chrono::high_resolution_clock::time_point glReadEndTime;
std::chrono::high_resolution_clock::time_point FlipStartTime;
std::chrono::high_resolution_clock::time_point FlipEndTime;
void renderFrame() // 16.6ms
{
    float grey;
    grey = 0.00f;

    glClearColor(grey, grey, grey, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    if(fp!=NULL) {
        if (!feof(fp)) {
            fread(rawData, fread_buf_size, 1, fp);
        }else{ DPRINTF("feof end of stream");}
    }
    else {
        DPRINTF("fp is null");
    }


//    cv::Mat freadInputMat(freadbh,freadbw,CV_8UC1, rawData);
    cv::Mat freadInputMat(freadbh,freadbw,CV_8UC4, rawData);
    cv::imwrite("/storage/emulated/0/opencvTesting/freadInputMat.jpg", freadInputMat);

//    cv::Mat outputInRGBA(freadbh,freadbw, CV_8UC4);
//    cv::cvtColor(freadInputMat,outputInRGBA,cv::COLOR_RGB2BGRA);
//    cv::imwrite("/storage/emulated/0/opencvTesting/freadoutputInRGBA.jpg", outputInRGBA);
//    cv::cvtColor(outputInRGBA,freadInputMat,cv::COLOR_BGRA2RGB);

   cv::Mat inputInMat = freadInputMat;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_map1);
//    glBindFramebuffer(GL_FRAMEBUFFER, iFrameBuffObject);
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, texture_map, 0);

//    cv::Mat inputInMat = cv::imread("/storage/emulated/0/opencvTesting/wp4473260.jpg",cv::IMREAD_COLOR);
    if(inputInMat.empty())
        LOGI("input matImage is empty");

    LOGI("width:[%d], height:[%d], Channels[%d] \n", inputInMat.cols,inputInMat.rows, inputInMat.channels());

    int bw = inputInMat.cols;
    int bh = inputInMat.rows;

//    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, bw, bh, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, inputInMat.data); //this is for grey input image
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bw, bh, 0, GL_RGB, GL_UNSIGNED_BYTE, inputInMat.data);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture_map2);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, reverse_turbo_array_1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bw, bh, 0, GL_RGB, GL_UNSIGNED_BYTE, inputInMat.data);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texture_map3);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bw, bh, 0, GL_RGB, GL_UNSIGNED_BYTE, inputInMat.data);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, texture_map4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bw, bh, 0, GL_RGB, GL_UNSIGNED_BYTE, inputInMat.data);


    glUseProgram(programId);

    glVertexAttribPointer(aPosition, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
    glEnableVertexAttribArray(aPosition);

    glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 0, gTexVertices);
    glEnableVertexAttribArray(aTexCoord);

    glUniform2f(rubyTextureSize, bw, bh);
    glUniform1i(rubyTexture1, 0);
    glUniform1i(rubyTexture2, 1);
    glUniform1i(rubyTexture3, 2);
    glUniform1i(rubyTexture4, 3);

    //if(rubyInputSize >= 0)
    //    glUniform2f(rubyInputSize, bw, bh);
    //if(rubyOutputSize >= 0)
    //    glUniform2f(rubyOutputSize, vw, vh);

//    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDrawArrays(GL_TRIANGLES, 0, 24);
    static int i = 0;
    /*
    if ( i == 20) {
        GLubyte *data = (GLubyte *) malloc(4 * scnw*scnh);
        if (data) {
            glReadPixels(0, 0, scnw, scnh, GL_RGBA, GL_UNSIGNED_BYTE, data);;
        }
        std::string filename("/storage/emulated/0/opencvTesting/image.rgb");
        std::ofstream fout(filename, std::ios::binary);
        fout.write((char *) data, 4 * scnw*scnh);
        fout.close();
        free(data);
    }*/
    i++;
    //get the image from texture
//    char *outBuffer = malloc(bw*bh*4);
//    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,outBuffer); //glGetTexImage is not supported in GLES

    //dump output
    uint32_t* readPixels;
    readPixels = new uint32_t[bw * bh];
    glReadStartTime= std::chrono::high_resolution_clock::now();
    glReadPixels(0, 0, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE, readPixels);
    glReadEndTime = std::chrono::high_resolution_clock::now();
    LOGI("glReadPixel Operation Time:[%lf]msec",std::chrono::duration<double, std::milli>(glReadEndTime-glReadStartTime).count());

    cv::Mat outputReadpixelInMat = cv::Mat(inputInMat.rows,inputInMat.cols,CV_8UC4,readPixels);
    if(outputReadpixelInMat.empty())
        LOGI("outputReadpixelInMat inputInMat empty");

    cv::imwrite("/storage/emulated/0/opencvTesting/outputReadpixelInMat.jpg", outputReadpixelInMat);
    cv::Mat flippedMat(outputReadpixelInMat.rows,outputReadpixelInMat.cols,CV_8UC4);
    FlipStartTime = std::chrono::high_resolution_clock::now();
    cv::flip(outputReadpixelInMat, flippedMat, 0);
    FlipEndTime = std::chrono::high_resolution_clock::now();
    LOGI("flip Operation Time:[%lf]msec",std::chrono::duration<double, std::milli>(FlipEndTime-FlipStartTime).count());

    cv::imwrite("/storage/emulated/0/opencvTesting/outputReadpixelInFlippedMat.jpg", flippedMat);

}

void _init(JNIEnv *env, jobject bmp) {
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    //AndroidBitmapInfo info;
    //AndroidBitmap_getInfo(env, bmp, &info);
    //bw = info.width;
    //bh = info.height;
    //AndroidBitmap_lockPixels(env, bmp, &img);
//    glGenFramebuffers(1, &iFrameBuffObject);
//    glBindFramebuffer(GL_FRAMEBUFFER, iFrameBuffObject);
    glGenTextures(1, &texture_map1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_map1);

//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, texture_map, 0);

//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &texture_map2);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture_map2);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glGenTextures(1, &texture_map3);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texture_map3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &texture_map4);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, texture_map4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    DPRINTF("read input file");
//    std::string FileName = std::string("/storage/emulated/0/opencvTesting/tina60-120");
    std::string FileName = std::string("/storage/emulated/0/opencvTesting/videoFrmImouInrawrgb24short.rgb");
    fp = fopen(FileName.c_str(),"r+b");
    if(NULL == fp) {
            DPRINTF(" fopen() Error!!!\n");
    }

//    freadbw = 1440;
    freadbw = 1920;
    freadbh = 1080 ;
//    fread_buf_size = freadbw*freadbh;
    fread_buf_size = freadbw*freadbh*3;
    //Allocate Buffer for rawData
    rawData = (unsigned char *)malloc(fread_buf_size);
    if (NULL == rawData) {
        DPRINTF("Rawdata is NULL\n");
    }


}

void _loadShader(JNIEnv *env, jstring jvs, jstring jfs) {
    jboolean b;
    if(jvs) {
        const char *vs = env->GetStringUTFChars(jvs, &b);
        gVs = vs? strdup(vs) : NULL;
        env->ReleaseStringUTFChars(jvs, vs);
    } else
        gVs = (char *)gVertexShader;

    if(jfs) {
        const char *fs = env->GetStringUTFChars(jfs, &b);
        gFs = fs? strdup(fs) : NULL;
        env->ReleaseStringUTFChars(jfs, fs);
    } else
        gFs = (char *)gFragmentShader;

    initProgram();
}

extern "C" {
JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv *env, jobject obj, jobject bmp)
{
    _init(env, bmp);
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_resize(JNIEnv *env, jobject obj, jint width, jint height)
{
    setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv *env, jobject obj)
{
    renderFrame();
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadShader(JNIEnv *env, jobject obj, jstring vs, jstring fs)
{
    _loadShader(env, vs, fs);
}
};