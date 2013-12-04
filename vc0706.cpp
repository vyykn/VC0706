#include <node.h>
#include <node_object_wrap.h>

// C standard library
#include <cstdlib>
#include <ctime>
#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>

// Brian's DEPS
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
using namespace v8;
using namespace node;


extern "C"  {
#include "deps/wiringSerial.h"
#include "deps/wiringPi.h"
}


// Brian's Defines
#define BAUD 38400

#define RESET 0x26
#define GEN_VERSION 0x11
#define READ_FBUF 0x32
#define GET_FBUF_LEN 0x34
#define FBUF_CTRL 0x36
#define DOWNSIZE_CTRL 0x54
#define DOWNSIZE_STATUS 0x55
#define READ_DATA 0x30
#define WRITE_DATA 0x31
#define COMM_MOTION_CTRL 0x37
#define COMM_MOTION_STATUS 0x38
#define COMM_MOTION_DETECTED 0x39
#define MOTION_CTRL 0x42
#define MOTION_STATUS 0x43
#define TVOUT_CTRL 0x44
#define OSD_ADD_CHAR 0x45

#define STOPCURRENTFRAME 0x0
#define STOPNEXTFRAME 0x1
#define RESUMEFRAME 0x3
#define STEPFRAME 0x2

#define SIZE640 0x00
#define SIZE320 0x11
#define SIZE160 0x22

#define MOTIONCONTROL 0x0
#define UARTMOTION 0x01
#define ACTIVATEMOTION 0x01

#define SET_ZOOM 0x52
#define GET_ZOOM 0x53

#define CAMERABUFFSIZ 100
#define CAMERADELAY 10

#define TO_SCALE 1
#define TO_U 1000000



class Camera {

public:
    Camera();
    ~Camera();
    void reset();
    void setMotionDetect(int flag);
    void clearBuffer();
    bool motionDetected();
    char * getVersion();
    char * takePicture();
    int stopMotion;
    int ready;

private:
    bool checkReply(int cmd, int size);
    void resumeVideo();

    uint8_t offscreen[8]; // font width;

    // Brian's Variables
    int fd;
    int frameptr;
    int bufferLen;
    int serialNum;
    char camerabuff[CAMERABUFFSIZ+1];
    char serialHeader[5];
    char imageName[16];
    char * empty;
};

Camera::Camera() {
    frameptr = 0;
    bufferLen = 0;
    serialNum = 0;
    stopMotion = 0;
    ready = 0;


    if ((fd = serialOpen("/dev/ttyAMA0", BAUD)) < 0)
        cout <<  "SPI Setup Failed: " <<  strerror(errno) << endl;

    if (wiringPiSetup() == -1)
        exit(1);

    ready = 1;

}

Camera::~Camera() {}

void Camera::reset() {
    // Camera Reset method
    serialPutchar(fd, (char)0x56);
    serialPutchar(fd, (char)serialNum);
    serialPutchar(fd, (char)RESET);
    serialPutchar(fd, (char)0x00);

    if (checkReply(RESET, 5) != true)
        cout <<  "Check Reply Status: " <<  strerror(errno) << endl;

    clearBuffer();

}

bool Camera::checkReply(int cmd, int size) {
    int reply[size];
    int t_count = 0;
    int length = 0;
    int avail = 0;
    int timeout = 5 * TO_SCALE;
    
    while ((timeout != t_count) && (length != CAMERABUFFSIZ) && length < size)
    {
        avail = serialDataAvail(fd);
        if (avail <= 0)
        {
            usleep(TO_U);
            t_count++;
            continue;
        }
        t_count = 0;
        // there's a byte!
        int newChar = serialGetchar(fd);
        reply[length++] = (char)newChar;
    }
    
    //Check the reply
    if (reply[0] != 0x76 && reply[1] != 0x00 && reply[2] != cmd)
        return false;
    else
        return true;
}

void Camera::clearBuffer() {
    int t_count = 0;
    int length = 0;
    int timeout = 2 * TO_SCALE;

    while ((timeout != t_count) && (length != CAMERABUFFSIZ))
    {
        int avail = serialDataAvail(fd);
        if (avail <= 0)
        {
            t_count++;
            continue;
        }
        t_count = 0;
        // there's a byte!
        serialGetchar(fd);
        length++;
    }
}

void Camera::resumeVideo()
{
    serialPutchar(fd, (char)0x56);
    serialPutchar(fd, (char)serialNum);
    serialPutchar(fd, (char)FBUF_CTRL);
    serialPutchar(fd, (char)0x01);
    serialPutchar(fd, (char)RESUMEFRAME);
    
    if (checkReply(FBUF_CTRL, 5) == false)
        printf("Camera did not resume\n");
}

char * Camera::getVersion()
{
    serialPutchar(fd, (char)0x56);
    serialPutchar(fd, (char)serialNum);
    serialPutchar(fd, (char)GEN_VERSION);
    serialPutchar(fd, (char)0x00);
    
    if (checkReply(GEN_VERSION, 5) == false)
    {
        printf("CAMERA NOT FOUND!!!\n");
    }
    
    int counter = 0;
    bufferLen = 0;
    int avail = 0;
    int timeout = 1 * TO_SCALE;
    
    while ((timeout != counter) && (bufferLen != CAMERABUFFSIZ))
    {
        avail = serialDataAvail(fd);
        if (avail <= 0)
        {
            usleep(TO_U);
            counter++;
            continue;
        }
        counter = 0;
        // there's a byte!
        int newChar = serialGetchar(fd);
        camerabuff[bufferLen++] = (char)newChar;
    }
    
    camerabuff[bufferLen] = 0;
    
    return camerabuff;
}

void Camera::setMotionDetect(int flag)
{
    serialPutchar(fd, (char)0x56);
    serialPutchar(fd, (char)serialNum);
    serialPutchar(fd, (char)MOTION_CTRL);
    serialPutchar(fd, (char)0x03);
    serialPutchar(fd, (char)MOTIONCONTROL);
    serialPutchar(fd, (char)UARTMOTION);
    serialPutchar(fd, (char)ACTIVATEMOTION);
    
    if (checkReply(MOTION_CTRL, 5) == false)
        printf("Error setting motion control settings\n");

    clearBuffer();
    
    serialPutchar(fd, (char)0x56);
    serialPutchar(fd, (char)serialNum);
    serialPutchar(fd, (char)COMM_MOTION_CTRL);
    serialPutchar(fd, (char)0x01);
    serialPutchar(fd, (char)flag);
    
    if (checkReply(COMM_MOTION_CTRL, 5) == false)
        printf("Error turning on motion detection\n");
}

bool Camera::motionDetected()
{
    int reply[5];
    int t_count = 0;
    int length = 0;
    int timeout = 4 * TO_SCALE;
    
    while ((timeout != t_count) && (length != CAMERABUFFSIZ) && length < 5)
    {
        int avail = serialDataAvail(fd);
        if (avail <= 0)
        {
            usleep(TO_U);
            t_count++;
            continue;
        }
        t_count = 0;
        // there's a byte!
        int newChar = serialGetchar(fd);
        reply[length++] = (char)newChar;
    }
    
    if (reply[0] == 0x76 && reply[1] == serialNum && reply[2] == COMM_MOTION_DETECTED && reply[3] == 0x00)
        return true;
    else
        return false;
}


char * Camera::takePicture()
{
    frameptr = 0;

    serialPutchar(fd, (char)0x56);
    serialPutchar(fd, (char)serialNum);
    serialPutchar(fd, (char)FBUF_CTRL);
    serialPutchar(fd, (char)0x01);
    serialPutchar(fd, (char)STOPCURRENTFRAME);
    
    if (checkReply(FBUF_CTRL, 5) == false)
    {
        return empty;
    }
    
    serialPutchar(fd, (char)0x56);
    serialPutchar(fd, (char)serialNum);
    serialPutchar(fd, (char)GET_FBUF_LEN);
    serialPutchar(fd, (char)0x01);
    serialPutchar(fd, (char)0x00);
    
    if (checkReply(GET_FBUF_LEN, 5) == false)
    {
        printf("FBUF_LEN REPLY NOT VALID!!!\n");
        return empty;
    }
    
    while(serialDataAvail(fd) <= 0){}

    printf("Serial Data Avail %u \n", serialDataAvail(fd));
    
    int len;
    len = serialGetchar(fd);
    len <<= 8;
    len |= serialGetchar(fd);
    len <<= 8;
    len |= serialGetchar(fd);
    len <<= 8;
    len |= serialGetchar(fd);

    printf("Length %u \n", len);

    if(len > 20000){
        printf("To Large... \n");
        resumeVideo();
        clearBuffer();
        return Camera::takePicture();
    }
    
    char image[len];
    
    int imgIndex = 0;
    
    while (len > 0)
    {
        int readBytes = len;
        
        serialPutchar(fd, (char)0x56);
        serialPutchar(fd, (char)serialNum);
        serialPutchar(fd, (char)READ_FBUF);
        serialPutchar(fd, (char)0x0C);
        serialPutchar(fd, (char)0x0);
        serialPutchar(fd, (char)0x0A);
        serialPutchar(fd, (char)(frameptr >> 24 & 0xff));
        serialPutchar(fd, (char)(frameptr >> 16 & 0xff));
        serialPutchar(fd, (char)(frameptr >> 8 & 0xff));
        serialPutchar(fd, (char)(frameptr & 0xFF));
        serialPutchar(fd, (char)(readBytes >> 24 & 0xff));
        serialPutchar(fd, (char)(readBytes >> 16 & 0xff));
        serialPutchar(fd, (char)(readBytes >> 8 & 0xff));
        serialPutchar(fd, (char)(readBytes & 0xFF));
        serialPutchar(fd, (char)(CAMERADELAY >> 8));
        serialPutchar(fd, (char)(CAMERADELAY & 0xFF));
            
        if (checkReply(READ_FBUF, 5) == false)
        {
            return empty;
        }
        
        int counter = 0;
        bufferLen = 0;
        int avail = 0;
        int timeout = 20 * TO_SCALE;
        
        while ((timeout != counter) && bufferLen < readBytes)
        {
            avail = serialDataAvail(fd);

            if (avail <= 0)
            {
                usleep(TO_U);
                counter++;
                continue;
            }
            counter = 0;
            int newChar = serialGetchar(fd);
            image[imgIndex++] = (char)newChar;
            
            bufferLen++;
        }
        
        frameptr += readBytes;
        len -= readBytes;
        
        if (checkReply(READ_FBUF, 5) == false)
        {
            printf("ERROR READING END OF CHUNK| start: %u | length: %u\n", frameptr, len);
        }
    }

    
    char name[23];
    int t = (int)time(NULL);
    sprintf(name, "images/%u.jpg", t);
    
    umask(0111);
    
    FILE *jpg = fopen(name, "w");
    if (jpg != NULL)
    {
        fwrite(image, sizeof(char), sizeof(image), jpg);
        fclose(jpg);
    }
    else
    {
        printf("IMAGE COULD NOT BE OPENED/MADE!\n");
    }
    
    sprintf(imageName, "%u.jpg", t);
    
    resumeVideo();
    
    return imageName;
    
}

// End Brian's Methods



namespace {

    struct PictureBaton {
        Persistent<Function> callback;

        char *display_message;

        bool error;
        std::string error_message;
        std::string pictureName;

    };

    struct ResetBaton {
        Persistent<Function> callback;
        Persistent<Object> emitter;

        char *display_message;

        bool error;
        std::string error_message;
        std::string versionNumber;

        int32_t result;
    };

    struct MotionBaton {
        Persistent<Function> callback;
        Persistent<Object> emitter;

        char *display_message;

        bool error;
        std::string error_message;
        std::string versionNumber;

        int32_t result;
    };

    static Camera *camera;

    class VC0706: public ObjectWrap {
        public:
            static Handle<Value> New(const Arguments& args);
            static Handle<Value> TakePicture(const Arguments& args);
            static void PictureAsyncWork(uv_work_t* req);
            static void PictureAsyncAfter(uv_work_t* req);

            static Handle<Value> Reset(const Arguments& args);
            static void ResetAsyncWork(uv_work_t* req);
            static void ResetAsyncAfter(uv_work_t* req);

            static Handle<Value> Motion(const Arguments& args);
            static void MotionAsyncWork(uv_work_t* req);
            static void MotionAsyncAfter(uv_work_t* req);
        };


    Handle<Value> VC0706::New(const Arguments& args) {
        HandleScope scope;

        assert(args.IsConstructCall());
        VC0706* self = new VC0706();
        self->Wrap(args.This());

        camera = new Camera();

        return scope.Close(args.This());
    }


    Handle<Value> VC0706::TakePicture(const Arguments& args) {
        HandleScope scope;

        if (args.Length() < 1) {
            return ThrowException(
                Exception::TypeError(String::New("Arguments: Callback Function"))
                );
        }

        if (!args[0]->IsFunction()) {
            return ThrowException(Exception::TypeError(
                String::New("First argument must be a callback function")));
        }

        Local<Function> callback = Local<Function>::Cast(args[0]);

        PictureBaton* pic_baton = new PictureBaton();
        pic_baton->error = false;
        pic_baton->callback = Persistent<Function>::New(callback);

        uv_work_t *req = new uv_work_t();
        req->data = pic_baton;

        int status = uv_queue_work(uv_default_loop(), req, PictureAsyncWork, (uv_after_work_cb)PictureAsyncAfter);
        assert(status == 0);

        return Undefined();
    }


    void VC0706::PictureAsyncWork(uv_work_t* req) {
        PictureBaton* pic_baton = static_cast<PictureBaton*>(req->data);

        if(camera->ready == 1){
            camera->ready = 0;

            // Force Stop motion detect
                camera->setMotionDetect(0);

                //Clear Buffer
                camera->clearBuffer();
            
            //Take Picture
            pic_baton->pictureName = camera->takePicture();

            //Clear Buffer
            camera->clearBuffer();

            //Say camera is ready again
            camera->ready = 1;
        }
        else{

            camera->stopMotion = 1;

            int t_count = 0;
            int timeout = 10 * TO_SCALE;

            while ((timeout != t_count) && (camera->ready != 1))
            {
                usleep(TO_U);
                t_count++;
            }

            if(camera->ready == 1){
                camera->ready = 0;


                // Force Stop motion detect
                camera->setMotionDetect(0);

                //Clear Buffer
                camera->clearBuffer();
                
                //Take Picture
                pic_baton->pictureName = camera->takePicture();

                //Clear Buffer
                camera->clearBuffer();

                //Say camera is ready again
                camera->ready = 1;
            }

            // Reset stop motion detect
            camera->stopMotion = 0;
            
        }

    }

    // This function is executed in the main V8/JavaScript thread. That means it's
    // safe to use V8 functions again. Don't forget the HandleScope!
    void VC0706::PictureAsyncAfter(uv_work_t* req) {
        HandleScope scope;
        PictureBaton* pic_baton = static_cast<PictureBaton*>(req->data);

        if (pic_baton->error) {
            Local<Value> err = Exception::Error(String::New(pic_baton->error_message.c_str()));

            const unsigned argc = 1;
            Local<Value> argv[argc] = { err };

            TryCatch try_catch;
            pic_baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);

            if (try_catch.HasCaught()) {
                node::FatalException(try_catch);
            }
        } else {

            const unsigned argc = 2;
            Local<Value> argv[argc] = {
                Local<Value>::New(Null()),
                Local<Value>::New(String::New(pic_baton->pictureName.c_str()))
            };

            TryCatch try_catch;
            pic_baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
            if (try_catch.HasCaught()) {
                node::FatalException(try_catch);
            }
            }

            pic_baton->callback.Dispose();

            free(pic_baton->display_message);

            delete pic_baton;
            delete req;
    }



    Handle<Value> VC0706::Reset(const Arguments& args) {
        HandleScope scope;

        if (args.Length() < 1) {
            return ThrowException(
                Exception::TypeError(String::New("Arguments: Callback Function"))
                );
        }

        if (!args[0]->IsFunction()) {
            return ThrowException(Exception::TypeError(
                String::New("First argument must be a callback function")));
        }

        Local<Function> callback = Local<Function>::Cast(args[0]);

        ResetBaton* reset_baton = new ResetBaton();
        reset_baton->error = false;
        reset_baton->callback = Persistent<Function>::New(callback);
        reset_baton->emitter = Persistent<Object>::New(args.This());

        uv_work_t *req = new uv_work_t();
        req->data = reset_baton;

        int status = uv_queue_work(uv_default_loop(), req, ResetAsyncWork, (uv_after_work_cb)ResetAsyncAfter);
        assert(status == 0);

        return Undefined();
    }

    void VC0706::ResetAsyncWork(uv_work_t* req) {
        ResetBaton* reset_baton = static_cast<ResetBaton*>(req->data);

        camera->reset();

        reset_baton->versionNumber = camera->getVersion();

    }

    void VC0706::ResetAsyncAfter(uv_work_t* req) {
        HandleScope scope;
        ResetBaton* reset_baton = static_cast<ResetBaton*>(req->data);

        if (reset_baton->error) {
            Local<Value> err = Exception::Error(String::New(reset_baton->error_message.c_str()));

            const unsigned argc = 1;
            Local<Value> argv[argc] = { err };

            TryCatch try_catch;
            reset_baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);

            if (try_catch.HasCaught()) {
                node::FatalException(try_catch);
            }
        } else {

            Local<Value> event[2] = {
            String::New("ResetComplete"), // event name
            String::New("success")  // argument
            };

            MakeCallback(reset_baton->emitter, "emit", 2, event);


            const unsigned argc = 2;
            Local<Value> argv[argc] = {
                Local<Value>::New(Null()),
                Local<Value>::New(String::New(reset_baton->versionNumber.c_str()))
            };

            TryCatch try_catch;
            reset_baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
            if (try_catch.HasCaught()) {
                node::FatalException(try_catch);
            }
        }

        reset_baton->callback.Dispose();
        reset_baton->emitter.Dispose();

        free(reset_baton->display_message);

        delete reset_baton;
        delete req;
    }


    Handle<Value> VC0706::Motion(const Arguments& args) {
        HandleScope scope;


        MotionBaton* motion_baton = new MotionBaton();
        motion_baton->error = false;
        motion_baton->emitter = Persistent<Object>::New(args.This());

        uv_work_t *req = new uv_work_t();
        req->data = motion_baton;

        int status = uv_queue_work(uv_default_loop(), req, MotionAsyncWork, (uv_after_work_cb)MotionAsyncAfter);
        assert(status == 0);

        return Undefined();
    }

    void VC0706::MotionAsyncWork(uv_work_t* req) {
        MotionBaton* motion_baton = static_cast<MotionBaton*>(req->data);

        if(camera->ready == 1) {
            camera->ready = 0;

            // Turn on motion detect
            camera->setMotionDetect(1);

            //Clear Buffer
            camera->clearBuffer();

            // Wait for either motion or interupt
            while (!camera->motionDetected() && camera->stopMotion == 0){
            }

            // If interupt
            if(camera->stopMotion == 1) {
                printf("Motion Was Internal - from printf\n");


                // Stop event emitter
                motion_baton->error = true;
                motion_baton->error_message = "Motion Shutdown was internal";


                // Force Stop motion detect
                camera->setMotionDetect(0);

                //Clear Buffer
                camera->clearBuffer();

                // Reset the stop motion
                camera->stopMotion = 0;

                // Say camera is ready
                camera->ready = 1;
            }
            // If motion
            else {

                // Re-enable event emitter
                motion_baton->error = false;

                // Camera is "ready" at this instance
                // (used for restarting after motion)
                camera->ready = 1;
            }


        }
        else {
            motion_baton->error = true;
            motion_baton->error_message = "Camera Busy";
        }

    }

    void VC0706::MotionAsyncAfter(uv_work_t* req) {
        HandleScope scope;
        MotionBaton* motion_baton = static_cast<MotionBaton*>(req->data);

        if (motion_baton->error) {
            
        } else {

            Local<Value> event[2] = {
            String::New("MotionDetected"), // event name
            String::New("Motion Detected")  // argument
            };

            MakeCallback(motion_baton->emitter, "emit", 2, event);

        }

        motion_baton->emitter.Dispose();

        free(motion_baton->display_message);

        delete motion_baton;
        delete req;
    }





void RegisterModule(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(VC0706::New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(String::New("VC0706"));
    NODE_SET_PROTOTYPE_METHOD(t, "TakePicture", VC0706::TakePicture);
    NODE_SET_PROTOTYPE_METHOD(t, "Reset", VC0706::Reset);
    NODE_SET_PROTOTYPE_METHOD(t, "MotionDetect", VC0706::Motion);

    target->Set(String::NewSymbol("VC0706"), t->GetFunction());

}

NODE_MODULE(VC0706, RegisterModule);
}
