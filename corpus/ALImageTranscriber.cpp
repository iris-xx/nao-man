
#include "alvisionimage.h"
#include "alvisiondefinitions.h"

#include "manconfig.h"

#include "ALImageTranscriber.h"

using boost::shared_ptr;
using namespace AL;

ALImageTranscriber::ALImageTranscriber(shared_ptr<Synchro> synchro,
                                       shared_ptr<Sensors> s,
                                       ALPtr<ALBroker> broker)
    : ThreadedImageTranscriber(s,synchro,"ALImageTranscriber"),
      log(), camera(), lem_name(""), camera_active(false),
      image(new unsigned char[IMAGE_BYTE_SIZE])
{
    try {
        log = broker->getLoggerProxy();
        // Possible values are
        // lowDebug, debug, lowInfo, info, warning, error, fatal
        log->setVerbosity("error");
    }catch (ALError &e) {
        std::cerr << "Could not create a proxy to ALLogger module" << std::endl;
    }

#ifdef USE_VISION
    registerCamera(broker);
    if(camera_active) {
        try{
        initCameraSettings(BOTTOM_CAMERA);
        }catch(ALError &e){
            cout << "Failed to init the camera settings:"<<e.toString()<<endl;
            camera_active = false;
        }
    }
    else
        cout << "\tCamera is inactive!" << endl;
#endif
}

ALImageTranscriber::~ALImageTranscriber() {
    delete [] image;
    stop();
}


int ALImageTranscriber::start() {
    return Thread::start();
}

void ALImageTranscriber::run() {
    Thread::running = true;
    Thread::trigger->on();

	long long lastProcessTimeAvg = VISION_FRAME_LENGTH_uS;
    while (Thread::running) {
        //start timer
        const long long startTime = micro_time();

        if (camera_active)
            waitForImage();
        subscriber->notifyNextVisionImage();

        //stop timer
        const long long processTime = micro_time() - startTime;
        //sleep until next frame

		lastProcessTimeAvg = lastProcessTimeAvg/2 + processTime/2;
        if (processTime > VISION_FRAME_LENGTH_uS){
			if (lastProcessTimeAvg > VISION_FRAME_LENGTH_PRINT_THRESH_uS)
				cout << "Time spent in ALImageTranscriber loop longer than"
					 << " frame length: " << processTime <<endl;
            //Don't sleep at all
        } else{
            // cout << "Sleeping for " << VISION_FRAME_LENGTH_uS
            //    -processTime << endl;

            //usleep(10000000);
            usleep(static_cast<useconds_t>(VISION_FRAME_LENGTH_uS
                                           -processTime));
        }
    }
    Thread::trigger->off();
}

void ALImageTranscriber::stop() {
    cout << "Stopping ALImageTranscriber" << endl;
    running = false;
#ifdef USE_VISION
    if(camera_active){
        cout << "lem_name = " << lem_name << endl;
        try {
            camera->callVoid("unregister", lem_name);
        }catch (ALError &e) {
            log->error("Man", "Could not call the unregister method of the NaoCam "
                       "module");
        }
    }
#endif

    Thread::stop();
}

void ALImageTranscriber::registerCamera(ALPtr<ALBroker> broker) {
    try {
        camera = broker->getProxy("NaoCam");
        camera_active =true;
    }catch (ALError &e) {
        log->error("ALImageTranscriber",
                   "Could not create a proxy to NaoCam module");
        camera_active =false;
        return;
    }

    lem_name = "ALImageTranscriber_LEM";
    int format = NAO_IMAGE_SIZE;
    int colorSpace = NAO_COLOR_SPACE;
    int fps = DEFAULT_CAMERA_FRAMERATE;

    int resolution = format;



#ifdef DEBUG_MAN_INITIALIZATION
    printf("  Registering LEM with format=%i colorSpace=%i fps=%i\n", format,
           colorSpace, fps);
#endif

    try {
        lem_name = camera->call<std::string>("register", lem_name, format,
                                             colorSpace, fps);
        cout << "Registered Camera: " << lem_name << " successfully"<<endl;
    } catch (ALError &e) {
        cout << "Failed to register camera" << lem_name << endl;
        camera_active = false;
//         SleepMs(500);

//         try {
//             printf("LEM failed once, trying again\n");
//             lem_name = camera->call<std::string>("register", lem_name, format,
//                                                  colorSpace, fps);
//         }catch (ALError &e2) {
//             log->error("ALImageTranscriber", "Could not call the register method of the NaoCam "
//                        "module\n" + e2.toString());
//             return;
//         }
    }

}

void ALImageTranscriber::initCameraSettings(int whichCam){

    int currentCam =  camera->call<int>( "getParam", kCameraSelectID );
    if (whichCam != currentCam){
        camera->callVoid( "setParam", kCameraSelectID,whichCam);
        SleepMs(CAMERA_SLEEP_TIME);
        currentCam =  camera->call<int>( "getParam", kCameraSelectID );
        if (whichCam != currentCam){
            cout << "Failed to switch to camera "<<whichCam
                 <<" retry in " << CAMERA_SLEEP_TIME <<" ms" <<endl;
            SleepMs(CAMERA_SLEEP_TIME);
            currentCam =  camera->call<int>( "getParam", kCameraSelectID );
            if (whichCam != currentCam){
                cout << "Failed to switch to camera "<<whichCam
                     <<" ... returning, no parameters initialized" <<endl;
                return;
            }
        }
        cout << "Switched to camera " << whichCam <<" successfully"<<endl;
    }

    // Turn off auto settings
    // Auto exposure
    try {
        camera->callVoid("setParam", kCameraAutoExpositionID,
                         DEFAULT_CAMERA_AUTO_EXPOSITION);
    } catch (ALError &e){
        log->error("ALImageTranscriber", "Couldn't set AutoExposition");
    }
	int param = camera->call<int>("getParam", kCameraAutoExpositionID);
	// if that didn't work, then try again
	if (param != DEFAULT_CAMERA_AUTO_EXPOSITION) {
		try {
			camera->callVoid("setParam", kCameraAutoExpositionID,
							 DEFAULT_CAMERA_AUTO_EXPOSITION);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set AutoExposition AGAIN");
		}
	}
    // Auto white balance
    try {
        camera->callVoid("setParam", kCameraAutoWhiteBalanceID,
                         DEFAULT_CAMERA_AUTO_WHITEBALANCE);

    } catch (ALError &e){
        log->error("ALImageTranscriber", "Couldn't set AutoWhiteBalance");
    }
	param = camera->call<int>("getParam", kCameraAutoWhiteBalanceID);
	if (param != DEFAULT_CAMERA_AUTO_WHITEBALANCE) {
		try {
			camera->callVoid("setParam", kCameraAutoWhiteBalanceID,
							 DEFAULT_CAMERA_AUTO_WHITEBALANCE);
		} catch (ALError &e){
			log->error("ALImageTranscriber","Couldn't set AutoWhiteBalance AGAIN");
		}
	}
    // Auto gain
    try {
        camera->callVoid("setParam", kCameraAutoGainID,
                         DEFAULT_CAMERA_AUTO_GAIN);
    } catch (ALError &e){
        log->error("ALImageTranscriber", "Couldn't set AutoGain");
    }
	param = camera->call<int>("getParam", kCameraAutoGainID);
	if (param != DEFAULT_CAMERA_AUTO_GAIN) {
		try {
			camera->callVoid("setParam", kCameraAutoGainID,
							 DEFAULT_CAMERA_AUTO_GAIN);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set AutoGain AGAIN");
		}
	}
    // Set camera defaults
    // brightness
    try {
        camera->callVoid("setParam", kCameraBrightnessID,
                         DEFAULT_CAMERA_BRIGHTNESS);
    } catch (ALError &e){
        log->error("ALImageTranscriber", "Couldn't set Brightness ");
    }
	param = camera->call<int>("getParam", kCameraBrightnessID);
	if (param != DEFAULT_CAMERA_BRIGHTNESS) {
		try {
			camera->callVoid("setParam", kCameraBrightnessID,
							 DEFAULT_CAMERA_BRIGHTNESS);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set BRIGHTNESS AGAIN");
		}
	}
    // contrast
    try {
        camera->callVoid("setParam", kCameraContrastID,
                         DEFAULT_CAMERA_CONTRAST);
    } catch (ALError &e){
        log->error("ALImageTranscriber", "Couldn't set Contrast");
    }
	param = camera->call<int>("getParam", kCameraContrastID);
	if (param != DEFAULT_CAMERA_CONTRAST) {
		try {
			camera->callVoid("setParam", kCameraContrastID,
							 DEFAULT_CAMERA_CONTRAST);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set Contrast AGAIN");
		}
	}
    // Red chroma
    try {
        camera->callVoid("setParam", kCameraRedChromaID,
                         DEFAULT_CAMERA_REDCHROMA);
    } catch (ALError &e){
        log->error("ALImageTranscriber", "Couldn't set RedChroma");
    }
	param = camera->call<int>("getParam", kCameraRedChromaID);
	if (param != DEFAULT_CAMERA_REDCHROMA) {
		try {
			camera->callVoid("setParam", kCameraRedChromaID,
							 DEFAULT_CAMERA_REDCHROMA);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set RedChroma AGAIN");
		}
	}
    // Blue chroma
    try {
        camera->callVoid("setParam", kCameraBlueChromaID,
                         DEFAULT_CAMERA_BLUECHROMA);
    } catch (ALError &e){
        log->error("ALImageTranscriber", "Couldn't set BlueChroma");
    }
	param = camera->call<int>("getParam", kCameraBlueChromaID);
	if (param != DEFAULT_CAMERA_BLUECHROMA) {
		try {
			camera->callVoid("setParam", kCameraBlueChromaID,
							 DEFAULT_CAMERA_BLUECHROMA);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set BlueChroma AGAIN");
		}
	}
    // Exposure length
    try {
        camera->callVoid("setParam",kCameraExposureID,
                         DEFAULT_CAMERA_EXPOSURE);
    } catch (ALError &e) {
        log->error("ALImageTranscriber", "Couldn't set Exposure");
    }
	param = camera->call<int>("getParam", kCameraExposureID);
	if (param != DEFAULT_CAMERA_EXPOSURE) {
		try {
			camera->callVoid("setParam", kCameraExposureID,
							 DEFAULT_CAMERA_EXPOSURE);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set Exposure AGAIN");
		}
	}
    // Gain
    try {
        camera->callVoid("setParam",kCameraGainID,
                         DEFAULT_CAMERA_GAIN);
    } catch (ALError &e) {
        log->error("ALImageTranscriber", "Couldn't set Gain");
    }
	param = camera->call<int>("getParam", kCameraGainID);
	if (param != DEFAULT_CAMERA_GAIN) {
		try {
			camera->callVoid("setParam", kCameraGainID,
							 DEFAULT_CAMERA_GAIN);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set Gain AGAIN");
		}
	}
    // Saturation
    try {
        camera->callVoid("setParam",kCameraSaturationID,
                         DEFAULT_CAMERA_SATURATION);
    } catch (ALError &e) {
        log->error("ALImageTranscriber", "Couldn't set Saturation");
    }
	param = camera->call<int>("getParam", kCameraSaturationID);
	if (param != DEFAULT_CAMERA_SATURATION) {
		try {
			camera->callVoid("setParam", kCameraSaturationID,
							 DEFAULT_CAMERA_SATURATION);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set Saturation AGAIN");
		}
	}
    // Hue
    try {
        camera->callVoid("setParam",kCameraHueID,
                         DEFAULT_CAMERA_HUE);
    } catch (ALError &e) {
        log->error("ALImageTranscriber", "Couldn't set Hue");
    }
	param = camera->call<int>("getParam", kCameraHueID);
	if (param != DEFAULT_CAMERA_HUE) {
		try {
			camera->callVoid("setParam", kCameraHueID,
							 DEFAULT_CAMERA_HUE);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set Hue AGAIN");
		}
	}
    // Lens correction X
    try {
        camera->callVoid("setParam",kCameraLensXID,
                         DEFAULT_CAMERA_LENSX);
    } catch (ALError &e) {
        log->error("ALImageTranscriber", "Couldn't set Lens Correction X");
    }
	param = camera->call<int>("getParam", kCameraLensXID);
	if (param != DEFAULT_CAMERA_LENSX) {
		try {
			camera->callVoid("setParam", kCameraLensXID,
							 DEFAULT_CAMERA_LENSX);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set Lens Correction X AGAIN");
		}
	}
    // Lens correction Y
    try {
        camera->callVoid("setParam",kCameraLensYID,
                         DEFAULT_CAMERA_LENSY);
    } catch (ALError &e) {
        log->error("ALImageTranscriber", "Couldn't set Lens Correction Y");
    }
	param = camera->call<int>("getParam", kCameraLensYID);
	if (param != DEFAULT_CAMERA_LENSY) {
		try {
			camera->callVoid("setParam", kCameraLensYID,
							 DEFAULT_CAMERA_LENSY);
		} catch (ALError &e){
			log->error("ALImageTranscriber", "Couldn't set Lens Correction Y AGAIN");
		}
	}
}


void ALImageTranscriber::waitForImage ()
{
    try {
#ifndef MAN_IS_REMOTE
#ifdef DEBUG_IMAGE_REQUESTS
        printf("Requesting local image of size %ix%i, color space %i\n",
               IMAGE_WIDTH, IMAGE_HEIGHT, NAO_COLOR_SPACE);
#endif
        ALVisionImage *ALimage = NULL;

        // Attempt to retrieve the next image
        try {
            ALimage = (ALVisionImage*) (camera->call<int>("getDirectRawImageLocal",lem_name));
        }catch (ALError &e) {
            log->error("NaoMain", "Could not call the getImageLocal method of the "
                       "NaoCam module");
        }
        if (ALimage != NULL) {
            memcpy(&image[0], ALimage->getFrame(), IMAGE_BYTE_SIZE);
            //image = ALimage->getFrame();
        }
        else
            cout << "\tALVisionImage from camera was null!!" << endl;

#ifdef DEBUG_IMAGE_REQUESTS
        //You can get some informations of the image.
        int width = ALimage->fWidth;
        int height = ALimage->fHeight;
        int nbLayers = ALimage->fNbLayers;
        int colorSpace = ALimage->fColorSpace;
        long long timeStamp = ALimage->fTimeStamp;
        int seconds = (int)(timeStamp/1000000LL);
        printf("Retrieved an image of dimensions %ix%i, color space %i,"
               "with %i layers and a time stamp of %is \n",
               width, height, colorSpace,nbLayers,seconds);
#endif

#else//Frame is remote:
#ifdef DEBUG_IMAGE_REQUESTS
        printf("Requesting remote image of size %ix%i, color space %i\n",
               IMAGE_WIDTH, IMAGE_HEIGHT, NAO_COLOR_SPACE);
#endif
        ALValue ALimage;
        ALimage.arraySetSize(7);

        // Attempt to retrive the next image
        try {
            ALimage = camera->call<ALValue>("getDirectRawImageRemote",
                                            lem_name);
        }catch (ALError &e) {
            log->error("NaoMain", "Could not call the getImageRemote method of the "
                       "NaoCam module");
        }

        //image = static_cast<const unsigned char*>(ALimage[6].GetBinary());
        memcpy(&image[0], ALimage[6].GetBinary(), IMAGE_BYTE_SIZE);
#ifdef DEBUG_IMAGE_REQUESTS
        //You can get some informations of the image.
        int width = (int) ALimage[0];
        int height = (int) ALimage[1];
        int nbLayers = (int) ALimage[2];
        int colorSpace = (int) ALimage[3];
        long long timeStamp = ((long long)(int)ALimage[4])*1000000LL +
            ((long long)(int)ALimage[5]);
        int seconds = (int)(timeStamp/1000000LL);
        printf("Retrieved an image of dimensions %ix%i, color space %i,"
               "with %i layers and a time stamp of %is \n",
               width, height, colorSpace,nbLayers,seconds);
#endif

#endif//IS_REMOTE

        if (image != NULL) {
            // Update Sensors image pointer
            sensors->lockImage();
            sensors->setImage(image);
            sensors->releaseImage();
        }

    }catch (ALError &e) {
        log->error("NaoMain", "Caught an error in run():\n" + e.toString());
    }
}


void ALImageTranscriber::releaseImage(){
#ifndef MAN_IS_REMOTE
    if (!camera_active)
        return;

    //Now you have finished with the image, you have to release it in the V.I.M.
    try
    {
        camera->call<int>( "releaseDirectRawImage", lem_name );
    }catch( ALError& e)
    {
        log->error( "ALImageTranscriber",
                    "could not call the releaseImage method of the NaoCam module" );
    }
#endif
}
