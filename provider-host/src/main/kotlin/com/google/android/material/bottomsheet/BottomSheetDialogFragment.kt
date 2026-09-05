@file:Suppress("unused")

package com.google.android.material.bottomsheet

import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager

open class BottomSheetDialogFragment : Fragment() {
    open fun show(manager: FragmentManager, tag: String?) = Unit
}
