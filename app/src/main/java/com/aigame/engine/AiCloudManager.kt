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

    suspend fun generateResponse(provider: String, apiKey: String, model: String, prompt: String): String = withContext(Dispatchers.IO) {
        try {
            val sysPrompt = "Ти си AI Game Master за 3D енджина GMS. Отговаряй кратко и ясно на български език."
            
            val request: Request = when (provider) {
                "OpenRouter", "OpenAI" -> {
                    val endpoint = if (provider == "OpenRouter") "https://openrouter.ai/api/v1/chat/completions" else "https://api.openai.com/v1/chat/completions"
                    
                    val jsonBody = JSONObject().apply {
                        put("model", model)
                        val msgs = JSONArray().apply {
                            put(JSONObject().apply {
                                put("role", "system")
                                put("content", sysPrompt)
                            })
                            put(JSONObject().apply {
                                put("role", "user")
                                put("content", prompt)
                            })
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
                                    put(JSONObject().apply {
                                        put("text", "$sysPrompt\n\nПотребител: $prompt")
                                    })
                                }
                                put("parts", parts)
                            })
                        }
                        put("contents", contents)
                    }

                    Request.Builder()
                        .url(endpoint)
                        .post(jsonBody.toString().toRequestBody(JSON_MEDIA))
                        .build()
                }
                "Anthropic" -> {
                    val endpoint = "https://api.anthropic.com/v1/messages"
                    val jsonBody = JSONObject().apply {
                        put("model", model)
                        put("max_tokens", 1024)
                        put("system", sysPrompt)
                        val msgs = JSONArray().apply {
                            put(JSONObject().apply {
                                put("role", "user")
                                put("content", prompt)
                            })
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
                else -> return@withContext "Неподдържан AI доставчик: $provider"
            }

            val response = client.newCall(request).execute()
            val body = response.body?.string() ?: return@withContext "Празен отговор от сървъра."

            if (!response.isSuccessful) {
                return@withContext "Грешка (${response.code}): $body"
            }

            val json = JSONObject(body)
            return@withContext when (provider) {
                "OpenRouter", "OpenAI" -> {
                    json.getJSONArray("choices")
                        .getJSONObject(0)
                        .getJSONObject("message")
                        .getString("content")
                }
                "Google Gemini" -> {
                    json.getJSONArray("candidates")
                        .getJSONObject(0)
                        .getJSONObject("content")
                        .getJSONArray("parts")
                        .getJSONObject(0)
                        .getString("text")
                }
                "Anthropic" -> {
                    json.getJSONArray("content")
                        .getJSONObject(0)
                        .getString("text")
                }
                else -> body
            }
        } catch (e: Exception) {
            return@withContext "Грешка при връзка: ${e.message}"
        }
    }
}
