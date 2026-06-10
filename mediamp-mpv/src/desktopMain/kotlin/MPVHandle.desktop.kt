/*
 * Copyright (C) 2024-2025 OpenAni and contributors.
 *
 * Use of this source code is governed by the Apache License version 2 license, which can be found at the following link.
 *
 * https://github.com/open-ani/mediamp/blob/main/LICENSE
 */
@file:JvmName("MPVHandleDesktop")

package org.openani.mediamp.mpv


internal actual fun attachSurface(ptr: Long, surface: Any): Boolean {
    // Desktop uses render context approach instead of surface attachment
    // The render context is created/managed via MPVHandle.createRenderContext()
    return true
}

internal actual fun detachSurface(ptr: Long): Boolean {
    // Desktop uses render context approach
    // The render context is destroyed via MPVHandle.destroyRenderContext()
    return true
}
