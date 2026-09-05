package android.util

import java.nio.charset.StandardCharsets
import java.util.Base64 as JavaBase64

class Base64 private constructor() {
    companion object {
        const val DEFAULT = 0
        const val NO_PADDING = 1
        const val NO_WRAP = 2
        const val CRLF = 4
        const val URL_SAFE = 8
        const val NO_CLOSE = 16

        @JvmStatic
        fun decode(input: String, flags: Int): ByteArray =
            decoder(flags).decode(normalizeForDecode(input.toByteArray(StandardCharsets.US_ASCII), flags))

        @JvmStatic
        fun decode(input: ByteArray, flags: Int): ByteArray =
            decoder(flags).decode(normalizeForDecode(input, flags))

        @JvmStatic
        fun decode(input: ByteArray, offset: Int, len: Int, flags: Int): ByteArray =
            decode(input.copyOfRange(offset, offset + len), flags)

        @JvmStatic
        fun encode(input: ByteArray, flags: Int): ByteArray = encoder(flags).encode(input)

        @JvmStatic
        fun encode(input: ByteArray, offset: Int, len: Int, flags: Int): ByteArray =
            encode(input.copyOfRange(offset, offset + len), flags)

        @JvmStatic
        fun encodeToString(input: ByteArray, flags: Int): String =
            String(encode(input, flags), StandardCharsets.US_ASCII)

        @JvmStatic
        fun encodeToString(input: ByteArray, offset: Int, len: Int, flags: Int): String =
            String(encode(input, offset, len, flags), StandardCharsets.US_ASCII)

        private fun decoder(flags: Int): JavaBase64.Decoder =
            if (flags and URL_SAFE != 0) JavaBase64.getUrlDecoder() else JavaBase64.getMimeDecoder()

        private fun encoder(flags: Int): JavaBase64.Encoder {
            val selected = when {
                flags and URL_SAFE != 0 -> JavaBase64.getUrlEncoder()
                flags and NO_WRAP != 0 -> JavaBase64.getEncoder()
                flags and CRLF != 0 -> JavaBase64.getMimeEncoder(76, "\r\n".toByteArray())
                else -> JavaBase64.getMimeEncoder(76, "\n".toByteArray())
            }
            return if (flags and NO_PADDING != 0) selected.withoutPadding() else selected
        }

        private fun normalizeForDecode(input: ByteArray, flags: Int): ByteArray {
            if (flags and URL_SAFE == 0) return input
            return input.filterNot { it.toInt().toChar().isWhitespace() }.toByteArray()
        }
    }
}
