package com.aigame.engine

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.content.SharedPreferences
import android.os.Bundle
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

class MainActivity : AppCompatActivity() {

    private lateinit var glSurfaceView: GLSurfaceView
    private lateinit var tvScore: TextView
    private lateinit var tvGameGoal: TextView
    private lateinit var tvWinBanner: TextView
    private lateinit var joystickView: JoystickView
    private lateinit var prefs: SharedPreferences

    private val messages = mutableListOf<ChatMessage>()
    private lateinit var adapter: ChatAdapter

    private var forwardInput = 0f
    private var strafeInput = 0f

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        prefs = getSharedPreferences("GMS_PREFS", Context.MODE_PRIVATE)

        glSurfaceView = findViewById(R.id.gl_surface_view)
        glSurfaceView.setEGLContextClientVersion(3)
        glSurfaceView.setRenderer(EngineRenderer())

        tvScore = findViewById(R.id.tv_score)
        tvGameGoal = findViewById(R.id.tv_game_goal)
        tvWinBanner = findViewById(R.id.tv_win_banner)
        joystickView = findViewById(R.id.joystick_view)

        // Джойстик движение на героя
        joystickView.onJoystickMove = { f, s ->
            forwardInput = f
            strafeInput = s
        }

        // 60 FPS геймплей лууп: движи героя и обновява точките
        lifecycleScope.launch {
            while (true) {
                if (forwardInput != 0f || strafeInput != 0f) {
                    NativeEngine.movePlayer(forwardInput, strafeInput)
                }
                val score = NativeEngine.getScore()
                tvScore.text = "💎 Точки: $score"

                if (NativeEngine.isWon()) {
                    tvWinBanner.visibility = View.VISIBLE
                } else {
                    tvWinBanner.visibility = View.GONE
                }
                delay(16)
            }
        }

        // Чат и конзола
        val chatRecycler: RecyclerView = findViewById(R.id.chat_recycler)
        val chatInput: EditText = findViewById(R.id.chat_input)
        val btnSend: Button = findViewById(R.id.btn_send)
        val btnAiCloud: Button = findViewById(R.id.btn_ai_cloud)

        adapter = ChatAdapter(messages)
        chatRecycler.layoutManager = LinearLayoutManager(this)
        chatRecycler.adapter = adapter

        addMessage("GMS Game Engine готов. Опиши каква игра искаш да създадеш!", false)

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

                addMessage("Създавам играта в реално време...", false)
                val loadingIndex = messages.size - 1

                lifecycleScope.launch {
                    val rawReply = AiCloudManager.generateResponse(provider, key, model, text)
                    val cleanReply = parseAndBuildGame(rawReply)

                    messages[loadingIndex] = ChatMessage(cleanReply, false)
                    adapter.notifyItemChanged(loadingIndex)
                    chatRecycler.scrollToPosition(loadingIndex)
                }
            }
        }
    }

    private fun addMessage(text: String, isUser: Boolean) {
        messages.add(ChatMessage(text, isUser))
        adapter.notifyItemInserted(messages.size - 1)
        findViewById<RecyclerView>(R.id.chat_recycler)?.scrollToPosition(messages.size - 1)
    }

    private fun parseAndBuildGame(reply: String): String {
        val regex = Regex("\\[(?:CMD:)?([A-Za-zА-Яа-я_]+)(?::([^\\]]+))?\\]")
        val matches = regex.findAll(reply)

        for (match in matches) {
            val cmd = match.groupValues[1].uppercase()
            val rawValue = match.groupValues[2]

            try {
                when (cmd) {
                    "CLEAR", "ИЗЧИСТИ" -> NativeEngine.clearWorld()
                    "SKY", "НЕБЕ" -> {
                        val rgb = rawValue.split(",").map { it.trim().toFloat() }
                        if (rgb.size == 3) NativeEngine.setSky(rgb[0], rgb[1], rgb[2])
                    }
                    "GOAL", "ЦЕЛ" -> {
                        tvGameGoal.text = rawValue
                    }
                    "SPAWN", "СПАУН" -> {
                        val numRegex = Regex("[-+]?\\d*\\.?\\d+")
                        val n = numRegex.findAll(rawValue).map { it.value.toFloat() }.toList()

                        if (n.size >= 11) {
                            var r = n[6]; var g = n[7]; var b = n[8]
                            if (r > 1.0f) r /= 255f; if (g > 1.0f) g /= 255f; if (b > 1.0f) b /= 255f
                            val beh = n[9].toInt()
                            val touch = n[10].toInt()
                            NativeEngine.spawnEntity(n[0], n[1], n[2], n[3], n[4], n[5], r, g, b, beh, touch)
                        }
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
