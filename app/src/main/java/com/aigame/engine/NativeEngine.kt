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

    // Зареждане на истински полигонални OBJ модели
    external fun loadObjString(objData: String, r: Float, g: Float, b: Float)
    external fun clearMesh()
}
