@file:Suppress("unused")

package com.lagradost.cloudstream3.utils

import android.content.Context
import android.content.SharedPreferences
import com.fasterxml.jackson.databind.json.JsonMapper

object DataStore {
    val mapper: JsonMapper = JsonMapper.builder().findAndAddModules().build()

    fun getSharedPrefs(context: Context): SharedPreferences =
        context.getSharedPreferences("rebuild_preference", Context.MODE_PRIVATE)
}

object DataStoreHelper {
    var currentAccount: String = "0"
}
