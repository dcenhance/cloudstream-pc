@file:Suppress("unused")

package com.lagradost.cloudstream3.plugins

import android.content.Context
import android.content.res.Resources

abstract class Plugin : BasePlugin() {
    open fun load(context: Context) {
        load()
    }

    var resources: Resources? = null
    var openSettings: ((Context) -> Unit)? = null
}

data class PluginData(
    val internalName: String,
    val url: String?,
    val isOnline: Boolean,
    val filePath: String,
    val version: Int,
)
