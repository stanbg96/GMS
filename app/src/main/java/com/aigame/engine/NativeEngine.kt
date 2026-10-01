package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // Зареждане на цял 3D свят през JSON (генериран от AI)
    external fun loadSceneJson(jsonStr: String): Boolean

    // Управление и статус
    external fun movePlayer(forwardInput: Float, strafeInput: Float)
    external fun clearWorld()
    external fun getScore(): Int
    external fun isWon(): Boolean
}
