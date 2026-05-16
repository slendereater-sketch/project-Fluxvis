package com.visualizer.engine

import android.view.Surface

object NativeInterface {
    init {
        System.loadLibrary("ndk_visualizer")
    }

    external fun init(surface: Surface?)
    external fun onResize(width: Int, height: Int)
    external fun renderFrame()
    external fun updateControls(zoom: Float, warp: Float, dampening: Float)
    external fun pushAudioData(data: FloatArray)
}
