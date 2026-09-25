package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // Команди за управление на 3D сцената
    external fun setBackgroundColor(r: Float, g: Float, b: Float)
    external fun setCubeColor(r: Float, g: Float, b: Float)
    external fun setCubeVisible(visible: Boolean)
    external fun setRotationSpeed(speed: Float)
}
