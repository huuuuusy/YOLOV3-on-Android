#include <jni.h>
#include "darknet.h"

#include <android/bitmap.h>
#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>

#define true JNI_TRUE
#define false JNI_FALSE


//定义voc数据集使用的20类标签名字
char *voc_names[] = {"aeroplane", "bicycle", "bird",
                     "boat", "bottle", "bus", "car",
                     "cat", "chair", "cow", "diningtable",
                     "dog", "horse", "motorbike", "person",
                     "pottedplant", "sheep", "sofa", "train", "tvmonitor"};

//参考darknet/examples/dector.c Line562~Line626，test_detector作用是调用训练好的参数文件进行输入图片的判断
double test_detector(char *datacfg, char *cfgfile, char *weightfile, char *filename, float thresh, float hier_thresh, char *outfile, int fullscreen)
{
    //在logcat中打印data,cfg,weight,image文件的信息
    LOGD("data=%s",datacfg);
    LOGD("cfg=%s",cfgfile);
    LOGD("wei=%s",weightfile);
    LOGD("img=%s",filename);

// 原版代码：直接使用时路径配置容易出错，打开App后无法解析文件，出现闪退
//    list *options = read_data_cfg(datacfg);
//    char *name_list = option_find_str(options, "names", "data/names.list");
//    char **names = get_labels(name_list);
//　解决方案：把相关文件直接拷贝到手机上，写成绝对路径读取
    char *name_list = "/sdcard/yolo/data/coco.names";
    char **names = get_labels(name_list);

    image **alphabet = load_alphabet();
    network *net = load_network(cfgfile, weightfile, 0);
    set_batch_network(net, 1);
    srand(2222222);
    double time;
    char buff[256];
    char *input = buff;
    float nms=.3;
    while(1){
        if(filename){
            strncpy(input, filename, 256);
        } else {
            printf("Enter Image Path: ");
            fflush(stdout);
            input = fgets(input, 256, stdin);
            if(!input) return 0;
            strtok(input, "\n");
        }
        image im = load_image_color(input,0,0);
        image sized = letterbox_image(im, net->w, net->h);
        //image sized = resize_image(im, net->w, net->h);
        //image sized2 = resize_max(im, net->w);
        //image sized = crop_image(sized2, -((net->w - sized2.w)/2), -((net->h - sized2.h)/2), net->w, net->h);
        //resize_network(net, sized.w, sized.h);
        layer l = net->layers[net->n-1];

        float *X = sized.data;
        time=what_time_is_it_now();
        network_predict(net, X);
        time = what_time_is_it_now()-time;
        LOGD("%s: Predicted in %f seconds.\n", input, time);
        int nboxes = 0;
        detection *dets = get_network_boxes(net, im.w, im.h, thresh, hier_thresh, 0, 1, &nboxes);
        //printf("%d\n", nboxes);
        //if (nms) do_nms_obj(boxes, probs, l.w*l.h*l.n, l.classes, nms);
        if (nms) do_nms_sort(dets, nboxes, l.classes, nms);
        draw_detections(im, dets, nboxes, thresh, names, alphabet, l.classes);
        free_detections(dets, nboxes);
        if(outfile){
            save_image(im, outfile);
        }
        else{
            save_image(im, "predictions");
#ifdef OPENCV
            cvNamedWindow("predictions", CV_WINDOW_NORMAL);
            if(fullscreen){
                cvSetWindowProperty("predictions", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
            }
            show_image(im, "predictions");
            cvWaitKey(0);
            cvDestroyAllWindows();
#endif
        }
        free_image(im);
        free_image(sized);
        if (filename) break;
    }
    //　源码无返回，这里修改为返回执行的时间，time在之前定义为完成判断的执行时间
    return time;
}

// 调用jni对文件进行处理
void JNICALL
Java_com_hushiyu1995_yolo_1v3_Yolo_inityolo(JNIEnv *env, jobject obj, jstring cfgfile, jstring weightfile) {
    const char *cfgfile_str = (*env)->GetStringUTFChars(env, cfgfile, 0);
    const char *weightfile_str = (*env)->GetStringUTFChars(env, weightfile, 0);

    //test_detector(cfgfile_str, weightfile_str, "/sdcard/name", 0.5f);

    (*env)->ReleaseStringUTFChars(env, cfgfile, cfgfile_str);
    (*env)->ReleaseStringUTFChars(env, weightfile, weightfile_str);
    return;
}


//将img传到sdcard上
jdouble JNICALL
Java_com_hushiyu1995_yolo_1v3_Yolo_testyolo(JNIEnv *env, jobject obj, jstring imgfile) {
    double time;
    const char *imgfile_str = (*env)->GetStringUTFChars(env, imgfile, 0);

    char *datacfg_str = "/sdcard/yolo/data/coco.names";
    char *cfgfile_str = "/sdcard/yolo/cfg/yolov3-tiny.cfg";
    char *weightfile_str = "/sdcard/yolo/weights/yolov3-tiny.weights";
    char *outimgfile_str = "/sdcard/yolo/out";

    time = test_detector(datacfg_str, cfgfile_str,
                         weightfile_str, imgfile_str,
                         0.2f, 0.5f, outimgfile_str, 0);

    (*env)->ReleaseStringUTFChars(env, imgfile, imgfile_str);
    return time;
}

jboolean JNICALL
Java_com_hushiyu1995_yolo_1v3_Yolo_detectimg(JNIEnv *env, jobject obj, jobject dst, jobject src) {
    AndroidBitmapInfo srcInfo, dstInfo;
    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_getInfo(env, src, &srcInfo)
        || ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_getInfo(env, dst, &dstInfo)) {
        LOGE("get bitmap info failed");
        return false;
    }

    void *srcBuf, *dstBuf;
    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_lockPixels(env, src, &srcBuf)) {
        LOGE("lock src bitmap failed");
        return false;
    }

    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_lockPixels(env, dst, &dstBuf)) {
        LOGE("lock dst bitmap failed");
        return false;
    }


    AndroidBitmap_unlockPixels(env, src);
    AndroidBitmap_unlockPixels(env, dst);
    return 1;
}
