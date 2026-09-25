package com.aigame.engine

// Този обект отговаря само за комуникацията с C++
object NativeEngine {
    init {
        System.loadLibrary("engine")
    }
    
    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()
}
