/*
 * MPV player surface provider for desktop Compose integration
 * On Windows with GPU support, uses D3D11 direct rendering; otherwise falls back to SW
 */

package org.openani.mediamp.mpv.compose

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.openani.mediamp.compose.MediampPlayerSurfaceProvider
import org.openani.mediamp.mpv.MpvMediampPlayer
import kotlin.reflect.KClass

public class MpvMediampPlayerSurfaceProvider : MediampPlayerSurfaceProvider<MpvMediampPlayer> {
    override val forClass: KClass<MpvMediampPlayer> = MpvMediampPlayer::class

    @Composable
    override fun Surface(mediampPlayer: MpvMediampPlayer, modifier: Modifier) {
        if (mediampPlayer.isGpuRenderMode) {
            MpvD3D11PlayerSurface(mediampPlayer, modifier)
        } else {
            MpvMediampPlayerSurface(mediampPlayer, modifier)
        }
    }
}
