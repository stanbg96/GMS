package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // 1=Юмрук, 2=Ритник, 3=Скок
    external fun triggerAction(actionType: Int)
    external fun movePlayer(forwardInput: Float, strafeInput: Float)
    external fun restartGame()
    external fun getPlayerHp(): Int
    external fun getScore(): Int
    external fun isWon(): Boolean
}
