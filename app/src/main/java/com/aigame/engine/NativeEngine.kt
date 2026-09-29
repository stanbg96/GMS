package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    external fun movePlayer(forwardInput: Float, strafeInput: Float)
    external fun getScore(): Int
    external fun isWon(): Boolean

    external fun clearWorld()
    external fun setSky(r: Float, g: Float, b: Float)
    external fun spawnEntity(x: Float, y: Float, z: Float,
                            sx: Float, sy: Float, sz: Float,
                            r: Float, g: Float, b: Float,
                            behavior: Int, touchRule: Int)
}
