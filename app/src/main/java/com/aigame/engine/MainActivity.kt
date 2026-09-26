package com.aigame.engine

import android.annotation.SuppressLint
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.HorizontalScrollView
import android.widget.TextView
import android.opengl.GLSurfaceView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.abs

class MainActivity : AppCompatActivity() {

    private lateinit var glSurfaceView: GLSurfaceView
    private lateinit var toolbarSelected: HorizontalScrollView
    private lateinit var tvEditorInfo: TextView
    private lateinit var joystickView: JoystickView

    private var downX = 0f
    private var downY = 0f
    private var prevX = 0f
    private var prevY = 0f

    private var forwardInput = 0f
    private var strafeInput = 0f

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        glSurfaceView = findViewById(R.id.gl_surface_view)
        glSurfaceView.setEGLContextClientVersion(3)
        glSurfaceView.setRenderer(EngineRenderer())

        toolbarSelected = findViewById(R.id.toolbar_selected)
        tvEditorInfo = findViewById(R.id.tv_editor_info)
        joystickView = findViewById(R.id.joystick_view)

        // Джойстик слушател за движение
        joystickView.onJoystickMove = { f, s ->
            forwardInput = f
            strafeInput = s
        }

        // Непрекъснат цикъл за плавно летене и движение (60 FPS)
        lifecycleScope.launch {
            while (true) {
                if (forwardInput != 0f || strafeInput != 0f) {
                    NativeEngine.moveCamera(forwardInput, strafeInput)
                }
                delay(16)
            }
        }

        // Тъч управление: Клик = Избор на обект | Плъзгане = Оглеждане
        glSurfaceView.setOnTouchListener { _, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    downX = event.x
                    downY = event.y
                    prevX = event.x
                    prevY = event.y
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = (event.x - prevX) * 0.005f
                    val dy = (event.y - prevY) * 0.005f
                    NativeEngine.rotateLook(dx, dy)
                    prevX = event.x
                    prevY = event.y
                }
                MotionEvent.ACTION_UP -> {
                    // Ако докосването е било кратко чукване на едно място -> Селекция с Raycast
                    val travel = abs(event.x - downX) + abs(event.y - downY)
                    if (travel < 20f) {
                        val hitIndex = NativeEngine.pickObject(event.x, event.y)
                        if (hitIndex >= 0) {
                            toolbarSelected.visibility = View.VISIBLE
                            tvEditorInfo.text = "Избран обект #$hitIndex"
                        } else {
                            toolbarSelected.visibility = View.GONE
                            tvEditorInfo.text = "Докосни обект за избор"
                        }
                    }
                }
            }
            true
        }

        // Бутони за управление на избрания 3D обект
        findViewById<Button>(R.id.btn_delete).setOnClickListener {
            NativeEngine.deleteSelected()
            toolbarSelected.visibility = View.GONE
            tvEditorInfo.text = "Обектът е изтрит"
        }

        findViewById<Button>(R.id.btn_duplicate).setOnClickListener {
            NativeEngine.duplicateSelected()
            tvEditorInfo.text = "Обектът е дублиран"
        }

        findViewById<Button>(R.id.btn_scale_up).setOnClickListener {
            NativeEngine.scaleSelected(1.25f)
        }

        findViewById<Button>(R.id.btn_scale_down).setOnClickListener {
            NativeEngine.scaleSelected(0.80f)
        }

        findViewById<Button>(R.id.btn_rotate).setOnClickListener {
            NativeEngine.rotateSelected(30f)
        }

        findViewById<Button>(R.id.btn_up).setOnClickListener {
            NativeEngine.moveSelectedY(0.4f)
        }

        findViewById<Button>(R.id.btn_down).setOnClickListener {
            NativeEngine.moveSelectedY(-0.4f)
        }

        findViewById<Button>(R.id.btn_close_selection).setOnClickListener {
            NativeEngine.pickObject(-1000f, -1000f) // Отмяна на селекцията
            toolbarSelected.visibility = View.GONE
            tvEditorInfo.text = "Докосни обект за избор"
        }

        findViewById<Button>(R.id.btn_spawn_new).setOnClickListener {
            NativeEngine.spawnNewObject()
            toolbarSelected.visibility = View.VISIBLE
            tvEditorInfo.text = "Добавен нов обект пред камерата"
        }
    }

    override fun onResume() { super.onResume(); glSurfaceView.onResume() }
    override fun onPause() { super.onPause(); glSurfaceView.onPause() }
}
