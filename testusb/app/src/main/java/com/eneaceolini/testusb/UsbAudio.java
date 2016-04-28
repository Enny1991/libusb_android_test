package com.eneaceolini.testusb;

/**
 * Created by enea on 21.04.16.
 */
public class UsbAudio {
    static {
        System.loadLibrary("usbaudio");
    }

    public native boolean setup();
//    public native void close();
//    public native void loop();
//    public native boolean stop();
//    public native int measure();

}