package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    external fun setBackgroundColor(r: Float, g: Float, b: Float)
    external fun setCubeColor(r: Float, g: Float, b: Float)
    external fun setCubeVisible(visible: Boolean)
    external fun setRotationSpeed(speed: Float)
    external fun setCubeScale(scale: Float)
}
