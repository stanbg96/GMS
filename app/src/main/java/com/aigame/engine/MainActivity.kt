package com.aigame.engine

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.opengl.GLSurfaceView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

class MainActivity : AppCompatActivity() {

    private val messages = mutableListOf<ChatMessage>()
    private lateinit var adapter: ChatAdapter
    private lateinit var glSurfaceView: GLSurfaceView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Инициализация на 3D енджина (OpenGL ES 3.0)
        glSurfaceView = findViewById(R.id.gl_surface_view)
        glSurfaceView.setEGLContextClientVersion(3)
        glSurfaceView.setRenderer(EngineRenderer())

        // Инициализация на чата
        val chatRecycler: RecyclerView = findViewById(R.id.chat_recycler)
        val chatInput: EditText = findViewById(R.id.chat_input)
        val btnSend: Button = findViewById(R.id.btn_send)

        adapter = ChatAdapter(messages)
        chatRecycler.layoutManager = LinearLayoutManager(this)
        chatRecycler.adapter = adapter

        addMessage("Система: Хардуерният 3D графичен енджин (OpenGL ES 3.0) е стартиран успешно.", false)

        btnSend.setOnClickListener {
            val text = chatInput.text.toString().trim()
            if (text.isNotEmpty()) {
                addMessage(text, true)
                chatInput.text.clear()
                addMessage("Обработвам команда: '$text'...", false)
            }
        }
    }

    private fun addMessage(text: String, isUser: Boolean) {
        messages.add(ChatMessage(text, isUser))
        adapter.notifyItemInserted(messages.size - 1)
        findViewById<RecyclerView>(R.id.chat_recycler).scrollToPosition(messages.size - 1)
    }

    override fun onResume() {
        super.onResume()
        glSurfaceView.onResume()
    }

    override fun onPause() {
        super.onPause()
        glSurfaceView.onPause()
    }
}
