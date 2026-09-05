@file:Suppress("unused")

package com.lagradost.cloudstream3.network

import okhttp3.Headers
import okhttp3.Interceptor
import okhttp3.Response

/**
 * Desktop ABI for providers that attach CloudStream's Android WebView-based
 * Cloudflare interceptor. Linux cannot solve WebView challenges here, so this
 * preserves ordinary requests and exposes empty cookie headers.
 */
class CloudflareKiller : Interceptor {
    val savedCookies: MutableMap<String, Map<String, String>> = mutableMapOf()

    fun getCookieHeaders(url: String): Headers = Headers.Builder().build()

    override fun intercept(chain: Interceptor.Chain): Response = chain.proceed(chain.request())

    companion object {
        @JvmStatic
        fun parseCookieMap(cookie: String): Map<String, String> = cookie.split(';').mapNotNull { item ->
            val pieces = item.split('=', limit = 2)
            val key = pieces.firstOrNull()?.trim().orEmpty()
            val value = pieces.getOrNull(1)?.trim().orEmpty()
            if (key.isEmpty() || value.isEmpty()) null else key to value
        }.toMap()
    }
}
