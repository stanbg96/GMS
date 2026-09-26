package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    external fun setBackgroundColor(r: Float, g: Float, b: Float)
    external fun rotateCamera(dx: Float, dy: Float)
    external fun zoomCamera(zoom: Float)
    external fun spawnCube(x: Float, y: Float, z: Float, scale: Float, r: Float, g: Float, b: Float)
    external fun clearWorld()
}
