package com.aigame.engine

object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // Камера и движение
    external fun moveCamera(forwardInput: Float, strafeInput: Float)
    external fun rotateLook(dx: Float, dy: Float)
    external fun zoomCamera(delta: Float)

    // Raycast селекция и преместване
    external fun pickObject(tapX: Float, tapY: Float): Int
    external fun moveSelectedXZ(deltaRight: Float, deltaForward: Float)

    // Операции с обекта
    external fun deleteSelected()
    external fun duplicateSelected()
    external fun scaleSelected(factor: Float)
    external fun rotateSelected(deg: Float)
    external fun moveSelectedY(deltaY: Float)
}
