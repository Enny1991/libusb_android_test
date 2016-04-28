//
// Created by enea on 28.04.16.
//

/*
 *
 * Dumb userspace USB Audio receiver
 * Copyright 2012 Joel Stanley <joel@jms.id.au>
 *
 * Based on the following:
 *
 * libusb example program to measure Atmel SAM3U isochronous performance
 * Copyright (C) 2012 Harald Welte <laforge@gnumonks.org>
 *
 * Copied with the author's permission under LGPL-2.1 from
 * http://git.gnumonks.org/cgi-bin/gitweb.cgi?p=sam3u-tests.git;a=blob;f=usb-benchmark-project/host/benchmark.c;h=74959f7ee88f1597286cd435f312a8ff52c56b7e
 *
 * An Atmel SAM3U test firmware is also available in the above repository.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libusb.h"

#include <jni.h>

#include <android/log.h>
#define LOGD(...) \
    __android_log_print(ANDROID_LOG_DEBUG, "UsbAudioNative", __VA_ARGS__)

#define UNUSED __attribute__((unused))

/* The first PCM stereo AudioStreaming endpoint. */
#define EP_ISO_IN	0x06
#define IFACE_NUM   2


//#define VID 0x0D8C
//#define PID 0x0102
static uint16_t VID, PID;
#define usb_interface interface

static int do_exit = 1;
static struct libusb_device_handle *devh = NULL;

static unsigned long num_bytes = 0, num_xfer = 0;
static struct timeval tv_start;

static JavaVM* java_vm = NULL;

static jclass au_id_jms_usbaudio_AudioPlayback = NULL;
static jmethodID au_id_jms_usbaudio_AudioPlayback_write;

struct libusb_bos_descriptor *bos_desc;

libusb_device_handle *handle;
libusb_device *dev;
uint8_t bus, port_path[8];
struct libusb_bos_descriptor *bos_desc;
struct libusb_config_descriptor *conf_desc;
const struct libusb_endpoint_descriptor *endpoint;
	int i, j, k, r;
	int iface, nb_ifaces, first_iface = -1;
	struct libusb_device_descriptor dev_desc;
	const char* speed_name[5] = { "Unknown", "1.5 Mbit/s (USB LowSpeed)", "12 Mbit/s (USB FullSpeed)",
		"480 Mbit/s (USB HighSpeed)", "5000 Mbit/s (USB SuperSpeed)"};
	char string[128];
	uint8_t string_index[3];	// indexes of the string descriptors
	uint8_t endpoint_in = 0, endpoint_out = 0;	// default IN and OUT endpoints
      /* pointer to native method interface */
JavaVMInitArgs vm_args; /* JDK/JRE 6 VM initialization arguments */



static void cb_xfr(struct libusb_transfer *xfr)
{

    LOGD("ENTERED CB");
	unsigned int i;

    int len = 0;

    // Get an env handle
    JNIEnv * env;
    void * void_env;
    bool had_to_attach = false;
    jint status = (*java_vm)->GetEnv(java_vm, &void_env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        (*java_vm)->AttachCurrentThread(java_vm, &env, NULL);
        had_to_attach = true;
    } else {
        env = void_env;
    }

    // Create a jbyteArray.
    int start = 0;
    jbyteArray audioByteArray = (*env)->NewByteArray(env, 192 * xfr->num_iso_packets);

    for (i = 0; i < xfr->num_iso_packets; i++) {
        struct libusb_iso_packet_descriptor *pack = &xfr->iso_packet_desc[i];

        if (pack->status != LIBUSB_TRANSFER_COMPLETED) {
            LOGD("Error (status %d: %s) :", pack->status,
                    libusb_error_name(pack->status));
            /* This doesn't happen, so bail out if it does. */
            exit(EXIT_FAILURE);
        }

        const uint8_t *data = libusb_get_iso_packet_buffer_simple(xfr, i);
        (*env)->SetByteArrayRegion(env, audioByteArray, len, pack->length, data);

        len += pack->length;
    }

    // Call write()
    (*env)->CallStaticVoidMethod(env, au_id_jms_usbaudio_AudioPlayback,
            au_id_jms_usbaudio_AudioPlayback_write, audioByteArray);
    (*env)->DeleteLocalRef(env, audioByteArray);
    if ((*env)->ExceptionCheck(env)) {
        LOGD("Exception while trying to pass sound data to java");
        return;
    }

	num_bytes += len;
	num_xfer++;

    if (had_to_attach) {
        (*java_vm)->DetachCurrentThread(java_vm);
    }


	if (libusb_submit_transfer(xfr) < 0) {
		LOGD("error re-submitting URB\n");
		exit(1);
	}
}

#define NUM_TRANSFERS 10
#define PACKET_SIZE 200
#define NUM_PACKETS 20

int rx_decode_buffer(const short *buf_48k_stereo, int ll) {


    int ret = 0, i;
// Get an env handle
    JNIEnv * env;
    void * void_env;
    bool had_to_attach = false;
    jint status = (*java_vm)->GetEnv(java_vm, &void_env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        (*java_vm)->AttachCurrentThread(java_vm, &env, NULL);
        had_to_attach = true;
    } else {
        env = void_env;
    }
       int start = 0;


    short bbuf[ll];
    jshortArray audioShortArray = (*env)->NewShortArray(env, PACKET_SIZE * 20);


    for(i = 0; i < ll; i++, buf_48k_stereo += 1) {
        bbuf[i] = *buf_48k_stereo;
    }

    LOGD("BOH %u",bbuf[3]);


    (*env)->CallStaticVoidMethod(env, au_id_jms_usbaudio_AudioPlayback,
                    au_id_jms_usbaudio_AudioPlayback_write, audioShortArray);
            (*env)->DeleteLocalRef(env, audioShortArray);
            if ((*env)->ExceptionCheck(env)) {
                LOGD("Exception while trying to pass sound data to java");
                return 0;
            }

    return 0;
}

static void transfer_cb(struct libusb_transfer *xfr) {

    int rc = 0;
    int len = 0;
    unsigned int i;



    /* All packets are 192 bytes. */
    uint8_t* recv = malloc(PACKET_SIZE * xfr->num_iso_packets);
    uint8_t* recv_next = recv;

    for (i = 0; i < xfr->num_iso_packets; i++) {
        struct libusb_iso_packet_descriptor *pack = &xfr->iso_packet_desc[i];
        if (pack->status != LIBUSB_TRANSFER_COMPLETED) {
            LOGD("Error (status %d: %s)\n", pack->status,
                    libusb_error_name(pack->status));
            continue;
        }
        const uint8_t *data = libusb_get_iso_packet_buffer_simple(xfr, i);
        /* PACKET_SIZE == 192 == pack->length */
        memcpy(recv_next, data, PACKET_SIZE);
        recv_next += PACKET_SIZE;
        len += pack->length;
    }
    /* Sanity check. If this is true, we've overflowed the recv buffer. */
    if (len > PACKET_SIZE * xfr->num_iso_packets) {
        LOGD("Error: incoming transfer had more data than we thought.\n");
        return;
    }
    /* At this point, recv points to a buffer containing len bytes of audio. */

    /* Call freedv. */
    // Call write()N

    rx_decode_buffer((short *)recv, len);



        free(recv);
	if ((rc = libusb_submit_transfer(xfr)) < 0) {
		LOGD("libusb_submit_transfer: %s.\n", libusb_error_name(rc));
	}
}





int usb_start_transfers() {


    LOGD("Setting up transfer...");
	static uint8_t buf[PACKET_SIZE * NUM_PACKETS];
	static struct libusb_transfer *xfr[NUM_TRANSFERS];
	int num_iso_pack = NUM_PACKETS;
    int i;

    for (i=0; i<NUM_TRANSFERS; i++) {
        xfr[i] = libusb_alloc_transfer(num_iso_pack);
        if (!xfr[i]) {
            LOGD("libusb_alloc_transfer failed.\n");
            return -ENOMEM;
        }

                   // set iso packet length

        libusb_fill_iso_transfer(xfr[i], devh, EP_ISO_IN, buf,
                sizeof(buf), num_iso_pack, transfer_cb, NULL, 1000);
        libusb_set_iso_packet_lengths(xfr[i], sizeof(buf)/num_iso_pack);

        libusb_submit_transfer(xfr[i]);
    }

    return 0;
}


static int benchmark_in(uint8_t ep)
{
	static uint8_t buf[PACKET_SIZE * NUM_PACKETS];
	static struct libusb_transfer *xfr[NUM_TRANSFERS];
	int num_iso_pack = NUM_PACKETS;
    int i;

	/* NOTE: To reach maximum possible performance the program must
	 * submit *multiple* transfers here, not just one.
	 *
	 * When only one transfer is submitted there is a gap in the bus
	 * schedule from when the transfer completes until a new transfer
	 * is submitted by the callback. This causes some jitter for
	 * isochronous transfers and loss of throughput for bulk transfers.
	 *
	 * This is avoided by queueing multiple transfers in advance, so
	 * that the host controller is always kept busy, and will schedule
	 * more transfers on the bus while the callback is running for
	 * transfers which have completed on the bus.
	 */


    for (i=0; i<NUM_TRANSFERS; i++) {
        xfr[i] = libusb_alloc_transfer(num_iso_pack);
        if (!xfr[i]) {
            LOGD("Could not allocate transfer");
            return -ENOMEM;
        }else{
        LOGD("Transfer allocated");
        }
        LOGD("Max packet size should be set to: %d",libusb_get_max_iso_packet_size(dev,EP_ISO_IN));
          // set iso packet length
        if(devh == NULL) LOGD("device handle is NULL");
        libusb_fill_iso_transfer(xfr[i], devh, EP_ISO_IN, buf,
                sizeof(buf), num_iso_pack, cb_xfr, NULL, 1000);
        libusb_set_iso_packet_lengths(xfr[i], sizeof(buf)/num_iso_pack);

        libusb_submit_transfer(xfr[i]);
    }


	gettimeofday(&tv_start, NULL);

    return 1;
}

unsigned int measure(void)
{
	struct timeval tv_stop;
	unsigned int diff_msec;

	gettimeofday(&tv_stop, NULL);

	diff_msec = (tv_stop.tv_sec - tv_start.tv_sec)*1000;
	diff_msec += (tv_stop.tv_usec - tv_start.tv_usec)/1000;

	printf("%lu transfers (total %lu bytes) in %u miliseconds => %lu bytes/sec\n",
		num_xfer, num_bytes, diff_msec, (num_bytes*1000)/diff_msec);

    return num_bytes;
}




int main(int argc, char** argv){
    LOGD("entered main");


    /*
    JNIEnv * env;
    void * void_env;
    bool had_to_attach = false;
    jint status = (*java_vm)->GetEnv(java_vm, &void_env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        (*java_vm)->AttachCurrentThread(java_vm, &env, NULL);
        had_to_attach = true;
    } else {
        env = void_env;
    }
    */



    bool show_help = false;
    	bool debug_mode = false;
    	const struct libusb_version* version;
    	int j, r;
    	size_t i, arglen;
    	unsigned tmp_vid, tmp_pid;
    	uint16_t endian_test = 0xBE00;
    	char *error_lang = NULL, *old_dbg_str = NULL, str[256];

    	libusb_device_handle *handle;
    	libusb_device *dev;
    	uint8_t bus, port_path[8];
    	struct libusb_bos_descriptor *bos_desc;
    	struct libusb_config_descriptor *conf_desc;
    	struct libusb_device_descriptor dev_desc;

    	// Default to generic, expecting VID:PID
    	VID = 0;
    	PID = 0;

    	if (((uint8_t*)&endian_test)[0] == 0xBE) {
    		printf("Despite their natural superiority for end users, big endian\n"
    			"CPUs are not supported with this program, sorry.\n");
    		return 0;
    	}
    	printf("Called main. \n");
    	if (argc >= 2) {
    		for (j = 1; j<argc; j++) {
    			arglen = strlen(argv[j]);
    			if ( ((argv[j][0] == '-') || (argv[j][0] == '/'))
    			  && (arglen >= 2) ) {
    				switch(argv[j][1]) {
    				case 'd':
    					break;
    				case 'i':
    					break;
    				case 'w':
    					break;
    				default:
    					show_help = true;
    					break;
    				}
    			} else {
    				for (i=0; i<arglen; i++) {
    					if (argv[j][i] == ':')
    						break;
    				}
    				if (i != arglen) {
    					if (sscanf(argv[j], "%x:%x" , &tmp_vid, &tmp_pid) != 2) {
    						printf("   Please specify VID & PID as \"vid:pid\" in hexadecimal format\n");
    						return 1;
    					}
    					VID = (uint16_t)tmp_vid;
    					PID = (uint16_t)tmp_pid;
    				} else {
    					show_help = true;
    				}
    			}
    		}
    	}

    	if ((show_help) || (argc == 1) || (argc > 7)) {
    		printf("usage: %s [-h] [-d] [-i] [-k] [-b file] [-l lang] [-j] [-x] [-s] [-p] [-w] [vid:pid]\n", argv[0]);
    		printf("   -h      : display usage\n");
    		printf("   -d      : enable debug output\n");
    		printf("   -i      : print topology and speed info\n");
    		printf("   -j      : test composite FTDI based JTAG device\n");
    		printf("   -k      : test Mass Storage device\n");
    		printf("   -b file : dump Mass Storage data to file 'file'\n");
    		printf("   -p      : test Sony PS3 SixAxis controller\n");
    		printf("   -s      : test Microsoft Sidewinder Precision Pro (HID)\n");
    		printf("   -x      : test Microsoft XBox Controller Type S\n");
    		printf("   -l lang : language to report errors in (ISO 639-1)\n");
    		printf("   -w      : force the use of device requests when querying WCID descriptors\n");
    		printf("If only the vid:pid is provided, xusb attempts to run the most appropriate test\n");
    		return 0;
    	}

    	//
    	int rc,k;

        	rc = libusb_init(NULL);
        	if (rc < 0) {
        		LOGD("Error initializing libusb: %s\n", libusb_error_name(rc));
                return false;
        	}
        	LOGD("Init LIBUSB: %s", libusb_error_name(rc));


        	//version = libusb_get_version();
        	//LOGD("Using libusb v%d.%d.%d.%d\n\n", version->major, version->minor, version->micro, version->nano);

            /* This device is the TI PCM2900C Audio CODEC default VID/PID. */
        	devh = libusb_open_device_with_vid_pid(NULL, VID, PID);
        	if (!devh) {
        		LOGD("Error finding USB device\n");
                libusb_exit(NULL);
                return false;
        	}else{
        	LOGD("Device handle properly captured.");
        	}

        	// setup transfer of all possible interfaces
        	dev = libusb_get_device(devh);
        	if(!dev){
        	LOGD("Failed to connected from handle.");
        	}else{
        	LOGD("Connected to device.");
        	}

        	rc = libusb_get_device_descriptor(dev, &dev_desc);
            LOGD("Getting Device descriptor: %s", libusb_error_name(rc));
        	rc = libusb_get_config_descriptor(dev, 0, &conf_desc);
        	LOGD("Getting config descriptor: %s", libusb_error_name(rc));
        	rc = libusb_get_bos_descriptor(devh, &bos_desc);
        	LOGD("Getting bos descriptor: %s", libusb_error_name(rc));
        	nb_ifaces = conf_desc->bNumInterfaces;

        	//r = libusb_set_auto_detach_kernel_driver(devh, 1);
        	//LOGD("Try auto-detach: %s",libusb_error_name(r));
/*
            for (iface = 0; iface < nb_ifaces; iface++)
            	{
            	    LOGD("Claiming iface %d ...", iface);
            		r = libusb_claim_interface(devh, iface);
            		if (r != LIBUSB_SUCCESS) {
            		LOGD("Failed to claim iface %d: %s. Detaching kernel interface...", iface, libusb_error_name(r));
            		    rc = libusb_detach_kernel_driver(devh, iface);
            		    LOGD("Detaching kernel iface: %s", libusb_error_name(rc));
            			r = libusb_claim_interface(devh, iface);
            			LOGD("Try reclaiming interface %d after detach kernel: %s", iface, libusb_error_name(r));
            		}



                    if (rc == LIBUSB_SUCCESS){
                        //count endopints
                        LOGD("Iface %d has %d alt_set: %s",iface,conf_desc->usb_interface[iface].num_altsetting, libusb_error_name(rc));

                        for(j = 0; j < conf_desc->usb_interface[iface].num_altsetting; j++){
                        rc = libusb_set_interface_alt_setting(devh, iface, j);
                        LOGD("Try to set alternative settings %d on IFACE %d: %s",j, iface, libusb_error_name(rc));
                        LOGD("Iface %d - alt_sett %d is %d/%d: has %d endpoints: %s",iface,j,conf_desc->usb_interface[iface].altsetting[j].bInterfaceClass,conf_desc->usb_interface[iface].altsetting[j].bInterfaceSubClass,conf_desc->usb_interface[iface].altsetting[j].bNumEndpoints, libusb_error_name(rc));

                        for(k=0;k<conf_desc->usb_interface[iface].altsetting[j].bNumEndpoints;k++){
                            endpoint = &conf_desc->usb_interface[iface].altsetting[j].endpoint[k];
                            LOGD("  Endpoint %d | Address : %04X | MAX_PACKET_SIZE = %04X",k,endpoint->bEndpointAddress,endpoint->wMaxPacketSize);
                        }
                        }
                    }
            	}
*/
            	// safe
            	LOGD("Claiming iface %d ...", 1);
            	r = libusb_claim_interface(devh, 1);
                if (r != LIBUSB_SUCCESS) {
                    LOGD("Failed to claim iface %d: %s. Detaching kernel interface...", iface, libusb_error_name(r));
                    rc = libusb_detach_kernel_driver(devh, 1);
                    LOGD("Detaching kernel iface: %s", libusb_error_name(rc));
                    r = libusb_claim_interface(devh, 1);
                    LOGD("Try reclaiming interface %d after detach kernel: %s", iface, libusb_error_name(r));
                }

                rc = libusb_set_interface_alt_setting(devh, 1, 3);
                LOGD("Try to set alternative settings %d on IFACE %d: %s",3, 1, libusb_error_name(rc));

            	/*
            rc = libusb_kernel_driver_active(devh, IFACE_NUM);
            if (rc == 1) {
                rc = libusb_detach_kernel_driver(devh, IFACE_NUM);
                if (rc < 0) {
                    LOGD("Could not detach kernel driver: %s\n",
                            libusb_error_name(rc));
                    libusb_close(devh);
                    libusb_exit(NULL);
                    return false;
                }
            }
        */

        	//rc = libusb_claim_interface(devh, IFACE_NUM);
        /*
        	if (rc < 0) {
        		LOGD("Error claiming interface: %s\n", libusb_error_name(rc));
                libusb_close(devh);
                libusb_exit(NULL);
                return false;
            }



        	rc = libusb_set_interface_alt_setting(devh, IFACE_NUM, 1);
        	if (rc < 0) {
        		LOGD("Error setting alt setting: %s\n", libusb_error_name(rc));
                libusb_close(devh);
                libusb_exit(NULL);
                return false;
        	}
        */
        /*
            // Get write callback handle
            jclass clazz = (*env)->FindClass(env, "com/eneaceolini/testusb/AudioPlayback");
            if (!clazz) {
                LOGD("Could not find com.example.enea.readsoundcard.AudioPlayback");
                libusb_close(devh);
                libusb_exit(NULL);
                return false;
            }

            au_id_jms_usbaudio_AudioPlayback = (*env)->NewGlobalRef(env, clazz);

            au_id_jms_usbaudio_AudioPlayback_write = (*env)->GetStaticMethodID(env,
                    au_id_jms_usbaudio_AudioPlayback, "write", "([S)V");
            if (!au_id_jms_usbaudio_AudioPlayback_write) {
                LOGD("Could not find com.example.enea.readsoundcard.AudioPlayback");
                (*env)->DeleteGlobalRef(env, au_id_jms_usbaudio_AudioPlayback);
                libusb_close(devh);
                libusb_exit(NULL);
                return false;
            }
            */


            rc = usb_start_transfers();
                if (rc != 0) {
                    LOGD("usb_start_transfers: %d\n" ,rc);
                }

            LOGD("Good to Go!");
            // Good to go
            /*
            do_exit = 0;
            LOGD("Starting capture");
        	if ((rc = benchmark_in(EP_ISO_IN)) < 0) {
                LOGD("Capture failed to start: %d", rc);
                return false;
            }

            */
    	//
    	while(true){
    	rc = libusb_handle_events(NULL);
        }


    	return 0;

}

JNIEXPORT jboolean JNICALL
Java_com_eneaceolini_testusb_UsbAudio_setup(JNIEnv* env UNUSED, jobject foo UNUSED)
{



    // Get write callback handle
    jclass clazz = (*env)->FindClass(env, "com/eneaceolini/testusb/AudioPlayback");
    if (!clazz) {
        LOGD("Could not find com.example.enea.readsoundcard.AudioPlayback");
        libusb_close(devh);
        libusb_exit(NULL);
        return false;
    }
    au_id_jms_usbaudio_AudioPlayback = (*env)->NewGlobalRef(env, clazz);

    au_id_jms_usbaudio_AudioPlayback_write = (*env)->GetStaticMethodID(env,
            au_id_jms_usbaudio_AudioPlayback, "write", "([S)V");
    if (!au_id_jms_usbaudio_AudioPlayback_write) {
        LOGD("Could not find com.example.enea.readsoundcard.AudioPlayback");
        (*env)->DeleteGlobalRef(env, au_id_jms_usbaudio_AudioPlayback);
        libusb_close(devh);
        libusb_exit(NULL);
        return false;
    }


    return true;

}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved UNUSED)
{
    LOGD("libusbaudio: loaded");
    java_vm = vm;

    return JNI_VERSION_1_6;
}





