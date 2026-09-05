@file:Suppress("unused")

package com.lagradost.cloudstream3.plugins

object PluginManager {
    private var online: Array<PluginData> = emptyArray()
    private var local: Array<PluginData> = emptyArray()

    fun getPluginsOnline(): Array<PluginData> = online.copyOf()
    fun getPluginsLocal(): Array<PluginData> = local.copyOf()
    fun unloadPlugin(filePath: String) = Unit

    fun setCurrentPlugin(data: PluginData?) {
        online = if (data?.isOnline == true) arrayOf(data) else emptyArray()
        local = if (data != null && !data.isOnline) arrayOf(data) else emptyArray()
    }
}
