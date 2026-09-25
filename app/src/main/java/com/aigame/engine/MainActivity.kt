package com.aigame.engine

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

class MainActivity : AppCompatActivity() {

    private val messages = mutableListOf<ChatMessage>()
    private lateinit var adapter: ChatAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Зареждане на C++ статуса в горния екран
        val engineStatus: TextView = findViewById(R.id.engine_status)
        engineStatus.text = stringFromJNI()

        // Инициализация на чата
        val chatRecycler: RecyclerView = findViewById(R.id.chat_recycler)
        val chatInput: EditText = findViewById(R.id.chat_input)
        val btnSend: Button = findViewById(R.id.btn_send)

        adapter = ChatAdapter(messages)
        chatRecycler.layoutManager = LinearLayoutManager(this)
        chatRecycler.adapter = adapter

        // Начално съобщение
        addMessage("Система: GMS Engine е готов. Очаквам команди за генериране на 3D свят...", false)

        btnSend.setOnClickListener {
            val text = chatInput.text.toString().trim()
            if (text.isNotEmpty()) {
                addMessage(text, true)
                chatInput.text.clear()
                
                // Временна симулация на отговор (по-късно ще го вържем с AI Cloud)
                addMessage("Обработвам команда: '$text'. Изпращам към C++ ядрото...", false)
            }
        }
    }

    private fun addMessage(text: String, isUser: Boolean) {
        messages.add(ChatMessage(text, isUser))
        adapter.notifyItemInserted(messages.size - 1)
        findViewById<RecyclerView>(R.id.chat_recycler).scrollToPosition(messages.size - 1)
    }

    external fun stringFromJNI(): String

    companion object {
        init {
            System.loadLibrary("engine")
        }
    }
}
