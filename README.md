camera_pi runs on raspberry pi and it detects image changes of the camera. 
Use OpenCV as image processing library.
Use mega sdk to upload file to mega cloud storage.

The camera code will use OpenCV to calculate compare current frame from camera and compare it with the reference frame. If they are quite different it will send a mail and update the current frame to the mega cloud storage.

Users have to provide UserName and Password of the MegaCloud to enable the uploading.

Dependency:

For opencv on raspberry pi.

sudo apt-get install libopencv-dev

For mega sdk (refer to  https://github.com/meganz/sdk)

and the mega sdk requires the following libraries.

sudo apt-get install libcrypto++-dev.

sudo apt-get install libcurl4-openssl-dev.

sudo apt-get install libc-ares-dev.

sudo apt-get install libssl-dev.

sudo apt-get install zlib1g-dev.

sudo apt-get install linsqlite3-dev.

sudo apt-get install libfreeimage-dev.


If everything goes well, just execute make at root dir.
