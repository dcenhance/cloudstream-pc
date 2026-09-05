package com.lagradost.cloudstream3.linux.host

import com.lagradost.cloudstream3.utils.ExtractorLinkType
import java.net.URI

data class FallbackMediaCandidate(
    val url: String,
    val type: ExtractorLinkType,
    val quality: Int,
)

internal fun directMediaCandidates(
    html: String,
    baseUrl: String,
    maximumCandidates: Int = 32,
): List<FallbackMediaCandidate> {
    if (maximumCandidates <= 0) return emptyList()
    val normalized = html
        .replace("\\/", "/")
        .replace("\\u002F", "/", ignoreCase = true)
        .replace("&amp;", "&")
    val quoted = Regex("""[\"']((?:https?:)?//[^\"'<>\s]+|/[^\"'<>\s]+)[\"']""")
    val absolute = Regex("""https?://[^\s\"'<>\\]+""", RegexOption.IGNORE_CASE)
    val values = buildList {
        quoted.findAll(normalized).forEach { add(it.range.first to it.groupValues[1]) }
        absolute.findAll(normalized).forEach { add(it.range.first to it.value) }
    }.sortedBy { it.first }

    val base = runCatching { URI(baseUrl) }.getOrNull()
    val seen = linkedSetOf<String>()
    val output = mutableListOf<FallbackMediaCandidate>()
    for ((_, raw) in values) {
        val url = runCatching {
            val candidate = URI(raw)
            val resolved = when {
                candidate.isAbsolute -> candidate
                base != null -> base.resolve(candidate)
                else -> null
            } ?: return@runCatching null
            if (resolved.scheme?.lowercase() !in setOf("http", "https")) null else resolved.toString()
        }.getOrNull() ?: continue
        val path = runCatching { URI(url).path.lowercase() }.getOrNull() ?: continue
        val type = when {
            path.endsWith(".m3u8") || path.endsWith(".m3u") -> ExtractorLinkType.M3U8
            path.endsWith(".mpd") -> ExtractorLinkType.DASH
            path.substringAfterLast('/').substringAfterLast('.').lowercase() in videoExtensions ->
                ExtractorLinkType.VIDEO
            else -> continue
        }
        if (!seen.add(url)) continue
        val quality = qualityPattern.find(path)?.groupValues?.get(1)?.toIntOrNull() ?: 0
        output += FallbackMediaCandidate(url, type, quality)
        if (output.size >= maximumCandidates) break
    }
    return output
}

internal fun embeddedPageCandidates(html: String, baseUrl: String, maximumCandidates: Int = 16): List<String> {
    if (maximumCandidates <= 0) return emptyList()
    val base = runCatching { URI(baseUrl) }.getOrNull() ?: return emptyList()
    val output = linkedSetOf<String>()
    val iframe = Regex("""<iframe\b[^>]*\bsrc\s*=\s*[\"']([^\"']+)[\"']""", RegexOption.IGNORE_CASE)
    for (match in iframe.findAll(html.replace("&amp;", "&"))) {
        val resolved = runCatching { base.resolve(match.groupValues[1]).toString() }.getOrNull() ?: continue
        val scheme = runCatching { URI(resolved).scheme?.lowercase() }.getOrNull()
        if (scheme !in setOf("http", "https")) continue
        output += resolved
        if (output.size >= maximumCandidates) break
    }
    return output.toList()
}

private val videoExtensions = setOf(
    "3gp", "avi", "flv", "m2ts", "m4v", "mkv", "mov", "mp4", "mpeg", "mpg", "ogv", "ts", "webm", "wmv",
)

private val qualityPattern = Regex("""(?i)(?:^|[/_.-])(\d{3,4})p?(?:[/_.-]|$)""")
