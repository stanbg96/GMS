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

    // Полигонални 3D примитиви
    external fun clearMesh()
    external fun addBox(x: Float, y: Float, z: Float, sx: Float, sy: Float, sz: Float, r: Float, g: Float, b: Float)
    external fun addCylinder(x: Float, y: Float, z: Float, radius: Float, width: Float, r: Float, g: Float, b: Float)
    external fun addWedge(x: Float, y: Float, z: Float, sx: Float, sy: Float, sz: Float, r: Float, g: Float, b: Float)
}
