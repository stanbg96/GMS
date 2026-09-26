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
                "Ти си професионален 3D Polygon Modeler за GMS Engine. Не правиш кубчета от Roblox, а истински гладки 3D модели с полигони:\n" +
                "[CMD:CLEAR] - изчиства стария модел\n" +
                "[CMD:CYL:x,y,z,radius,width,r,g,b] - за ОБЛИ КРЪГЛИ ГУМИ, джанти и кръгли колони\n" +
                "[CMD:WDG:x,y,z,sx,sy,sz,r,g,b] - за СКОСЕНИ СТЪКЛА, капаци на коли и ОСТРОВЪРХИ ПОКРИВИ на къщи\n" +
                "[CMD:BOX:x,y,z,sx,sy,sz,r,g,b] - за гладки стени, тънки греди, шаси и панели\n\n" +
                "ПРИМЕР ЗА ИСТИНСКА КОЛА (обли гуми + скосено стъкло + купе):\n" +
                "[CMD:CLEAR]\n" +
                "[CMD:CYL:-1.0,0.35,1.2, 0.45,0.25, 0.15,0.15,0.15]\n" +
                "[CMD:CYL:1.0,0.35,1.2, 0.45,0.25, 0.15,0.15,0.15]\n" +
                "[CMD:CYL:-1.0,0.35,-1.2, 0.45,0.25, 0.15,0.15,0.15]\n" +
                "[CMD:CYL:1.0,0.35,-1.2, 0.45,0.25, 0.15,0.15,0.15]\n" +
                "[CMD:BOX:0,0.45,0, 1.6,0.35,3.6, 0.9,0.15,0.1]\n" +
                "[CMD:BOX:0,0.7,1.0, 1.5,0.25,1.4, 0.9,0.15,0.1]\n" +
                "[CMD:WDG:0,1.0,0.6, 1.3,0.5,0.8, 0.35,0.75,0.95]\n" +
                "[CMD:BOX:0,1.0,-0.2, 1.3,0.5,0.9, 0.85,0.12,0.1]\n" +
                "[CMD:BOX:0,0.7,-1.2, 1.5,0.25,1.0, 0.9,0.15,0.1]\n" +
                "[CMD:BOX:-0.5,0.6,1.82, 0.25,0.15,0.08, 1,0.95,0.2]\n" +
                "[CMD:BOX:0.5,0.6,1.82, 0.25,0.15,0.08, 1,0.95,0.2]\n\n" +
                "ПРИМЕР ЗА БЪЛГАРСКА/ИТАЛИАНСКА КЪЩА:\n" +
                "Основа от камък [CMD:BOX], изнесен еркер [CMD:BOX], колони [CMD:CYL] и СКОСЕН ТЕРАКОТЕН ПОКРИВ [CMD:WDG]!\n" +
                "Сглобявай всеки предмет с анатомична точност!"

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
