#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "cv.h"
#include "highgui.h"
 
#define PORT 8888
 
CvCapture*  capture;
IplImage*   img0;
IplImage*   img1;
IplImage*   source; 
IplImage*   destination;
int         is_data_ready = 0;
int         serversock, clientsock;
 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
 
void* streamServer(void* arg);
void  quit(char* msg, int retval);
 
int main(int argc, char** argv)
{
    pthread_t   thread_s;
    int         key;
    int flag =  0;
 
    if (argc == 2) {
        
            flag = 1;
        
        capture = cvCaptureFromCAM(0);
        
        //capture = cvCaptureFromFile(argv[1]);
        
    } else {
        capture = cvCaptureFromCAM(0);
    }
 
    if (!capture) {
        quit("cvCapture failed", 1);
    }
 


    //cvQueryFrame(capture);
     //capture = cvCreateCameraCapture(-1);



    //cvSetCaptureProperty( capture, CV_CAP_PROP_FRAME_WIDTH, 320);
    //cvSetCaptureProperty( capture, CV_CAP_PROP_FRAME_HEIGHT, 240);
   
        img0 = cvQueryFrame(capture);

// Create an IplImage object *image 
source = img0;


// declare a destination IplImage object with correct size, depth and channels
     destination = cvCreateImage
( cvSize((int)(source->width/2) , (int)(source->height/2) ),
                                     source->depth, source->nChannels );

//use cvResize to resize source to a destination image
cvResize(source, destination,CV_INTER_LINEAR);






    img1 = cvCreateImage(cvGetSize(destination), IPL_DEPTH_8U, 1);
 
    cvZero(img1);
    cvNamedWindow("stream_server", CV_WINDOW_AUTOSIZE);
 
    /* print the width and height of the frame, needed by the client */
    fprintf(stdout, "width:  %d\nheight: %d\n\n", destination->width, destination->height);
    fprintf(stdout, "Press 'q' to quit.\n\n");
 
    /* run the streaming server as a separate thread */
    if (pthread_create(&thread_s, NULL, streamServer, NULL)) {
        quit("pthread_create failed.", 1);
    }
 
    while(key != 'q') {
        /* get a frame from camera */
        img0 = cvQueryFrame(capture);
        if (!img0) break;

        // Create an IplImage object *image 
        source = img0;


        // declare a destination IplImage object with correct size, depth and channels
             destination = cvCreateImage
        ( cvSize((int)(source->width/2) , (int)(source->height/2) ),
                                     source->depth, source->nChannels );

        //use cvResize to resize source to a destination image
        cvResize(source, destination,CV_INTER_LINEAR);
        
        destination->origin = 0;
        //cvFlip(img0, img0, -1);
 
        /**
         * convert to grayscale 
         * note that the grayscaled image is the image to be sent to the client 
         * so we enclose it with pthread_mutex_lock to make it thread safe 
         */
        pthread_mutex_lock(&mutex);
        cvCvtColor(destination, img1, CV_BGR2GRAY);
        is_data_ready = 1;
        pthread_mutex_unlock(&mutex);
 
        /* also display the video here on server */
        if(flag>0){
            cvShowImage("stream_server", destination);
            key = cvWaitKey(30);
        }
 
    }
    /* user has pressed 'q', terminate the streaming server */
    if (pthread_cancel(thread_s)) {
        quit("pthread_cancel failed.", 1);
    }
 
    /* free memory */
    cvDestroyWindow("stream_server");
    quit(NULL, 0);
}
 
/**
 * This is the streaming server, run as a separate thread
 * This function waits for a client to connect, and send the grayscaled images
 */ 
void* streamServer(void* arg)
{
    struct sockaddr_in server;
 
    /* make this thread cancellable using pthread_cancel() */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
 
    /* open socket */
    if ((serversock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        quit("socket() failed", 1);
    }
 
    /* setup server's IP and port */
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
 
    /* bind the socket */
    if (bind(serversock, (const void*)&server, sizeof(server)) == -1) {
        quit("bind() failed", 1);
    }
 
    /* wait for connection */
    if (listen(serversock, 10) == -1) {
        quit("listen() failed.", 1);
    }
 
    /* accept a client */
    if ((clientsock = accept(serversock, NULL, NULL)) == -1) {
        quit("accept() failed", 1);
    }
 
    /* the size of the data to be sent */
    int imgsize = img1->imageSize;
    int bytes, i;
 
    /* start sending images */
    while(1) 
    {
        /* send the grayscaled frame, thread safe */
        pthread_mutex_lock(&mutex);
        if (is_data_ready) {
            bytes = send(clientsock, img1->imageData, imgsize, 0);
            is_data_ready = 0;
        }
        pthread_mutex_unlock(&mutex);
 
        /* if something went wrong, restart the connection */
        if (bytes != imgsize) {
            fprintf(stderr, "Connection closed.\n");
            close(clientsock);
 
            if ((clientsock = accept(serversock, NULL, NULL)) == -1) {
                quit("accept() failed", 1);
            }
        }
 
        /* have we terminated yet? */
        pthread_testcancel();
 
        /* no, take a rest for a while */
        //usleep(1000);
    }
}
 
/**
 * this function provides a way to exit nicely from the system
 */
void quit(char* msg, int retval)
{
    if (retval == 0) {
        fprintf(stdout, "%s", (msg == NULL ? "" : msg));
        fprintf(stdout, "\n");
    } else {
        fprintf(stderr, "%s", (msg == NULL ? "" : msg));
        fprintf(stderr, "\n");
    }
 
    if (clientsock) close(clientsock);
    if (serversock) close(serversock);
    if (capture) cvReleaseCapture(&capture);
    if (img1) cvReleaseImage(&img1);
 
    pthread_mutex_destroy(&mutex);
 
    exit(retval);
}
