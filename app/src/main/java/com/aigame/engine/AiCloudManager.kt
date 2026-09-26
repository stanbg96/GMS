package com.aigame.engine

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit

object AiCloudManager {
    val client = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()

    private val JSON_MEDIA = "application/json; charset=utf-8".toMediaType()

    suspend fun fetchModels(provider: String, apiKey: String): List<String> = withContext(Dispatchers.IO) {
        val models = mutableListOf<String>()
        try {
            val request = when (provider) {
                "Google Gemini" -> Request.Builder().url("https://generativelanguage.googleapis.com/v1beta/models?key=$apiKey").build()
                "OpenAI" -> Request.Builder().url("https://api.openai.com/v1/models").addHeader("Authorization", "Bearer $apiKey").build()
                "OpenRouter" -> Request.Builder().url("https://openrouter.ai/api/v1/models").addHeader("Authorization", "Bearer $apiKey").build()
                "Anthropic" -> return@withContext listOf("claude-3-5-sonnet-20240620", "claude-3-opus-20240229", "claude-3-haiku-20240307")
                else -> return@withContext emptyList()
            }

            val response = client.newCall(request).execute()
            val body = response.body?.string() ?: return@withContext emptyList()

            if (response.isSuccessful) {
                val json = JSONObject(body)
                if (provider == "Google Gemini") {
                    val arr = json.getJSONArray("models")
                    for (i in 0 until arr.length()) models.add(arr.getJSONObject(i).getString("name").replace("models/", ""))
                } else {
                    val arr = json.getJSONArray("data")
                    for (i in 0 until arr.length()) models.add(arr.getJSONObject(i).getString("id"))
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return@withContext models
    }

    suspend fun generateResponse(provider: String, apiKey: String, model: String, prompt: String): String = withContext(Dispatchers.IO) {
        try {
            val sysPrompt = 
                "Ти си професионален AI 3D Архитект и Моделиер за GMS Game Engine.\n" +
                "Отговаряй кратко и точно на български език.\n" +
                "ТИ СТРОИШ ОБЕКТИ ОТ ОСНОВНИ 3D ДЕТАЙЛИ чрез следната команда:\n" +
                "[CMD:OBJ:x,y,z,sx,sy,sz,rx,ry,rz,r,g,b]\n" +
                "където: x,y,z са позиция; sx,sy,sz са размери (дължина, височина, ширина); rx,ry,rz са наклон в градуси; r,g,b са цветове от 0.0 до 1.0.\n\n" +
                "ПРИМЕР ЗА КОЛА (издължено шаси, 4 отделни гуми, фарове, кабина със стъкла, покрив и стопове):\n" +
                "[CMD:CLEAR]\n" +
                "[CMD:OBJ:0,0.5,0, 1.6,0.35,3.6, 0,0,0, 0.9,0.15,0.1]\n" +
                "[CMD:OBJ:-0.95,0.35,1.1, 0.25,0.7,0.7, 0,0,0, 0.15,0.15,0.15]\n" +
                "[CMD:OBJ:0.95,0.35,1.1, 0.25,0.7,0.7, 0,0,0, 0.15,0.15,0.15]\n" +
                "[CMD:OBJ:-0.95,0.35,-1.1, 0.25,0.7,0.7, 0,0,0, 0.15,0.15,0.15]\n" +
                "[CMD:OBJ:0.95,0.35,-1.1, 0.25,0.7,0.7, 0,0,0, 0.15,0.15,0.15]\n" +
                "[CMD:OBJ:0,0.7,1.0, 1.45,0.25,1.2, 0,0,0, 0.9,0.15,0.1]\n" +
                "[CMD:OBJ:0,1.05,-0.2, 1.3,0.55,1.7, 0,0,0, 0.35,0.75,0.95]\n" +
                "[CMD:OBJ:0,1.38,-0.2, 1.35,0.1,1.75, 0,0,0, 0.85,0.12,0.1]\n" +
                "[CMD:OBJ:-0.5,0.6,1.82, 0.25,0.15,0.1, 0,0,0, 1.0,0.95,0.2]\n" +
                "[CMD:OBJ:0.5,0.6,1.82, 0.25,0.15,0.1, 0,0,0, 1.0,0.95,0.2]\n" +
                "[CMD:OBJ:-0.5,0.6,-1.82, 0.25,0.15,0.1, 0,0,0, 0.95,0.1,0.1]\n" +
                "[CMD:OBJ:0.5,0.6,-1.82, 0.25,0.15,0.1, 0,0,0, 0.95,0.1,0.1]\n\n" +
                "ПРИМЕР ЗА БЪЛГАРСКА ВЪЗРОЖДЕНСКА КЪЩА:\n" +
                "[CMD:CLEAR]\n" +
                "[CMD:OBJ:0,0.8,0, 4.0,1.6,3.6, 0,0,0, 0.45,0.45,0.47]\n" +
                "[CMD:OBJ:0,2.3,0.3, 4.6,1.4,4.2, 0,0,0, 0.95,0.95,0.92]\n" +
                "[CMD:OBJ:-1.8,1.5,1.5, 0.18,0.6,0.8, 25,0,0, 0.35,0.2,0.1]\n" +
                "[CMD:OBJ:1.8,1.5,1.5, 0.18,0.6,0.8, 25,0,0, 0.35,0.2,0.1]\n" +
                "[CMD:OBJ:0,3.15,0.3, 5.2,0.35,4.8, 0,0,0, 0.32,0.18,0.12]\n" +
                "[CMD:OBJ:0,3.55,0.3, 4.2,0.45,3.6, 0,0,0, 0.28,0.15,0.10]\n\n" +
                "Винаги генерирай детайлни конструкции с подходящи пропорции за всяко текстово описание!"

            val request: Request = when (provider) {
                "OpenRouter", "OpenAI" -> {
                    val endpoint = if (provider == "OpenRouter") "https://openrouter.ai/api/v1/chat/completions" else "https://api.openai.com/v1/chat/completions"
                    val jsonBody = JSONObject().apply {
                        put("model", model)
                        val msgs = JSONArray().apply {
                            put(JSONObject().apply { put("role", "system"); put("content", sysPrompt) })
                            put(JSONObject().apply { put("role", "user"); put("content", prompt) })
                        }
                        put("messages", msgs)
                    }

                    val builder = Request.Builder()
                        .url(endpoint)
                        .addHeader("Authorization", "Bearer $apiKey")
                        .post(jsonBody.toString().toRequestBody(JSON_MEDIA))
                    
                    if (provider == "OpenRouter") {
                        builder.addHeader("HTTP-Referer", "https://github.com/stanbg96/GMS")
                        builder.addHeader("X-Title", "GMS Engine")
                    }
                    builder.build()
                }
                "Google Gemini" -> {
                    val cleanModel = if (model.startsWith("models/")) model else "models/$model"
                    val endpoint = "https://generativelanguage.googleapis.com/v1beta/$cleanModel:generateContent?key=$apiKey"
                    val jsonBody = JSONObject().apply {
                        val contents = JSONArray().apply {
                            put(JSONObject().apply {
                                val parts = JSONArray().apply {
                                    put(JSONObject().apply { put("text", "$sysPrompt\n\nПотребител: $prompt") })
                                }
                                put("parts", parts)
                            })
                        }
                        put("contents", contents)
                    }

                    Request.Builder().url(endpoint).post(jsonBody.toString().toRequestBody(JSON_MEDIA)).build()
                }
                "Anthropic" -> {
                    val endpoint = "https://api.anthropic.com/v1/messages"
                    val jsonBody = JSONObject().apply {
                        put("model", model)
                        put("max_tokens", 1024)
                        put("system", sysPrompt)
                        val msgs = JSONArray().apply {
                            put(JSONObject().apply { put("role", "user"); put("content", prompt) })
                        }
                        put("messages", msgs)
                    }

                    Request.Builder()
                        .url(endpoint)
                        .addHeader("x-api-key", apiKey)
                        .addHeader("anthropic-version", "2023-06-01")
                        .post(jsonBody.toString().toRequestBody(JSON_MEDIA))
                        .build()
                }
                else -> return@withContext "Неподдържан AI: $provider"
            }

            val response = client.newCall(request).execute()
            val body = response.body?.string() ?: return@withContext "Празен отговор."

            if (!response.isSuccessful) return@withContext "Грешка (${response.code}): $body"

            val json = JSONObject(body)
            return@withContext when (provider) {
                "OpenRouter", "OpenAI" -> json.getJSONArray("choices").getJSONObject(0).getJSONObject("message").getString("content")
                "Google Gemini" -> json.getJSONArray("candidates").getJSONObject(0).getJSONObject("content").getJSONArray("parts").getJSONObject(0).getString("text")
                "Anthropic" -> json.getJSONArray("content").getJSONObject(0).getString("text")
                else -> body
            }
        } catch (e: Exception) {
            return@withContext "Грешка при връзка: ${e.message}"
        }
    }
}
