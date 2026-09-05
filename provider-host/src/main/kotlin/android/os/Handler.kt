@file:Suppress("unused")

package android.os

open class Looper private constructor() {
    companion object {
        private val main = Looper()
        @JvmStatic fun getMainLooper(): Looper = main
        @JvmStatic fun myLooper(): Looper = main
    }
}

open class Handler @JvmOverloads constructor(val looper: Looper = Looper.getMainLooper()) {
    open fun post(action: Runnable): Boolean { action.run(); return true }
    open fun postDelayed(action: Runnable, delayMillis: Long): Boolean { action.run(); return true }
    open fun removeCallbacks(action: Runnable) = Unit
    open fun removeCallbacksAndMessages(token: Any?) = Unit
}
