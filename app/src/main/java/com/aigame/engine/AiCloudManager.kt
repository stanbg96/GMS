package com.aigame.engine

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.util.concurrent.TimeUnit

object AiCloudManager {
    val client = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS)
        .readTimeout(15, TimeUnit.SECONDS)
        .build()

    // Функция за изтегляне на всички налични модели от избрания доставчик
    suspend fun fetchModels(provider: String, apiKey: String): List<String> = withContext(Dispatchers.IO) {
        val models = mutableListOf<String>()
        try {
            val request = when (provider) {
                "Google Gemini" -> Request.Builder()
                    .url("https://generativelanguage.googleapis.com/v1beta/models?key=$apiKey")
                    .build()
                "OpenAI" -> Request.Builder()
                    .url("https://api.openai.com/v1/models")
                    .addHeader("Authorization", "Bearer $apiKey")
                    .build()
                "OpenRouter" -> Request.Builder()
                    .url("https://openrouter.ai/api/v1/models")
                    .addHeader("Authorization", "Bearer $apiKey")
                    .build()
                "Anthropic" -> {
                    // Anthropic няма публичен краен изход за списък с модели, затова ги задаваме твърдо
                    return@withContext listOf("claude-3-5-sonnet-20240620", "claude-3-opus-20240229", "claude-3-haiku-20240307")
                }
                else -> return@withContext emptyList()
            }

            val response = client.newCall(request).execute()
            val body = response.body?.string() ?: return@withContext emptyList()

            if (response.isSuccessful) {
                val json = JSONObject(body)
                if (provider == "Google Gemini") {
                    val arr = json.getJSONArray("models")
                    for (i in 0 until arr.length()) {
                        val name = arr.getJSONObject(i).getString("name").replace("models/", "")
                        models.add(name)
                    }
                } else {
                    // OpenAI и OpenRouter използват един и същ JSON формат ("data" масив)
                    val arr = json.getJSONArray("data")
                    for (i in 0 until arr.length()) {
                        models.add(arr.getJSONObject(i).getString("id"))
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext models
    }
}
