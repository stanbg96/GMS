package com.aigame.engine

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.content.SharedPreferences
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.*
import android.opengl.GLSurfaceView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.abs

class MainActivity : AppCompatActivity() {

    private lateinit var glSurfaceView: GLSurfaceView
    private lateinit var toolbarSelected: HorizontalScrollView
    private lateinit var tvEditorInfo: TextView
    private lateinit var joystickView: JoystickView
    private lateinit var prefs: SharedPreferences

    private val messages = mutableListOf<ChatMessage>()
    private lateinit var adapter: ChatAdapter

    private var downX = 0f
    private var downY = 0f
    private var prevX = 0f
    private var prevY = 0f

    private var forwardInput = 0f
    private var strafeInput = 0f

    private var currentSelectedIdx = -1
    private var isDraggingObject = false

    private val mainHandler = Handler(Looper.getMainLooper())
    private val holdRunnable = Runnable {
        if (currentSelectedIdx >= 0) {
            isDraggingObject = true
            tvEditorInfo.text = "🎯 Влачи с пръст, за да местиш обекта"
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        prefs = getSharedPreferences("GMS_PREFS", Context.MODE_PRIVATE)

        glSurfaceView = findViewById(R.id.gl_surface_view)
        glSurfaceView.setEGLContextClientVersion(3)
        glSurfaceView.setRenderer(EngineRenderer())

        toolbarSelected = findViewById(R.id.toolbar_selected)
        tvEditorInfo = findViewById(R.id.tv_editor_info)
        joystickView = findViewById(R.id.joystick_view)

        // Джойстик движение
        joystickView.onJoystickMove = { f, s ->
            forwardInput = f
            strafeInput = s
        }

        lifecycleScope.launch {
            while (true) {
                if (forwardInput != 0f || strafeInput != 0f) {
                    NativeEngine.moveCamera(forwardInput, strafeInput)
                }
                delay(16)
            }
        }

        // Бутони за Zoom горе вдясно
        findViewById<Button>(R.id.btn_zoom_in).setOnClickListener { NativeEngine.zoomCamera(1.8f) }
        findViewById<Button>(R.id.btn_zoom_out).setOnClickListener { NativeEngine.zoomCamera(-1.8f) }

        // Тъч логика
        glSurfaceView.setOnTouchListener { _, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    downX = event.x
                    downY = event.y
                    prevX = event.x
                    prevY = event.y
                    isDraggingObject = false

                    val hit = NativeEngine.pickObject(event.x, event.y)
                    if (hit >= 0) {
                        currentSelectedIdx = hit
                        toolbarSelected.visibility = View.VISIBLE
                        tvEditorInfo.text = "Избран обект #$hit (Задръж за влачене)"
                        mainHandler.postDelayed(holdRunnable, 250)
                    } else {
                        mainHandler.removeCallbacks(holdRunnable)
                    }
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = event.x - prevX
                    val dy = event.y - prevY

                    if (isDraggingObject) {
                        // Влачене на обекта по земята
                        NativeEngine.moveSelectedXZ(dx * 0.035f, -dy * 0.035f)
                    } else {
                        val travel = abs(event.x - downX) + abs(event.y - downY)
                        if (travel > 18f) {
                            mainHandler.removeCallbacks(holdRunnable)
                        }
                        // Оглеждане с камерата
                        NativeEngine.rotateLook(dx * 0.0045f, dy * 0.005f)
                    }
                    prevX = event.x
                    prevY = event.y
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    mainHandler.removeCallbacks(holdRunnable)
                    val travel = abs(event.x - downX) + abs(event.y - downY)

                    if (!isDraggingObject && travel < 18f && currentSelectedIdx < 0) {
                        NativeEngine.pickObject(-1000f, -1000f)
                        toolbarSelected.visibility = View.GONE
                        tvEditorInfo.text = "Докосни обект за избор | Задръж за влачене"
                    }

                    if (isDraggingObject) {
                        isDraggingObject = false
                        tvEditorInfo.text = "Обект #$currentSelectedIdx е преместен"
                    }
                }
            }
            true
        }

        // Действия с избрания обект
        findViewById<Button>(R.id.btn_delete).setOnClickListener {
            NativeEngine.deleteSelected()
            toolbarSelected.visibility = View.GONE
            currentSelectedIdx = -1
            tvEditorInfo.text = "Обектът е изтрит"
        }
        findViewById<Button>(R.id.btn_duplicate).setOnClickListener {
            NativeEngine.duplicateSelected()
            tvEditorInfo.text = "Обектът е дублиран"
        }
        findViewById<Button>(R.id.btn_scale_up).setOnClickListener { NativeEngine.scaleSelected(1.25f) }
        findViewById<Button>(R.id.btn_scale_down).setOnClickListener { NativeEngine.scaleSelected(0.80f) }
        findViewById<Button>(R.id.btn_rotate).setOnClickListener { NativeEngine.rotateSelected(30f) }
        findViewById<Button>(R.id.btn_up).setOnClickListener { NativeEngine.moveSelectedY(0.4f) }
        findViewById<Button>(R.id.btn_down).setOnClickListener { NativeEngine.moveSelectedY(-0.4f) }
        findViewById<Button>(R.id.btn_close_selection).setOnClickListener {
            NativeEngine.pickObject(-1000f, -1000f)
            toolbarSelected.visibility = View.GONE
            currentSelectedIdx = -1
            tvEditorInfo.text = "Докосни обект за избор | Задръж за влачене"
        }

        // ЧАТ СИСТЕМА (30%)
        val chatRecycler: RecyclerView = findViewById(R.id.chat_recycler)
        val chatInput: EditText = findViewById(R.id.chat_input)
        val btnSend: Button = findViewById(R.id.btn_send)
        val btnAiCloud: Button = findViewById(R.id.btn_ai_cloud)

        adapter = ChatAdapter(messages)
        chatRecycler.layoutManager = LinearLayoutManager(this)
        chatRecycler.adapter = adapter

        addMessage("3D Editor активен: Джойстик долу вляво, Zoom (+/-) горе вдясно.", false)

        btnAiCloud.setOnClickListener { showAiCloudDialog() }

        btnSend.setOnClickListener {
            val text = chatInput.text.toString().trim()
            if (text.isNotEmpty()) {
                val provider = prefs.getString("ai_provider", "OpenRouter") ?: "OpenRouter"
                val key = prefs.getString("ai_api_key", "") ?: ""
                val model = prefs.getString("ai_model", "") ?: ""

                if (key.isEmpty() || model.isEmpty() || model == "Не е избран") {
                    addMessage("Система: Моля, настройте AI Cloud от бутона ☁️ AI!", false)
                    return@setOnClickListener
                }

                addMessage(text, true)
                chatInput.text.clear()

                addMessage("...", false)
                val loadingIndex = messages.size - 1

                lifecycleScope.launch {
                    val rawReply = AiCloudManager.generateResponse(provider, key, model, text)
                    messages[loadingIndex] = ChatMessage(rawReply, false)
                    adapter.notifyItemChanged(loadingIndex)
                    chatRecycler.scrollToPosition(loadingIndex)
                }
            }
        }
    }

    private fun addMessage(text: String, isUser: Boolean) {
        messages.add(ChatMessage(text, isUser))
        adapter.notifyItemInserted(messages.size - 1)
        findViewById<RecyclerView>(R.id.chat_recycler).scrollToPosition(messages.size - 1)
    }

    private fun showAiCloudDialog() {
        val dialog = Dialog(this)
        dialog.setContentView(R.layout.dialog_ai_settings)
        dialog.window?.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)

        val spinnerProvider: Spinner = dialog.findViewById(R.id.spinner_provider)
        val etApiKey: EditText = dialog.findViewById(R.id.et_api_key)
        val btnFetch: Button = dialog.findViewById(R.id.btn_fetch_models)
        val tvModelLabel: TextView = dialog.findViewById(R.id.tv_model_label)
        val spinnerModels: Spinner = dialog.findViewById(R.id.spinner_models)
        val tvStatus: TextView = dialog.findViewById(R.id.tv_status)
        val btnTest: Button = dialog.findViewById(R.id.btn_test)
        val btnSave: Button = dialog.findViewById(R.id.btn_save)

        val providers = arrayOf("OpenRouter", "Google Gemini", "OpenAI", "Anthropic")
        spinnerProvider.adapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, providers)

        val savedProvider = prefs.getString("ai_provider", "OpenRouter")
        spinnerProvider.setSelection(providers.indexOf(savedProvider))
        etApiKey.setText(prefs.getString("ai_api_key", ""))

        var currentModels = mutableListOf<String>()

        btnFetch.setOnClickListener {
            val provider = spinnerProvider.selectedItem.toString()
            val key = etApiKey.text.toString().trim()
            if (key.isEmpty()) {
                tvStatus.text = "Моля, въведи API ключ!"
                tvStatus.setTextColor(0xFFFF0000.toInt())
                return@setOnClickListener
            }
            tvStatus.text = "Сваляне..."
            tvStatus.setTextColor(0xFFFFFF00.toInt())
            lifecycleScope.launch {
                val models = AiCloudManager.fetchModels(provider, key)
                if (models.isNotEmpty()) {
                    currentModels.clear()
                    currentModels.addAll(models)
                    spinnerModels.adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, currentModels)
                    tvModelLabel.visibility = View.VISIBLE
                    spinnerModels.visibility = View.VISIBLE
                    tvStatus.text = "Намерени ${models.size} модела."
                    tvStatus.setTextColor(0xFF00FF00.toInt())
                } else {
                    tvStatus.text = "Грешка при сваляне."
                    tvStatus.setTextColor(0xFFFF0000.toInt())
                }
            }
        }

        btnTest.setOnClickListener {
            val provider = spinnerProvider.selectedItem.toString()
            val key = etApiKey.text.toString().trim()
            val model = if (spinnerModels.visibility == View.VISIBLE && spinnerModels.selectedItem != null) spinnerModels.selectedItem.toString() else ""
            if (key.isEmpty() || model.isEmpty()) {
                tvStatus.text = "Избери модел първо!"
                tvStatus.setTextColor(0xFFFF0000.toInt())
                return@setOnClickListener
            }
            tvStatus.text = "Тестване..."
            tvStatus.setTextColor(0xFFFFFF00.toInt())
            lifecycleScope.launch {
                val res = AiCloudManager.generateResponse(provider, key, model, "Тест. Кажи 'Работи!'.")
                tvStatus.text = res
                tvStatus.setTextColor(0xFF00FF00.toInt())
            }
        }

        btnSave.setOnClickListener {
            val provider = spinnerProvider.selectedItem.toString()
            val key = etApiKey.text.toString().trim()
            val model = if (spinnerModels.visibility == View.VISIBLE && spinnerModels.selectedItem != null) spinnerModels.selectedItem.toString() else "Не е избран"
            prefs.edit().putString("ai_provider", provider).putString("ai_api_key", key).putString("ai_model", model).apply()
            addMessage("AI запазен: $provider ($model)", false)
            dialog.dismiss()
        }

        dialog.show()
    }

    override fun onResume() { super.onResume(); glSurfaceView.onResume() }
    override fun onPause() { super.onPause(); glSurfaceView.onPause() }
}
