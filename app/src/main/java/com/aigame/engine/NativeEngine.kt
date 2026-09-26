package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // Свободна камера и Джойстик
    external fun moveCamera(forwardInput: Float, strafeInput: Float)
    external fun rotateLook(dx: Float, dy: Float)

    // Raycast селекция (клик върху модел)
    external fun pickObject(tapX: Float, tapY: Float): Int

    // Редакторски опции за избрания обект
    external fun deleteSelected()
    external fun duplicateSelected()
    external fun scaleSelected(factor: Float)
    external fun rotateSelected(deg: Float)
    external fun moveSelectedY(deltaY: Float)
    external fun spawnNewObject()
}
