@file:Suppress("unused")

package com.lagradost.cloudstream3

import androidx.appcompat.app.AppCompatActivity
import com.lagradost.cloudstream3.utils.Event

open class MainActivity : AppCompatActivity() {
    companion object {
        val afterPluginsLoadedEvent = Event<Boolean>()
    }
}
