package com.aigame.engine

import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

data class ChatMessage(val text: String, val isUser: Boolean)

class ChatAdapter(private val messages: List<ChatMessage>) : RecyclerView.Adapter<ChatAdapter.ChatViewHolder>() {

    class ChatViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val messageContainer: LinearLayout = view.findViewById(R.id.message_container)
        val textMessage: TextView = view.findViewById(R.id.text_message)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ChatViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_chat_message, parent, false)
        return ChatViewHolder(view)
    }

    override fun onBindViewHolder(holder: ChatViewHolder, position: Int) {
        val msg = messages[position]
        holder.textMessage.text = msg.text
        
        if (msg.isUser) {
            holder.messageContainer.gravity = Gravity.END
            holder.textMessage.setBackgroundColor(0xFF4CAF50.toInt()) // Зелено за теб
            holder.textMessage.setTextColor(0xFFFFFFFF.toInt())
        } else {
            holder.messageContainer.gravity = Gravity.START
            holder.textMessage.setBackgroundColor(0xFF333333.toInt()) // Тъмно сиво за AI
            holder.textMessage.setTextColor(0xFF00FF00.toInt()) // Зелен хакерски текст
        }
    }

    override fun getItemCount() = messages.size
}
