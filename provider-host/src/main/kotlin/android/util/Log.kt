@file:Suppress("unused")

package android.util

object Log {
    const val VERBOSE = 2
    const val DEBUG = 3
    const val INFO = 4
    const val WARN = 5
    const val ERROR = 6
    const val ASSERT = 7

    @JvmStatic fun v(tag: String?, message: String?): Int = 0
    @JvmStatic fun v(tag: String?, message: String?, error: Throwable?): Int = 0
    @JvmStatic fun d(tag: String?, message: String?): Int = 0
    @JvmStatic fun d(tag: String?, message: String?, error: Throwable?): Int = 0
    @JvmStatic fun i(tag: String?, message: String?): Int = 0
    @JvmStatic fun i(tag: String?, message: String?, error: Throwable?): Int = 0
    @JvmStatic fun w(tag: String?, message: String?): Int = 0
    @JvmStatic fun w(tag: String?, message: String?, error: Throwable?): Int = 0
    @JvmStatic fun w(tag: String?, error: Throwable?): Int = 0
    @JvmStatic fun e(tag: String?, message: String?): Int = 0
    @JvmStatic fun e(tag: String?, message: String?, error: Throwable?): Int = 0
    @JvmStatic fun wtf(tag: String?, message: String?): Int = 0
    @JvmStatic fun wtf(tag: String?, message: String?, error: Throwable?): Int = 0
    @JvmStatic fun isLoggable(tag: String?, priority: Int): Boolean = false
    @JvmStatic fun getStackTraceString(error: Throwable?): String = error?.stackTraceToString().orEmpty()
}
