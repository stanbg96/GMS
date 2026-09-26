package com.aigame.engine

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.content.SharedPreferences
import android.os.Bundle
import android.view.MotionEvent
import android.view.ViewGroup
import android.widget.*
import android.opengl.GLSurfaceView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private val messages = mutableListOf<ChatMessage>()
    private lateinit var adapter: ChatAdapter
    private lateinit var glSurfaceView: GLSurfaceView
    private lateinit var prefs: SharedPreferences

    private var prevTouchX = 0f
    private var prevTouchY = 0f

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        prefs = getSharedPreferences("GMS_PREFS", Context.MODE_PRIVATE)

        glSurfaceView = findViewById(R.id.gl_surface_view)
        glSurfaceView.setEGLContextClientVersion(3)
        glSurfaceView.setRenderer(EngineRenderer())

        glSurfaceView.setOnTouchListener { _, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    prevTouchX = event.x
                    prevTouchY = event.y
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = (event.x - prevTouchX) * 0.005f
                    val dy = (event.y - prevTouchY) * 0.005f
                    NativeEngine.rotateCamera(dx, dy)
                    prevTouchX = event.x
                    prevTouchY = event.y
                }
            }
            true
        }

        findViewById<Button>(R.id.btn_zoom_in).setOnClickListener { NativeEngine.zoomCamera(-2.0f) }
        findViewById<Button>(R.id.btn_zoom_out).setOnClickListener { NativeEngine.zoomCamera(2.0f) }

        val chatRecycler: RecyclerView = findViewById(R.id.chat_recycler)
        val chatInput: EditText = findViewById(R.id.chat_input)
        val btnSend: Button = findViewById(R.id.btn_send)
        val btnAiCloud: Button = findViewById(R.id.btn_ai_cloud)

        adapter = ChatAdapter(messages)
        chatRecycler.layoutManager = LinearLayoutManager(this)
        chatRecycler.adapter = adapter

        val savedProvider = prefs.getString("ai_provider", "OpenRouter") ?: "OpenRouter"
        val savedModel = prefs.getString("ai_model", "") ?: ""
        if (savedModel.isNotEmpty()) {
            addMessage("Система: GMS HD Polygon Engine е готов. AI: $savedProvider ($savedModel)", false)
        } else {
            addMessage("Система: GMS Engine е готов. Натисни 'AI Cloud' за модел.", false)
        }

        btnAiCloud.setOnClickListener { showAiCloudDialog() }

        btnSend.setOnClickListener {
            val text = chatInput.text.toString().trim()
            if (text.isNotEmpty()) {
                val provider = prefs.getString("ai_provider", "") ?: ""
                val key = prefs.getString("ai_api_key", "") ?: ""
                val model = prefs.getString("ai_model", "") ?: ""

                if (key.isEmpty() || model.isEmpty() || model == "Не е избран") {
                    addMessage("Система: Моля, настройте AI Cloud първо!", false)
                    return@setOnClickListener
                }

                addMessage(text, true)
                chatInput.text.clear()

                addMessage("...", false)
                val loadingIndex = messages.size - 1

                lifecycleScope.launch {
                    val rawReply = AiCloudManager.generateResponse(provider, key, model, text)
                    val cleanReply = parseAndExecuteCommands(rawReply)

                    messages[loadingIndex] = ChatMessage(cleanReply, false)
                    adapter.notifyItemChanged(loadingIndex)
                    chatRecycler.scrollToPosition(loadingIndex)
                }
            }
        }
    }

    private fun parseAndExecuteCommands(reply: String): String {
        val regex = Regex("\\[(?:CMD:)?([A-Za-zА-Яа-я_]+)(?::([^\\]]+))?\\]")
        val matches = regex.findAll(reply)

        for (match in matches) {
            val cmd = match.groupValues[1].uppercase()
            val rawValue = match.groupValues[2]

            try {
                when (cmd) {
                    "CLEAR", "ИЗЧИСТИ" -> NativeEngine.clearMesh()
                    "CYL", "ЦИЛИНДЪР" -> {
                        val n = Regex("[-+]?\\d*\\.?\\d+").findAll(rawValue).map { it.value.toFloat() }.toList()
                        if (n.size >= 8) {
                            var r = n[5]; var g = n[6]; var b = n[7]
                            if (r > 1.0f) r /= 255f; if (g > 1.0f) g /= 255f; if (b > 1.0f) b /= 255f
                            NativeEngine.addCylinder(n[0], n[1], n[2], n[3], n[4], r, g, b)
                        }
                    }
                    "WDG", "СКОСЯВАНЕ", "ПОКРИВ" -> {
                        val n = Regex("[-+]?\\d*\\.?\\d+").findAll(rawValue).map { it.value.toFloat() }.toList()
                        if (n.size >= 9) {
                            var r = n[6]; var g = n[7]; var b = n[8]
                            if (r > 1.0f) r /= 255f; if (g > 1.0f) g /= 255f; if (b > 1.0f) b /= 255f
                            NativeEngine.addWedge(n[0], n[1], n[2], n[3], n[4], n[5], r, g, b)
                        }
                    }
                    "BOX", "ПАНЕЛ" -> {
                        val n = Regex("[-+]?\\d*\\.?\\d+").findAll(rawValue).map { it.value.toFloat() }.toList()
                        if (n.size >= 9) {
                            var r = n[6]; var g = n[7]; var b = n[8]
                            if (r > 1.0f) r /= 255f; if (g > 1.0f) g /= 255f; if (b > 1.0f) b /= 255f
                            NativeEngine.addBox(n[0], n[1], n[2], n[3], n[4], n[5], r, g, b)
                        }
                    }
                    "BG" -> {
                        val rgb = rawValue.split(",").map { it.trim().toFloat() }
                        if (rgb.size == 3) NativeEngine.setBackgroundColor(rgb[0], rgb[1], rgb[2])
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        return reply.replace(regex, "").trim()
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

            tvStatus.text = "Сваляне на модели..."
            tvStatus.setTextColor(0xFFFFFF00.toInt())

            lifecycleScope.launch {
                val models = AiCloudManager.fetchModels(provider, key)
                if (models.isNotEmpty()) {
                    currentModels.clear()
                    currentModels.addAll(models)
                    spinnerModels.adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, currentModels)
                    tvModelLabel.visibility = android.view.View.VISIBLE
                    spinnerModels.visibility = android.view.View.VISIBLE
                    tvStatus.text = "Успех! Намерени ${models.size} модела."
                    tvStatus.setTextColor(0xFF00FF00.toInt())
                } else {
                    tvStatus.text = "Грешка! Провери ключа или интернета."
                    tvStatus.setTextColor(0xFFFF0000.toInt())
                }
            }
        }

        btnTest.setOnClickListener {
            val provider = spinnerProvider.selectedItem.toString()
            val key = etApiKey.text.toString().trim()
            val model = if (spinnerModels.visibility == android.view.View.VISIBLE && spinnerModels.selectedItem != null) {
                spinnerModels.selectedItem.toString()
            } else ""

            if (key.isEmpty() || model.isEmpty()) {
                tvStatus.text = "Първо свали и избери модел!"
                tvStatus.setTextColor(0xFFFF0000.toInt())
                return@setOnClickListener
            }

            tvStatus.text = "Тест..."
            tvStatus.setTextColor(0xFFFFFF00.toInt())

            lifecycleScope.launch {
                val testRes = AiCloudManager.generateResponse(provider, key, model, "Тест. Кажи 'Работи!'.")
                tvStatus.text = testRes
                tvStatus.setTextColor(0xFF00FF00.toInt())
            }
        }

        btnSave.setOnClickListener {
            val provider = spinnerProvider.selectedItem.toString()
            val key = etApiKey.text.toString().trim()
            val model = if (spinnerModels.visibility == android.view.View.VISIBLE && spinnerModels.selectedItem != null) {
                spinnerModels.selectedItem.toString()
            } else {
                "Не е избран"
            }

            prefs.edit()
                .putString("ai_provider", provider)
                .putString("ai_api_key", key)
                .putString("ai_model", model)
                .apply()

            addMessage("Система: AI Cloud запазен -> $provider | $model", false)
            dialog.dismiss()
        }

        dialog.show()
    }

    private fun addMessage(text: String, isUser: Boolean) {
        messages.add(ChatMessage(text, isUser))
        adapter.notifyItemInserted(messages.size - 1)
        findViewById<RecyclerView>(R.id.chat_recycler).scrollToPosition(messages.size - 1)
    }

    override fun onResume() { super.onResume(); glSurfaceView.onResume() }
    override fun onPause() { super.onPause(); glSurfaceView.onPause() }
}
