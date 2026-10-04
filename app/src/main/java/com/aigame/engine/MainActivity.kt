package com.aigame.engine

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.content.SharedPreferences
import android.os.Bundle
import android.view.Choreographer
import android.view.SurfaceView
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.filament.EntityManager
import com.google.android.filament.LightManager
import com.google.android.filament.Skybox
import com.google.android.filament.utils.ModelViewer
import com.google.android.filament.utils.Utils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.nio.ByteBuffer

class MainActivity : AppCompatActivity() {

    private lateinit var surfaceView: SurfaceView
    private lateinit var modelViewer: ModelViewer
    private lateinit var prefs: SharedPreferences

    private val messages = mutableListOf<ChatMessage>()
    private lateinit var adapter: ChatAdapter

    private val choreographer = Choreographer.getInstance()
    private val frameCallback = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            choreographer.postFrameCallback(this)
            modelViewer.render(frameTimeNanos)
        }
    }

    companion object {
        init {
            Utils.init()
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        prefs = getSharedPreferences("GMS_PREFS", Context.MODE_PRIVATE)

        surfaceView = findViewById(R.id.filament_surface_view)
        modelViewer = ModelViewer(surfaceView)

        setupPbrLighting()

        surfaceView.setOnTouchListener { _, event ->
            modelViewer.onTouchEvent(event)
            true
        }

        // Зареждане на началния PBR модел
        loadGlbModel("models/model.glb")

        val chatRecycler: RecyclerView = findViewById(R.id.chat_recycler)
        val chatInput: EditText = findViewById(R.id.chat_input)
        val btnSend: Button = findViewById(R.id.btn_send)
        val btnAiCloud: Button = findViewById(R.id.btn_ai_cloud)

        adapter = ChatAdapter(messages)
        chatRecycler.layoutManager = LinearLayoutManager(this)
        chatRecycler.adapter = adapter

        addMessage("Filament PBR активен! Напиши 'кола' или 'шлем', за да заредиш истински 3D модел.", false)

        btnAiCloud.setOnClickListener { showAiCloudDialog() }

        btnSend.setOnClickListener {
            val text = chatInput.text.toString().trim()
            if (text.isEmpty()) return@setOnClickListener

            addMessage(text, true)
            chatInput.text.clear()

            val lower = text.lowercase()
            if (lower.contains("кола") || lower.contains("автомобил") || lower.contains("truck") || lower.contains("car")) {
                loadGlbModel("models/car.glb")
                addMessage("Зареждам истински PBR автомобил с физично осветление...", false)
                return@setOnClickListener
            } else if (lower.contains("шлем") || lower.contains("helmet") || lower.contains("робот")) {
                loadGlbModel("models/model.glb")
                addMessage("Зареждам фотореалистичния шлем...", false)
                return@setOnClickListener
            }

            val provider = prefs.getString("ai_provider", "OpenRouter") ?: "OpenRouter"
            val key = prefs.getString("ai_api_key", "") ?: ""
            val model = prefs.getString("ai_model", "") ?: ""

            if (key.isEmpty() || model.isEmpty() || model == "Не е избран") {
                addMessage("Система: Моля, въведете API ключ от бутона ☁️ AI!", false)
                return@setOnClickListener
            }

            addMessage("...", false)
            val loadingIndex = messages.size - 1

            lifecycleScope.launch {
                val reply = AiCloudManager.generateResponse(provider, key, model, text)
                messages[loadingIndex] = ChatMessage(reply, false)
                adapter.notifyItemChanged(loadingIndex)
                chatRecycler.scrollToPosition(loadingIndex)
            }
        }
    }

    private fun setupPbrLighting() {
        modelViewer.scene.skybox = Skybox.Builder()
            .color(0.12f, 0.14f, 0.18f, 1.0f)
            .build(modelViewer.engine)

        val sunlight = EntityManager.get().create()
        LightManager.Builder(LightManager.Type.SUN)
            .color(1.0f, 0.98f, 0.95f)
            .intensity(120_000.0f)
            .direction(0.4f, -1.0f, -0.6f)
            .castShadows(true)
            .build(modelViewer.engine, sunlight)

        modelViewer.scene.addEntity(sunlight)
    }

    private fun loadGlbModel(assetPath: String) {
        lifecycleScope.launch {
            val buffer = withContext(Dispatchers.IO) {
                try {
                    assets.open(assetPath).use { input ->
                        val bytes = input.readBytes()
                        ByteBuffer.wrap(bytes)
                    }
                } catch (e: Exception) {
                    null
                }
            }

            buffer?.let {
                modelViewer.loadModelGlb(it)
                modelViewer.transformToUnitCube()
            }
        }
    }

    private fun addMessage(text: String, isUser: Boolean) {
        messages.add(ChatMessage(text, isUser))
        adapter.notifyItemInserted(messages.size - 1)
        findViewById<RecyclerView>(R.id.chat_recycler)?.scrollToPosition(messages.size - 1)
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
                tvStatus.text = "Въведи ключ!"
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
                tvStatus.text = "Избери модел!"
                tvStatus.setTextColor(0xFFFF0000.toInt())
                return@setOnClickListener
            }
            tvStatus.text = "Тест..."
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

    override fun onResume() {
        super.onResume()
        choreographer.postFrameCallback(frameCallback)
    }

    override fun onPause() {
        super.onPause()
        choreographer.removeFrameCallback(frameCallback)
    }

    override fun onDestroy() {
        super.onDestroy()
        choreographer.removeFrameCallback(frameCallback)
    }
}
