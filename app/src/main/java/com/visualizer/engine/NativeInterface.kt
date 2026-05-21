package com.visualizer.engine

import android.view.Surface

object NativeInterface {
    init {
        System.loadLibrary("vulkan_visualizer")
    }

    external fun nSetSurface(surface: Surface?)
    external fun nOnResize(width: Int, height: Int)
    external fun nRender()
    external fun nPushAudioData(data: FloatArray)
}
