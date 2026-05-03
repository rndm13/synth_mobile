package com.raylib.synth;
public class NativeLoader extends android.app.NativeActivity {
    static {
        System.loadLibrary("main");
    }
}
