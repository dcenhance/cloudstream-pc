package com.lagradost.cloudstream3.linux.host

import com.lagradost.cloudstream3.utils.ExtractorLinkType
import kotlin.test.Test
import kotlin.test.assertEquals

class FallbackSourceDiscoveryTest {
    @Test
    fun extractsDeduplicatedDirectMediaCandidatesFromHtmlAndScripts() {
        val html = """
            <video><source src="/media/clip-720p.mp4"></video>
            <script>
              window.player = {
                "hls": "https:\/\/cdn.example\/video\/1080P\/master.m3u8?token=one",
                "dash": "https:\u002F\u002Fcdn.example\u002Fvideo\u002Fmanifest.mpd",
                "poster": "https://cdn.example/poster.jpg",
                "duplicate": "https://cdn.example/video/1080P/master.m3u8?token=one"
              };
            </script>
        """.trimIndent()

        val candidates = directMediaCandidates(html, "https://provider.example/watch/1")

        assertEquals(3, candidates.size)
        assertEquals("https://provider.example/media/clip-720p.mp4", candidates[0].url)
        assertEquals(ExtractorLinkType.VIDEO, candidates[0].type)
        assertEquals(720, candidates[0].quality)
        assertEquals(ExtractorLinkType.M3U8, candidates[1].type)
        assertEquals(ExtractorLinkType.DASH, candidates[2].type)
    }

    @Test
    fun extractsHttpIframeTargetsForRegisteredHosterFallbacks() {
        val html = "<iframe src=\"/embed/abc\"></iframe>" +
            "<iframe src=\"https://embed.example/video/abc\"></iframe>" +
            "<iframe src=\"javascript:void(0)\"></iframe>"

        assertEquals(
            listOf("https://provider.example/embed/abc", "https://embed.example/video/abc"),
            embeddedPageCandidates(html, "https://provider.example/watch/1"),
        )
    }
}
