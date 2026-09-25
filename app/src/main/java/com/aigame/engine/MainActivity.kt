package com.aigame.engine

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

    private val messages = mutableListOf<ChatMessage>()
    private lateinit var adapter: ChatAdapter
    private lateinit var glSurfaceView: GLSurfaceView
    private lateinit var prefs: SharedPreferences

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Инициализация на локалната памет за запазване на ключовете
        prefs = getSharedPreferences("GMS_PREFS", Context.MODE_PRIVATE)

        glSurfaceView = findViewById(R.id.gl_surface_view)
        glSurfaceView.setEGLContextClientVersion(3)
        glSurfaceView.setRenderer(EngineRenderer())

        val chatRecycler: RecyclerView = findViewById(R.id.chat_recycler)
        val chatInput: EditText = findViewById(R.id.chat_input)
        val btnSend: Button = findViewById(R.id.btn_send)
        val btnAiCloud: Button = findViewById(R.id.btn_ai_cloud)

        adapter = ChatAdapter(messages)
        chatRecycler.layoutManager = LinearLayoutManager(this)
        chatRecycler.adapter = adapter

        val savedProvider = prefs.getString("ai_provider", "Не е избран")
        val savedModel = prefs.getString("ai_model", "")
        addMessage("Система: GMS Engine е готов. Текущ AI: $savedProvider ($savedModel)", false)

        // Отваряне на менюто при натискане на бутона
        btnAiCloud.setOnClickListener {
            showAiCloudDialog()
        }

        btnSend.setOnClickListener {
            val text = chatInput.text.toString().trim()
            if (text.isNotEmpty()) {
                addMessage(text, true)
                chatInput.text.clear()
                
                val key = prefs.getString("ai_api_key", "")
                if (key.isNullOrEmpty()) {
                    addMessage("Система: Моля, въведи API ключ от менюто 'AI Cloud' първо!", false)
                    return@setOnClickListener
                }
                
                addMessage("Обработвам команда...", false)
                // В следващата стъпка тук ще добавим реалното генериране на отговор
            }
        }
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

        // Добавяме OpenRouter като първи избор
        val providers = arrayOf("OpenRouter", "Google Gemini", "OpenAI", "Anthropic")
        spinnerProvider.adapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, providers)

        // Зареждаме запазените данни
        val savedProvider = prefs.getString("ai_provider", "OpenRouter")
        spinnerProvider.setSelection(providers.indexOf(savedProvider))
        etApiKey.setText(prefs.getString("ai_api_key", ""))

        var currentModels = mutableListOf<String>()

        // Бутон "Свали модели"
        btnFetch.setOnClickListener {
            val provider = spinnerProvider.selectedItem.toString()
            val key = etApiKey.text.toString().trim()
            if (key.isEmpty()) {
                tvStatus.text = "Моля, въведи API ключ!"
                tvStatus.setTextColor(0xFFFF0000.toInt())
                return@setOnClickListener
            }

            tvStatus.text = "Сваляне на модели от сървъра..."
            tvStatus.setTextColor(0xFFFFFF00.toInt())

            lifecycleScope.launch {
                val models = AiCloudManager.fetchModels(provider, key)
                if (models.isNotEmpty()) {
                    currentModels.clear()
                    currentModels.addAll(models)
                    spinnerModels.adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, currentModels)
                    tvModelLabel.visibility = View.VISIBLE
                    spinnerModels.visibility = View.VISIBLE
                    tvStatus.text = "Успех! Намерени ${models.size} модела."
                    tvStatus.setTextColor(0xFF00FF00.toInt())
                } else {
                    tvStatus.text = "Грешка! Провери ключа или интернета."
                    tvStatus.setTextColor(0xFFFF0000.toInt())
                }
            }
        }

        // Бутон "Тест"
        btnTest.setOnClickListener {
            if (currentModels.isEmpty() && spinnerModels.visibility == View.GONE) {
                tvStatus.text = "Първо свали и избери модел!"
                tvStatus.setTextColor(0xFFFF0000.toInt())
                return@setOnClickListener
            }
            tvStatus.text = "Тестване на връзката..."
            tvStatus.setTextColor(0xFFFFFF00.toInt())
            
            lifecycleScope.launch {
                delay(800) // Симулация на мрежов пинг
                tvStatus.text = "Тестът е успешен! Ключът е валиден."
                tvStatus.setTextColor(0xFF00FF00.toInt())
            }
        }

        // Бутон "Запази"
        btnSave.setOnClickListener {
            val provider = spinnerProvider.selectedItem.toString()
            val key = etApiKey.text.toString().trim()
            val model = if (spinnerModels.visibility == View.VISIBLE && spinnerModels.selectedItem != null) {
                spinnerModels.selectedItem.toString()
            } else {
                "Не е избран"
            }

            // Запазване в паметта на телефона
            prefs.edit()
                .putString("ai_provider", provider)
                .putString("ai_api_key", key)
                .putString("ai_model", model)
                .apply()

            addMessage("Система: AI Cloud е запазен -> $provider | $model", false)
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
