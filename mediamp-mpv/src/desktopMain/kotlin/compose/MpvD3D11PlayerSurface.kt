/*
 * Desktop MPV player surface - renders via vo=gpu + D3D11 to an AWT Canvas HWND
 * Uses SwingPanel to embed a java.awt.Canvas, then passes its HWND to mpv's wid option.
 * mpv renders directly to the native window, no pixel readback needed.
 */

package org.openani.mediamp.mpv.compose

import androidx.compose.foundation.background
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.awt.SwingPanel
import androidx.compose.ui.graphics.Color
import com.sun.jna.Native
import com.sun.jna.Pointer
import org.openani.mediamp.InternalMediampApi
import org.openani.mediamp.mpv.MpvMediampPlayer

@OptIn(InternalMediampApi::class)
@Composable
public fun MpvD3D11PlayerSurface(
    mediampPlayer: MpvMediampPlayer,
    modifier: Modifier = Modifier,
) {
    val handle = mediampPlayer.impl
    val widSetRef = remember { mutableListOf(false) }

    SwingPanel(
        modifier = modifier.background(Color.Black),
        factory = {
            java.awt.Canvas()
        },
        update = { canvas ->
            if (!canvas.isValid) return@SwingPanel
            if (widSetRef[0]) return@SwingPanel
            try {
                val ptr: Pointer = Native.getComponentPointer(canvas)
                val hwnd = Pointer.nativeValue(ptr)
                if (hwnd != 0L) {
                    println("[D3D11] Setting mpv wid=${hwnd}")
                    handle.setPropertyLong("wid", hwnd)
                    widSetRef[0] = true
                }
            } catch (e: Exception) {
                println("[D3D11] Failed to get HWND: ${e.message}")
            }
        }
    )
}
