package com.aigame.engine

import android.opengl.GLSurfaceView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class EngineRenderer : GLSurfaceView.Renderer {
    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        NativeEngine.onSurfaceCreated()
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        NativeEngine.onSurfaceChanged(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        NativeEngine.onDrawFrame()
    }
}
