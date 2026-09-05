package com.lagradost.cloudstream3.linux.host

import com.lagradost.cloudstream3.SubtitleFile
import com.lagradost.cloudstream3.utils.ExtractorLink
import com.lagradost.cloudstream3.utils.ExtractorLinkPlayList
import com.lagradost.cloudstream3.utils.ExtractorLinkType
import com.lagradost.cloudstream3.utils.PlayListItem
import kotlinx.serialization.json.*
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class SourceProtocolTest {
    @Suppress("DEPRECATION", "DEPRECATION_ERROR")
    @Test
    fun preservesPlaybackAndSubtitleRequestContext() {
        val link = ExtractorLink(
            source = "VOE",
            name = "VOE 1080p",
            url = "https://cdn.example/video.m3u8",
            referer = "https://voe.sx/e/1",
            quality = 1080,
            isM3u8 = true,
            headers = mapOf("Origin" to "https://voe.sx"),
        )
        val subtitle = SubtitleFile("German", "https://cdn.example/de.vtt").apply {
            headers = mapOf("Authorization" to "token")
        }

        val result = sourceResultJson(listOf(link), listOf(subtitle), true)
        val serializedLink = result["links"]!!.jsonArray.single().jsonObject
        val serializedSubtitle = result["subtitles"]!!.jsonArray.single().jsonObject

        assertTrue(result["success"]!!.jsonPrimitive.boolean)
        assertEquals("https://voe.sx/e/1", serializedLink["referer"]!!.jsonPrimitive.content)
        assertEquals("https://voe.sx", serializedLink["headers"]!!.jsonObject["Origin"]!!.jsonPrimitive.content)
        assertEquals("German", serializedSubtitle["language"]!!.jsonPrimitive.content)
        assertEquals("token", serializedSubtitle["headers"]!!.jsonObject["Authorization"]!!.jsonPrimitive.content)
    }

    @Suppress("DEPRECATION")
    @Test
    fun preservesConcatenatedPlaylistSegments() {
        val link = ExtractorLinkPlayList(
            source = "Segment host",
            name = "Segmented video",
            playlist = listOf(
                PlayListItem("https://cdn.example/part-1.mp4", 2_000_000),
                PlayListItem("https://cdn.example/part-2.mp4", 3_500_000),
            ),
            referer = "https://provider.example/",
            quality = 720,
            type = ExtractorLinkType.VIDEO,
        )

        val json = sourceResultJson(listOf(link), emptyList(), true)
        val segments = json["links"]!!.jsonArray.single().jsonObject["playlist"]!!.jsonArray

        assertEquals(2, segments.size)
        assertEquals("https://cdn.example/part-1.mp4",
            segments[0].jsonObject["url"]!!.jsonPrimitive.content)
        assertEquals(3_500_000,
            segments[1].jsonObject["durationUs"]!!.jsonPrimitive.long)
    }
}
