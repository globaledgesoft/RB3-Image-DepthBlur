**Robotics RB3 - Use Case Project – Depth Based Background Blur**

This project about bluring the image (background information) using depth information image from RB3 device. It also comes with a feature to push the blurred image to AWS S3

**Pre - requisites**

* RB3 Robotics Dev Kit
* Linux os
* Amazon AWS S3


**Building and Transfering Camera Application**

* Download application source from camera_app directory
* Build and transfer the application from host machine to RB3 as mentioned in the RB3 Linux User Guide
* On successful build "hal3_test" will be generated. Follow the steps mentioned in RB3 Linux User Guide to push this image to RB3 device

**Steps to get Depth Based Background Blur Image**


* Run the camera application “hal3_test” on the device. Give the following options on prompt in the following sequence 

  / # hal3_test 

* Capture main camera image

  CAM0>>A:id=2,psize=1920x1080,pformat=yuv420,ssize=3264x2448,sformat=jpeg

  CAM2>>s:1


* Add CAM1(ToF) and capture the depth image with specified format and size and given in the below commands

  CAM2>>A:id=1,psize=640x480,pformat=raw16,dsize=640x480,dformat=raw16

  CAM1>>P:1


* Follow the below instructions to quit the application after deleting the cameras (D: Delete camera device, Q:Quit) 

  CAM1>>D

  CAM1>>Q

* The output images are stored to /data/misc/camera

