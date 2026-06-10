/*
 * MPV player surface provider for desktop Compose integration
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
        MpvMediampPlayerSurface(mediampPlayer, modifier)
    }
}
