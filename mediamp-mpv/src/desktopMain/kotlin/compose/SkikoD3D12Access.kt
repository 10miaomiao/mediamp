package org.openani.mediamp.mpv.compose

import org.jetbrains.skia.DirectContext
import org.openani.mediamp.mpv.MPVHandle
import java.awt.Component
import java.awt.Container
import java.awt.Window

/**
 * Reflection-based access to Compose/Skiko's D3D12 device.
 *
 * On Windows, Compose Desktop uses Skiko with D3D12 backend by default.
 * This utility finds the SkiaLayer in the AWT component hierarchy,
 * extracts the D3D12 device pointer via reflection, and provides
 * adapter + queue for creating a compatible DirectContext.
 */
object SkikoD3D12Access {

    data class ComposeD3D12Device(
        val adapterPtr: Long,
        val devicePtr: Long,
        val queuePtr: Long,
        val directContext: DirectContext? = null,
    )

    @Volatile
    private var cached: ComposeD3D12Device? = null

    /**
     * Get Compose's D3D12 device by reflecting into the SkiaLayer hierarchy.
     * Returns null if Compose is not using D3D12 backend or reflection fails.
     */
    fun getComposeD3D12Device(window: Window): ComposeD3D12Device? {
        cached?.let { return it }

        synchronized(this) {
            cached?.let { return it }

            val result = tryExtractDevice(window)
            if (result != null) {
                println("[SkikoD3D12] Found Compose D3D12 device: adapter=${result.adapterPtr}, device=${result.devicePtr}, queue=${result.queuePtr}")
                cached = result
            } else {
                println("[SkikoD3D12] Could not find Compose D3D12 device (Compose may not be using D3D12 backend)")
            }
            return result
        }
    }

    private fun tryExtractDevice(window: Window): ComposeD3D12Device? {
        // Step 1: Find SkiaLayer in the AWT component hierarchy
        // Compose 1.10+ uses WindowSkiaLayerComponent which wraps the SkiaLayer
        var skiaLayer = findComponent(window, "org.jetbrains.skiko.SkiaLayer")
        if (skiaLayer == null) {
            // Compose 1.10+: look for WindowSkiaLayerComponent
            val wrapper = findComponentByPattern(window, "WindowSkiaLayerComponent")
            if (wrapper != null) {
                println("[SkikoD3D12] Found WindowSkiaLayerComponent: ${wrapper.javaClass.name}")
                // Search inside the wrapper for SkiaLayer
                if (wrapper is Container) {
                    skiaLayer = findComponent(wrapper, "org.jetbrains.skiko.SkiaLayer")
                    if (skiaLayer == null) {
                        // Dump inner structure for debugging
                        dumpComponents(wrapper, 0, "SkikoD3D12")
                    }
                }
                // If still not found, try using the wrapper itself
                if (skiaLayer == null) {
                    skiaLayer = wrapper
                }
            }
        }
        if (skiaLayer == null) {
            println("[SkikoD3D12] SkiaLayer not found in window component hierarchy")
            return null
        }
        println("[SkikoD3D12] Found SkiaLayer: ${skiaLayer.javaClass.name}")

        // Step 2: Get redrawerManager (private field of SkiaLayer)
        // Try on the component itself and all superclasses
        val redrawerManager = tryGetRedrawerManager(skiaLayer)

        if (redrawerManager == null) {
            println("[SkikoD3D12] redrawerManager is null")
            return null
        }

        // Step 3: Get redrawer from redrawerManager
        val redrawer = when (redrawerManager) {
            is RedrawerManagerWrapper -> redrawerManager.redrawer
            else -> try {
                val field = redrawerManager.javaClass.getDeclaredField("redrawer")
                field.isAccessible = true
                field.get(redrawerManager)
            } catch (e: Exception) {
                println("[SkikoD3D12] Failed to get redrawer: ${e.message}")
                null
            }
        }

        if (redrawer == null) {
            println("[SkikoD3D12] redrawer is null (not initialized yet?)")
            return null
        }

        val redrawerClassName = redrawer.javaClass.name
        println("[SkikoD3D12] Redrawer type: $redrawerClassName")

        // Step 4: Check if it's a D3D12 redrawer
        if (!redrawerClassName.contains("Direct3D")) {
            println("[SkikoD3D12] Redrawer is not Direct3D (actual: $redrawerClassName)")
            return null
        }

        // Step 5: Get device (private Long field of Direct3DRedrawer)
        val devicePtr = try {
            val field = redrawer.javaClass.getDeclaredField("device")
            field.isAccessible = true
            field.getLong(redrawer)
        } catch (e: Exception) {
            println("[SkikoD3D12] Failed to get device: ${e.message}")
            return null
        }

        if (devicePtr == 0L) {
            println("[SkikoD3D12] device pointer is 0 (not initialized)")
            return null
        }
        println("[SkikoD3D12] D3D12 device pointer: $devicePtr (0x${devicePtr.toString(16)})")

        // Step 6: Get DirectContext from Direct3DContextHandler
        // This avoids creating a new command queue (which can crash due to Skiko's D3D12 COM object layout)
        val directContext = tryGetDirectContext(redrawer)
        if (directContext != null) {
            println("[SkikoD3D12] Got DirectContext from Skiko: $directContext")
            return ComposeD3D12Device(0L, devicePtr, 0L, directContext)
        }

        // Fallback: try to get adapter and create queue via JNI
        println("[SkikoD3D12] DirectContext not found, trying JNI fallback...")
        val adapterPtr = MPVHandle.getD3D12DefaultAdapter()
        if (adapterPtr == 0L) {
            println("[SkikoD3D12] Failed to get default DXGI adapter")
            return null
        }

        val queuePtr = MPVHandle.createD3D12CommandQueue(devicePtr)
        if (queuePtr == 0L) {
            println("[SkikoD3D12] Failed to create command queue on device")
            MPVHandle.releaseD3D12Resource(adapterPtr)
            return null
        }

        return ComposeD3D12Device(adapterPtr, devicePtr, queuePtr)
    }

    /**
     * Try to get the DirectContext from Direct3DRedrawer.contextHandler.context
     */
    private fun tryGetDirectContext(redrawer: Any): DirectContext? {
        try {
            // Direct3DRedrawer has a private field 'contextHandler' of type Direct3DContextHandler
            val contextHandlerField = redrawer.javaClass.getDeclaredField("contextHandler")
            contextHandlerField.isAccessible = true
            val contextHandler = contextHandlerField.get(redrawer)

            if (contextHandler != null) {
                println("[SkikoD3D12] Found contextHandler: ${contextHandler.javaClass.name}")

                // Direct3DContextHandler inherits from ContextBasedContextHandler which has 'context' field
                var clazz: Class<*>? = contextHandler.javaClass
                while (clazz != null) {
                    try {
                        val contextField = clazz.getDeclaredField("context")
                        contextField.isAccessible = true
                        val context = contextField.get(contextHandler)
                        if (context is DirectContext) {
                            println("[SkikoD3D12] Found DirectContext in ${clazz.simpleName}")
                            return context
                        }
                    } catch (_: NoSuchFieldException) {
                        // Try superclass
                    } catch (e: Exception) {
                        println("[SkikoD3D12] Error accessing context in ${clazz.simpleName}: ${e.message}")
                    }
                    clazz = clazz.superclass
                }

                // Also try to find 'directContext' field
                clazz = contextHandler.javaClass
                while (clazz != null) {
                    try {
                        for (field in clazz.declaredFields) {
                            if (field.type.name.contains("DirectContext") || field.name.contains("context")) {
                                field.isAccessible = true
                                val value = field.get(contextHandler)
                                if (value is DirectContext) {
                                    println("[SkikoD3D12] Found DirectContext via field scan: ${clazz.simpleName}.${field.name}")
                                    return value
                                }
                            }
                        }
                    } catch (e: Exception) {
                        println("[SkikoD3D12] Error scanning fields in ${clazz.simpleName}: ${e.message}")
                    }
                    clazz = clazz.superclass
                }
            }
        } catch (e: Exception) {
            println("[SkikoD3D12] Failed to get DirectContext: ${e.message}")
        }
        return null
    }

    private fun tryGetRedrawerManager(component: Component): Any? {
        // Try to find redrawerManager field in the component's class hierarchy
        var clazz: Class<*>? = component.javaClass
        while (clazz != null) {
            try {
                val field = clazz.getDeclaredField("redrawerManager")
                field.isAccessible = true
                val value = field.get(component)
                if (value != null) {
                    println("[SkikoD3D12] Found redrawerManager in ${clazz.simpleName}")
                    return value
                }
            } catch (_: NoSuchFieldException) {
                // Try superclass
            } catch (e: Exception) {
                println("[SkikoD3D12] Error accessing redrawerManager in ${clazz.simpleName}: ${e.message}")
            }
            clazz = clazz.superclass
        }

        // Fallback: try to get redrawer directly
        println("[SkikoD3D12] redrawerManager not found, trying redrawer directly")
        clazz = component.javaClass
        while (clazz != null) {
            try {
                val field = clazz.getDeclaredField("redrawer")
                field.isAccessible = true
                val value = field.get(component)
                if (value != null) {
                    println("[SkikoD3D12] Found redrawer directly in ${clazz.simpleName}: ${value.javaClass.name}")
                    // Return a wrapper that mimics redrawerManager
                    return RedrawerManagerWrapper(value)
                }
            } catch (_: NoSuchFieldException) {
                // Try superclass
            } catch (e: Exception) {
                println("[SkikoD3D12] Error accessing redrawer in ${clazz.simpleName}: ${e.message}")
            }
            clazz = clazz.superclass
        }

        // Debug: dump all declared fields
        println("[SkikoD3D12] No redrawerManager or redrawer found. Declared fields:")
        clazz = component.javaClass
        while (clazz != null && clazz != Any::class.java) {
            for (field in clazz.declaredFields) {
                println("[SkikoD3D12]   ${clazz.simpleName}.${field.name}: ${field.type.name}")
            }
            clazz = clazz.superclass
        }

        return null
    }

    /** Wrapper to mimic RedrawerManager when we find redrawer directly */
    private class RedrawerManagerWrapper(val redrawer: Any)

    private fun tryGetRedrawerDirectly(skiaLayer: Component): Any? {
        return try {
            val field = skiaLayer.javaClass.getDeclaredField("redrawer")
            field.isAccessible = true
            field.get(skiaLayer)
        } catch (e: Exception) {
            null
        }
    }

    private fun findComponent(container: Container, className: String): Component? {
        return container.components.asSequence()
            .filter { it.javaClass.name == className }
            .ifEmpty {
                container.components.asSequence()
                    .filterIsInstance<Container>()
                    .mapNotNull { findComponent(it, className) }
            }.firstOrNull()
    }

    private fun findComponentByPattern(container: Container, pattern: String): Component? {
        return container.components.asSequence()
            .filter { it.javaClass.name.contains(pattern) }
            .ifEmpty {
                container.components.asSequence()
                    .filterIsInstance<Container>()
                    .mapNotNull { findComponentByPattern(it, pattern) }
            }.firstOrNull()
    }

    private fun dumpComponents(container: Container, depth: Int, tag: String) {
        val indent = "  ".repeat(depth)
        for (component in container.components) {
            println("[$tag] ${indent}${component.javaClass.name}")
            if (depth < 5 && component is Container) {
                dumpComponents(component, depth + 1, tag)
            }
        }
    }
}
