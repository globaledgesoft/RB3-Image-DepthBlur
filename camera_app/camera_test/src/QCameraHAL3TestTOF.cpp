////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCameraHAL3TestTOF.cpp
/// @brief Test for TOF
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCameraHAL3TestTOF.h"
#include <unistd.h>
#include <log/log.h>
#include "inttypes.h"
#include <hardware/gralloc1.h>
#include <pthread.h>
#include <stdio.h>

#define LOG_TAG "TEST_TOF"
#include "TestLog.h"

//background_blur
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>


#define STB_IMAGE_IMPLEMENTATION
#include "stb-image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb-image/stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb-image/stb_image_resize.h"



using namespace android;
#define PREVIEW_STREAM 0
#define DEPTH_STREAM 1

//background_blur 
extern char main_cam_img[256];

//background_blur
/************************************************************************
* name : blur_main_cam_snapshot
* function : blur the whole snapshot
************************************************************************/
unsigned char * blurMainCameraSnapshot(unsigned char *img, unsigned char *res, int width, int height, int channels) {
    int c = channels;
    int w = width;
    int h = height;
    for(int k = 0; k < c; k++) {
        for(int j = 0; j < h-5; j++) {
            for(int i = 0; i < w-5; i++) {
                int sum = 0;
                for (int l=j; l<j+5; l++) {
                    for(int m=i; m<i+5; m++) {
                        int img_ptr = k + c*m + c*w*l;
                        sum += img[img_ptr];
                    }
                }
                int res_ptr = k + ((c*i)+6) + ((c*w*j)+(c*w));//change 3 for 3*3 and 6 for 5*5
                res[res_ptr] = (sum/25);
            }
        }
    }
	return res;
}



//background_blur
/************************************************************************
* name : rawToJpg
* function: convert raw16 image to jpeg.
************************************************************************/
void rawToJpg(char fname[]) {

	FILE *fp = fopen(fname, "rb");
	unsigned char data;
	unsigned char *img_data = (unsigned char *)malloc(640*480);
	unsigned short int d=0;
	unsigned int range = 0;

	while(!feof(fp)) {
		for(int i=0; i<2; i++) {
			data = getc(fp);
			d = (d<<8)^data;
		}		
		*(img_data+range) = (unsigned char)((d/(short int)(-1))*255*15);
		d = 0;
		range++;
	}
	
	stbi_write_jpg("depth.jpg",640,480,1,img_data,100);
	free(img_data);
	img_data = NULL;

}

//background_blur
/************************************************************************
* name : MainCameraSnapshotDepthBasedBlur
* function: depth based background blur
************************************************************************/
void MainCameraSnapshotDepthBasedBlur(unsigned char *resize_img, unsigned char *depth_img, unsigned char *res1, int depth_width, int depth_height, int depth_channels) {

	int c = depth_channels;
	int w = depth_width;
	int h = depth_height;
	for(int k = 0; k < c; k++) {
		for(int j = 0; j < h-5; j++) {
			for(int i = 0; i < w-5; i++) {
				int sum = 0;
				for (int l=j; l<j+5; l++) {
					for(int m=i; m<i+5; m++) {
						int img_ptr = k + c*m + c*w*l;
						sum += resize_img[img_ptr];
					}
				}
				int res_ptr = k + ((c*i)+6) + ((c*w*j)+(c*w));//change 3 for 3*3 and 6 for 5*5
				if(depth_img[res_ptr] > 70) {
					res1[res_ptr] = resize_img[res_ptr];
     			} else {
							res1[res_ptr] = (sum/25);
				}
			}
		}
	}
}

/************************************************************************
* name : QCameraHAL3TestTOF
* function: construct object.
************************************************************************/
QCameraHAL3TestTOF::QCameraHAL3TestTOF(camera_module_t* module,
    QCameraHAL3Device* device,
    TestConfig* config)
{
    mModule = module;
    mConfig = config;
    mDevice = device;

    mDumpPreviewNum = 0;
    mFrameCount = 0;
    mLastFrameCount = 0;
    mLastFpsTime = 0;

    mMetadataExt = NULL;
    mFfbmDepthCb = NULL;
}

/************************************************************************
* name : ~QCameraHAL3TestTOF
* function: destory object.
************************************************************************/
QCameraHAL3TestTOF::~QCameraHAL3TestTOF()
{

}

/************************************************************************
* name : getCurrentMeta
* function: Get current metadata setting.
************************************************************************/
android::CameraMetadata* QCameraHAL3TestTOF::getCurrentMeta()
{
    return &(mDevice->mCurrentMeta);
}

/************************************************************************
* name : setCurrentMeta
* function: Set A External metadata as current metadata. this is used by nonZSL
************************************************************************/
void QCameraHAL3TestTOF::setCurrentMeta(android::CameraMetadata* meta)
{
    mMetadataExt = meta;
}
/************************************************************************
* name : updataMetaDatas
* function: update request command.
************************************************************************/
void QCameraHAL3TestTOF::updataMetaDatas(android::CameraMetadata* meta)
{
    mDevice->updateMetadataForNextRequest(meta);
}

/************************************************************************
* name : HandleMetaData
* function: analysis meta info from capture result.
************************************************************************/
void QCameraHAL3TestTOF::HandleMetaData(DeviceCallback* cb, camera3_capture_result *result)
{
    TEST_INFO("HandleMetaData Enter\n");
}

/************************************************************************
* name : showPreviewFPS
* function: show preview frame FPS.
************************************************************************/
void QCameraHAL3TestTOF::showPreviewFPS()
{
    double fps = 0;
    mFrameCount++;
    nsecs_t now = systemTime();
    nsecs_t diff = now - mLastFpsTime;
    if (diff > s2ns(2) ) {
        fps = (((double)(mFrameCount - mLastFrameCount)) *
                (double)(s2ns(1))) / (double)diff;
        ALOGI("PROFILE_PREVIEW_FRAMES_PER_SECOND CAMERA %d: %.4f: mFrameCount=%d",
              mConfig->mCameraId, fps, mFrameCount);
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
    }
}

void QCameraHAL3TestTOF::setFfbmRawCb(void (*cb)(void*, int)) {
    TEST_INFO("%s:%d register", __func__, __LINE__);
    mFfbmDepthCb = cb;
}

/************************************************************************
* name : CapturePostProcess
* function: callback for postprocess capture result.
************************************************************************/
void QCameraHAL3TestTOF::CapturePostProcess(DeviceCallback* cb, camera3_capture_result *result)
{
    //TEST_INFO("CapturePostProcess Enter\n");
    const camera3_stream_buffer_t* buffers = NULL;
    QCameraHAL3TestTOF* testtof = (QCameraHAL3TestTOF*)cb;
    QCameraHAL3Device* device = testtof->mDevice;
    buffers = result->output_buffers;
    for (uint32_t i = 0;i < result->num_output_buffers ;i++) {
        int index = device->findStream(buffers[i].stream);
        CameraStream* stream = device->mCameraStreams[index];
        // Dump frame
        char fname[256];
        time_t timer;
        struct tm * t;
        time(&timer);
        if (stream->streamType == CAMERA3_TEMPLATE_STILL_CAPTURE && testtof->mDumpPreviewNum > 0) {
            t = localtime(&timer);
            testtof->mDumpPreviewNum--;
            sprintf(fname, "%s/camera_irbg_%d_%4d_%02d_%02d_%02d_%02d_%02d.%s",
                CAMERA_STORAGE_DIR,result->frame_number,t->tm_year+1900,t->tm_mon+1,
                t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec,"raw");
            FILE* fd = fopen(fname, "wb");
            BufferInfo* info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
            int size = info->size;
            void* data = info->vaddr;
            fwrite(data, size, 1, fd);
            fclose(fd);

	}
        if (stream->streamType == CAMERA3_TEMPLATE_PREVIEW && testtof->mDumpPreviewNum > 0) {
            t = localtime(&timer);
            testtof->mDumpPreviewNum--;
            sprintf(fname, "%s/camera_depth_%d_%4d_%02d_%02d_%02d_%02d_%02d.%s",
                CAMERA_STORAGE_DIR,result->frame_number,t->tm_year+1900,t->tm_mon+1,
                t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec,"raw");
            FILE* fd = fopen(fname, "wb");
            BufferInfo* info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
            int size = info->size;
            void* data = info->vaddr;
            fwrite(data, size, 1, fd);
            fclose(fd);
	    	//background_blur
			if(main_cam_img[0] == '\0') { 
				printf("main camera snapshot not taken \n");
	    	} else {
					
					printf("main camera snapshot taken \n");
                	//main camera snapshot blurring code starts
                	char main_cam_img_blur[256];
                	time_t begin,end;
                	begin= time(NULL);
                	int width, height, channels;
                	unsigned char *img = stbi_load(main_cam_img, &width, &height, &channels, 0);
					rawToJpg(fname);
					int depth_height, depth_width, depth_channels;
					unsigned char *depth_img = stbi_load("depth.jpg",&depth_width,&depth_height,&depth_channels,0);
					unsigned char *resize_img = (unsigned char *)malloc(depth_width * depth_height * depth_channels);
					stbir_resize_uint8( img, width, height, 0, resize_img , depth_width, depth_height, 0, depth_channels);
                	unsigned char *res = (unsigned char *)malloc(depth_width*depth_height*depth_channels);
                	res = blurMainCameraSnapshot(resize_img, res, depth_width, depth_height, depth_channels);
                	sprintf(main_cam_img_blur, "%s/main_camera_blur_snapshot%d_%4d_%02d_%02d_%02d_%02d_%02d.%s",
                    	CAMERA_STORAGE_DIR, result->frame_number,t->tm_year+1900,t->tm_mon+1,
                    	t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec,"jpg");
                	stbi_write_jpg(main_cam_img_blur,depth_width,depth_height,depth_channels,res,100);
                	end = time(NULL);
                	printf("Time taken to blur main camera snapshot is %f \n", difftime(end, begin));
                	//main camera snapshot blurring code ends
					//main camera snapshot depth based blur starts
					begin= time(NULL);
					unsigned char *res1 = (unsigned char *)malloc(depth_width * depth_height * depth_channels);
		 			MainCameraSnapshotDepthBasedBlur(resize_img, depth_img, res1, depth_width, depth_height, depth_channels);
		   			char res_fname[256];
		   			sprintf(res_fname, "%s/main_camera_snapshot_depth_based_blur_%d_%4d_%02d_%02d_%02d_%02d_%02d.%s",
                   	CAMERA_STORAGE_DIR,result->frame_number,t->tm_year+1900,t->tm_mon+1,
                   	t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec,"jpg");
		   			stbi_write_jpg(res_fname,depth_width,depth_height,depth_channels,res1,100);
					free(img);
					free(depth_img);
					free(resize_img);
					free(res);
					free(res1);
					img = NULL;
					depth_img = NULL;
					resize_img = NULL;
					res = NULL;
					res1 = NULL;
		   			end = time(NULL);
		   			printf("Time taken for main camera snapshot depth based blur is %f \n", difftime(end, begin));
	    	}
			//main camera snapshot depth based blur ends
        }
        // FPS Show
        if (stream->streamType == CAMERA3_TEMPLATE_PREVIEW && mConfig->mShowFps) {
            showPreviewFPS();
        }
    }
}

/************************************************************************
* name : initTofStreams
* function: private function for init streams for depth.
************************************************************************/
int QCameraHAL3TestTOF::initTofStreams()
{
    int res = 0;
    camera3_stream_t previewStream;
    camera3_stream_t depthStream;
    std::vector<camera3_stream_t*> streams;
    int stream_num = 2;
    //init stream configure
    bool supportsPartialResults;
    uint32_t partialResultCount;
    std::vector<AvailableStream> outputPreviewStreams;
    AvailableStream previewThreshold = {mConfig->mPreviewStream.width,
        mConfig->mPreviewStream.height,mConfig->mPreviewStream.format};
    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(mDevice->mCharacteristics,
            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = mDevice->getAvailableOutputStreams(outputPreviewStreams, &previewThreshold);
    }
    if (res < 0 || outputPreviewStreams.size() == 0) {
        TEST_ERR("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
            mConfig->mPreviewStream.width, mConfig->mPreviewStream.height,
            mConfig->mPreviewStream.format);
        return -1;
    }

    previewStream.stream_type = CAMERA3_STREAM_OUTPUT;
    previewStream.width = outputPreviewStreams[0].width;
    previewStream.height = outputPreviewStreams[0].height;
    previewStream.format = outputPreviewStreams[0].format;
    previewStream.data_space = HAL_DATASPACE_UNKNOWN;
    // for Full Test UseCase
    previewStream.usage = GRALLOC1_CONSUMER_USAGE_NONE;
    previewStream.rotation = 0;
    // Fields to be filled by HAL (max_buffers, priv) are initialized to 0
    previewStream.max_buffers = 0;
    previewStream.priv = 0;

    std::vector<AvailableStream> outputDepthStreams;
    AvailableStream depthThreshold = {mConfig->mDepthStream.width,
        mConfig->mDepthStream.height,mConfig->mDepthStream.format};
    if (res == 0){
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(mDevice->mCharacteristics,
            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = mDevice->getAvailableOutputStreams(outputDepthStreams, &depthThreshold);
    }
    if (res < 0 || outputDepthStreams.size() == 0) {
        TEST_ERR("Failed to find output stream for depth: w: %d, h: %d, fmt: %d",
            mConfig->mDepthStream.width, mConfig->mDepthStream.height,
            mConfig->mDepthStream.format);
        return -1;
    }

    depthStream.stream_type = CAMERA3_STREAM_OUTPUT;
    depthStream.width = outputDepthStreams[0].width;
    depthStream.height = outputDepthStreams[0].height;
    depthStream.format = outputDepthStreams[0].format;
    depthStream.data_space = HAL_DATASPACE_UNKNOWN;
    depthStream.usage = 0;
    depthStream.rotation = 0;
    // Fields to be filled by HAL (max_buffers, priv) are initialized to 0
    depthStream.max_buffers = 0;
    depthStream.priv = 0;

    streams.resize(stream_num);
    streams[PREVIEW_STREAM] = &previewStream;
    streams[DEPTH_STREAM] = &depthStream;

    mDevice->setPutBufferExt(PUT_BUFFER_INTERNAL);
    mDevice->setFpsRange(mConfig->mFpsRange[0], mConfig->mFpsRange[1]);
    mDevice->configureStreams(streams);
    mDevice->constructDefaultRequestSettings(PREVIEW_STREAM,CAMERA3_TEMPLATE_PREVIEW, true);
    mDevice->constructDefaultRequestSettings(DEPTH_STREAM,CAMERA3_TEMPLATE_STILL_CAPTURE);

    return res;
}

int QCameraHAL3TestTOF::PreinitTofStreams()
{
    int res = 0;
    TEST_INFO("preinit TOF streams start\n");
    camera3_stream_t previewStream;
    camera3_stream_t depthStream;
    std::vector<camera3_stream_t*> streams;
    int stream_num = 2;
    int * diff_stream0 = new int;
    *diff_stream0 = 0;
     int * diff_stream1 = new int;
     *diff_stream1 = 1;

    previewStream.stream_type = CAMERA3_STREAM_OUTPUT;
    previewStream.width = mConfig->mPreviewStream.width;
    previewStream.height = mConfig->mPreviewStream.height;
    previewStream.format = mConfig->mPreviewStream.format;
    previewStream.data_space = HAL_DATASPACE_UNKNOWN;
    previewStream.usage = 0;
    previewStream.rotation = 0;
    previewStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    previewStream.priv = (void *)diff_stream0;

    depthStream.stream_type = CAMERA3_STREAM_OUTPUT;
    depthStream.width = mConfig->mDepthStream.width;
    depthStream.height = mConfig->mDepthStream.height;
    depthStream.format = mConfig->mDepthStream.format;
    depthStream.data_space = HAL_DATASPACE_UNKNOWN;
    depthStream.usage = 0;
    depthStream.rotation = 0;
    depthStream.max_buffers = DEPTH_STREAM_BUFFER_MAX;
    depthStream.priv = (void *)diff_stream1;

    streams.resize(stream_num);
    streams[PREVIEW_STREAM] = &previewStream;
    streams[DEPTH_STREAM] = &depthStream;

    mDevice->PreAllocateStreams(streams);
    TEST_INFO("preinit TOF streams end\n");
    return res;
}

/************************************************************************
* name : run
* function: interface for create depth thread.
************************************************************************/
void QCameraHAL3TestTOF::run()
{
    //open camera
    int res = 0;
    printf("CameraId:%d\n",mConfig->mCameraId);
    mDevice->setCallBack(this);
    mLastFpsTime = systemTime();
    res = initTofStreams();
    CameraThreadData* resultThread = new CameraThreadData();
    CameraThreadData* requestThread = new CameraThreadData();
    requestThread->requestNumber[PREVIEW_STREAM] = REQUEST_NUMBER_UMLIMIT;
    requestThread->requestNumber[DEPTH_STREAM] = REQUEST_NUMBER_UMLIMIT;

    mDevice->processCaptureRequestOn(requestThread,resultThread);
}

/************************************************************************
* name : stop
* function:  stop all the thread and release the device object.
************************************************************************/
void QCameraHAL3TestTOF::stop()
{
    mDevice->stopStreams();
}

/************************************************************************
* name : dumpPreview
* function: set frame count of preveiw dump.
************************************************************************/
void QCameraHAL3TestTOF::dumpPreview(int count)
{
    //mDumpPreviewNum += count;
    mDumpPreviewNum = count*2;
}

/************************************************************************
* name : TofLoadEeprom
* function: Load tof eeprom data
************************************************************************/
int QCameraHAL3TestTOF::TofLoadEeprom()
{
	/*load eeprom*/
	FILE *fp;
	char *eeprom_path = "/sys/TOFsensor/eepromdata/eeprom_bin";
	int bytes;
	if ((fp = fopen(eeprom_path, "rb")) == NULL) {
		printf("Failed to load eeprom\n");
		return -1;
	} else {
		bytes = fread(&dev_eeprom, sizeof(tl_dev_eeprom), 1, fp);
		if (bytes == 0) {
			printf("The eeprom version is not supported\n");
			return -1;
		}
		fclose(fp);
	}
	return 0;
}

/************************************************************************
* name : TofInitStruct
* function: Init tof struct
************************************************************************/
void QCameraHAL3TestTOF::TofInitStruct()
{
	temp_val.idle_delay = 0;
	temp_val.ini_ofst_delay = 0;
	temp_val.exp_hd = 0;
	temp_val.tl_transmit.mode = 0;
	temp_val.tl_transmit.image_type_output_sel = 1;
	temp_val.tl_transmit.external_sync = 0;
}

/************************************************************************
* name : TofAfeCalculate
* function: Calculate tof eepromdata
************************************************************************/
int QCameraHAL3TestTOF::TofAfeCalculate()
{
	int ini_ofst_delay;
	int idle_delay;
	ini_ofst_delay = dev_eeprom.mode[0].exp_prm.vd_ini_ofst;
	if(dev_eeprom.mode[0].exp_prm.afe_idle_val[0] == 0){
		idle_delay = 0;
	} else {
		idle_delay = dev_eeprom.mode[0].exp_prm.afe_idle_val[0] + 2;
	}

	if (dev_eeprom.mode[0].exp_prm.idle_peri_num == 0) {
		idle_delay = 0;
	} else if (dev_eeprom.mode[0].exp_prm.idle_peri_adr[0] == 0 &&
			   dev_eeprom.mode[0].exp_prm.idle_peri_num >= 1) {
		idle_delay = 0;
	}

	if (dev_eeprom.mode[0].exp_prm.vd_ini_ofst_adr_num == 0) {
		ini_ofst_delay = 0;
	}

	if (((dev_eeprom.cmn.cam_info.enable_mode >> 0) & 0x0001U) == 0U) {
		printf("this is not support\n");
		return -1;
	}

	temp_val.idle_delay = idle_delay;
	temp_val.ini_ofst_delay = ini_ofst_delay;

	exp_val = dev_eeprom.mode[0].exp.exp_default;
	if (exp_val > dev_eeprom.mode[0].exp.exp_max) {
		exp_val = dev_eeprom.mode[0].exp.exp_max;
	}
}

/************************************************************************
* name : TofTransmit
* function: Transfer data to kernel
************************************************************************/
int QCameraHAL3TestTOF::TofTransmit()
{
	int ret = 0;
	FILE *fp;
	char *transmit_path = "/sys/TOFsensor/config/config";
	if ((fp = fopen(transmit_path, "wb+")) == NULL) {
		printf("open config failed\n");
		return 0;
	} else {
		fwrite(&temp_val.tl_transmit, sizeof(tl_transmit_kernel), 1, fp);
		fclose(fp);
	}
	return ret;
}

/************************************************************************
* name : TofCalExp
* function: Calculate exposure
************************************************************************/
int QCameraHAL3TestTOF::TofCalExp()
{
	uint32_t	pls_num = 0;
	int64_t		clk;
	int32_t		hd, dmy_hd;
	float		tmp;
	int		i;

	/* exposure setting */
	temp_val.tl_transmit.p_exp.long_val  = exp_val;
	temp_val.tl_transmit.p_exp.short_val = exp_val / 4U;
	temp_val.tl_transmit.p_exp.lms_val   = exp_val - (exp_val / 4U) - 1U;

	if(dev_eeprom.mode[0].exp_prm.num_clk_in_hd == 0U){	return -1;	}

	/* [a] calculate number of clocks in A0/A1/A2 */
	for(i=0; i<(int)TL_AFE_ARY_SIZE(dev_eeprom.mode[0].pls_mod.val); i++){
		pls_num += (uint32_t)dev_eeprom.mode[0].pls_mod.val[i];
	}
	tmp = ((float)pls_num * (float)exp_val) / 40.0F;
	clk = (((int64_t)dev_eeprom.mode[0].exp_prm.tof_seq_ini_ofst +
				((int64_t)exp_val * (int64_t)dev_eeprom.mode[0].exp_prm.ld_pls_duty)) -
			(int64_t)2LL) + (int64_t)lroundf(tmp);
	if(clk < 0){	return -1;	}

	/* [b] calculate number of HDs in A0/A1/A2 */
	tmp = (float)clk / ((float)dev_eeprom.mode[0].exp_prm.num_clk_in_hd * 1.0F);
	hd  = (int32_t)ceilf(tmp);

	/* [c] calculate number of HDs in exposure */
	temp_val.exp_hd = ((hd + hd + hd) *
			(int32_t)dev_eeprom.mode[0].exp_prm.beta_num) +
		(int32_t)dev_eeprom.mode[0].exp_prm.tof_emt_period_ofst;
	if(temp_val.exp_hd > (int32_t)(0xFFFFU -
				(uint16_t)TL_AFE_READ_SIZE_OFFSET)){
		return -1;
	}
	if(temp_val.exp_hd > (int32_t)(0xFFFFU - (uint16_t)TL_AFE_START_V_OFFSET)){
		return -1;
	}

	/* [d] calculate number of HDs in CCD dummy transfer */
	dmy_hd =
		(int32_t)dev_eeprom.mode[0].exp_prm.vd_duration -
		(int32_t)temp_val.ini_ofst_delay -
		temp_val.exp_hd - (int32_t)dev_eeprom.mode[0].exp_prm.num_hd_in_readout;

	if(dmy_hd < 0){	return -1;	}
	if(dmy_hd > 0xFFFFL){ return -1; }

	/* calculate read size */
	temp_val.tl_transmit.p_exp.idle = (temp_val.idle_delay == 0) ? 0 : (temp_val.idle_delay - 2U);

    temp_val.tl_transmit.p_exp.ini_ofst = (temp_val.ini_ofst_delay == 0) ? 0 : (temp_val.ini_ofst_delay - 1U);
	temp_val.tl_transmit.p_exp.read_size2 =
		temp_val.ini_ofst_delay +
		(uint16_t)temp_val.exp_hd +
		TL_AFE_READ_SIZE_OFFSET + temp_val.idle_delay;

	/* calculate ccd dummy transfer */
	temp_val.tl_transmit.p_exp.ccd_dummy    = (uint16_t)dmy_hd - temp_val.idle_delay;
	/* calculate start_v */
	temp_val.tl_transmit.p_exp.chkr_start_v = (uint16_t)temp_val.exp_hd + TL_AFE_START_V_OFFSET;
#if 0
	printf("ini_ofst_delay: %#x,exp_hd: %#x, TL_AFE:%#x,idle_delay: %#x\n",
			temp_val.ini_ofst_delay,
			(uint16_t)temp_val.exp_hd,
			TL_AFE_READ_SIZE_OFFSET,
			temp_val.idle_delay);
	printf("temp_val.tl_transmit.p_exp.idle: %#x, temp_val.tl_transmit.p_exp.ini_ofst: %#x,\
			temp_val.tl_transmit.p_exp.read_size2: %#x, temp_val.tl_transmit.p_exp.ccd_dummy: %#x,\
			temp_val.tl_transmit.p_exp.chkr_start_v: %#x\n"
		   "temp_val.tl_transmit.p_exp.long_val: %#x, temp_val.tl_transmit.p_exp.short_val: %#x,\
		   temp_val.tl_transmit.p_exp.lms_val: %#x\n\n",
		   temp_val.tl_transmit.p_exp.idle,temp_val.tl_transmit.p_exp.ini_ofst,
		   temp_val.tl_transmit.p_exp.read_size2,temp_val.tl_transmit.p_exp.ccd_dummy,
		   temp_val.tl_transmit.p_exp.chkr_start_v,
		   temp_val.tl_transmit.p_exp.long_val,temp_val.tl_transmit.p_exp.short_val,temp_val.tl_transmit.p_exp.lms_val);
#endif
	return 0;
}
