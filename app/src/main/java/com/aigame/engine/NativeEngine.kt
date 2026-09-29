package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // Моментален рестарт
    external fun restartGame()

    // Управление и екшън
    external fun movePlayer(forwardInput: Float, strafeInput: Float)
    external fun triggerAction(actionType: Int)
    external fun getPlayerHp(): Int
    external fun getScore(): Int
    external fun isWon(): Boolean

    // Изграждане и AI
    external fun clearWorld()
    external fun setSky(r: Float, g: Float, b: Float)
    external fun spawnEntity(x: Float, y: Float, z: Float,
                            sx: Float, sy: Float, sz: Float,
                            r: Float, g: Float, b: Float,
                            tag: Int, hp: Int, aiType: Int)
}
