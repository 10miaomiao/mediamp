/*
 * MPV player surface provider for desktop Compose integration.
 *
 * Windows: GPU direct output via shared GL context (zero CPU readback).
 * Other platforms: Canvas-based SW rendering (mpv SW → ByteBuffer → Bitmap → Canvas).
 */

package org.openani.mediamp.mpv.compose

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.openani.mediamp.InternalMediampApi
import org.openani.mediamp.compose.MediampPlayerSurfaceProvider
import org.openani.mediamp.internal.Platform
import org.openani.mediamp.internal.currentPlatform
import org.openani.mediamp.mpv.MpvMediampPlayer
import kotlin.reflect.KClass

public class MpvMediampPlayerSurfaceProvider : MediampPlayerSurfaceProvider<MpvMediampPlayer> {
    override val forClass: KClass<MpvMediampPlayer> = MpvMediampPlayer::class

    @OptIn(InternalMediampApi::class)
    @Composable
    override fun Surface(mediampPlayer: MpvMediampPlayer, modifier: Modifier) {
        // Async readback path: mpv ANGLE → D3D11 → staging texture CPU回读 → Canvas
        MpvMediampPlayerSurface(mediampPlayer, modifier)
    }
}
