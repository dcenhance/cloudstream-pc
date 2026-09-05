@file:Suppress("unused")

package android.net

import java.net.URI
import java.net.URLDecoder
import java.net.URLEncoder
import java.nio.charset.StandardCharsets

class Uri private constructor(private val raw: String) {
    private val parsed: URI by lazy { URI(raw) }
    val scheme: String? get() = parsed.scheme
    val host: String? get() = parsed.host
    val path: String? get() = parsed.rawPath?.let(::decode)
    val query: String? get() = parsed.rawQuery
    val queryParameterNames: Set<String>
        get() = queryPairs().mapTo(linkedSetOf()) { it.first }

    fun getQueryParameter(key: String): String? = getQueryParameters(key).firstOrNull()
    fun getQueryParameters(key: String): List<String> = queryPairs().filter { it.first == key }.map { it.second }
    fun buildUpon(): Builder = Builder(raw)
    override fun toString(): String = raw

    private fun queryPairs(): List<Pair<String, String>> = parsed.rawQuery.orEmpty().split('&')
        .filter { it.isNotEmpty() }
        .map {
            val pieces = it.split('=', limit = 2)
            decode(pieces[0]) to decode(pieces.getOrElse(1) { "" })
        }

    class Builder internal constructor(value: String) {
        private val base = URI(value)
        private val parameters = mutableListOf<Pair<String, String>>()
        init {
            base.rawQuery.orEmpty().split('&').filter { it.isNotEmpty() }.forEach {
                val pieces = it.split('=', limit = 2)
                parameters += decode(pieces[0]) to decode(pieces.getOrElse(1) { "" })
            }
        }
        fun appendQueryParameter(key: String, value: String?): Builder = apply { parameters += key to value.orEmpty() }
        fun clearQuery(): Builder = apply { parameters.clear() }
        fun build(): Uri {
            val query = parameters.joinToString("&") { encode(it.first) + "=" + encode(it.second) }.ifEmpty { null }
            return Uri(URI(base.scheme, base.authority, base.path, query, base.fragment).toString())
        }
    }

    companion object {
        @JvmStatic fun parse(value: String): Uri = Uri(value)
        @JvmStatic fun decode(value: String): String = URLDecoder.decode(value, StandardCharsets.UTF_8)
        @JvmStatic fun encode(value: String): String = URLEncoder.encode(value, StandardCharsets.UTF_8).replace("+", "%20")
    }
}
