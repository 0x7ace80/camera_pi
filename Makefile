#OPENCV_INC=`pkg-config --cflags opencv`
#OPENCV_LIB=`pkg-config --libs opencv`

OPENCV_INC += -I/usr/local/include
OPENCV_LIB += -L/usr/local/lib -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_ts -lopencv_video -lopencv_videostab

MEGA_INC =  -I/usr/local/include
MEGA_INC += -I/usr/local/incldue/mega
MEGA_INC += -I/usr/local/include/mega/posix
MEGA_INC += -I/opt/local/include
MEGA_LIB = -L/usr/local/lib -lmega

all:
	rm -rf *.o camera_pi
	g++ $(MEGA_INC) -c megacli.cpp -o megacli.o
	g++ $(OPENCV_INC) $(MEGA_INC) -c camera.cpp megacli.o -o camera.o
	g++ $(OPENCV_LIB) $(MEGA_LIB) -o camera_pi camera.o megacli.o
