package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // Управление и универсални действия
    external fun movePlayer(forwardInput: Float, strafeInput: Float)
    external fun triggerAction(actionType: Int) // 1=Удар, 2=Стрелба, 3=Скок
    external fun getPlayerHp(): Int
    external fun getScore(): Int
    external fun isWon(): Boolean

    // Универсално изграждане на светове от AI
    external fun clearWorld()
    external fun setSky(r: Float, g: Float, b: Float)
    external fun spawnEntity(x: Float, y: Float, z: Float,
                            sx: Float, sy: Float, sz: Float,
                            r: Float, g: Float, b: Float,
                            tag: Int, hp: Int, aiType: Int)
}
