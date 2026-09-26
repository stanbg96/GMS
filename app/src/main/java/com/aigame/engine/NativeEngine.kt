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
    external fun clearMesh()

    // Процедурен синтез на гладки 3D модели
    external fun generateModel(type: String, r: Float, g: Float, b: Float)
}
