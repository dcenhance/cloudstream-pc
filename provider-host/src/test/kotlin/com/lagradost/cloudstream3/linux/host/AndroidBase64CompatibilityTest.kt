package com.lagradost.cloudstream3.linux.host

import android.util.Base64
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class AndroidBase64CompatibilityTest {
    @Test
    fun decodesAndroidDefaultStringInput() {
        assertEquals("CloudStream", String(Base64.decode("Q2xvdWRTdHJlYW0=", Base64.DEFAULT)))
    }

    @Test
    fun encodesUrlSafeInputWithoutPaddingOrWrapping() {
        val encoded = Base64.encodeToString(
            byteArrayOf(-5, -1, -17),
            Base64.URL_SAFE or Base64.NO_PADDING or Base64.NO_WRAP,
        )
        assertEquals("-__v", encoded)
        assertContentEquals(byteArrayOf(-5, -1, -17), Base64.decode(encoded, Base64.URL_SAFE))
    }
}
