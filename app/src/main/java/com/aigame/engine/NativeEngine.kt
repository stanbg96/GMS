package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    external fun loadSceneJson(jsonStr: String): Boolean
    external fun movePlayer(forwardInput: Float, strafeInput: Float)
    external fun triggerAction(actionType: Int)
    external fun getPlayerHp(): Int
    external fun getScore(): Int
    external fun isWon(): Boolean
    external fun clearWorld()
}
