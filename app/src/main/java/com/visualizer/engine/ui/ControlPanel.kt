package com.visualizer.engine.ui

import androidx.compose.foundation.gestures.*
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import com.visualizer.engine.NativeInterface

@Composable
fun ControlPanel() {
    var zoom by remember { mutableStateOf(1f) }
    var warp by remember { mutableStateOf(0.5f) }
    var dampening by remember { mutableStateOf(0.1f) }

    Surface(
        modifier = Modifier
            .fillMaxSize()
            .pointerInput(Unit) {
                detectTransformGestures { centroid, pan, zoomChange, rotation ->
                    zoom *= zoomChange
                    // Update native engine on change
                    NativeInterface.updateControls(zoom, warp, dampening)
                }
            },
        color = MaterialTheme.colorScheme.background.copy(alpha = 0.3f)
    ) {
        Column(
            modifier = Modifier
                .padding(16.dp)
                .fillMaxHeight(),
            verticalArrangement = Arrangement.Bottom
        ) {
            Text("Engine Controls", style = MaterialTheme.typography.titleLarge)
            
            Spacer(modifier = Modifier.height(8.dp))
            
            Text("Warp Intensity")
            Slider(
                value = warp,
                onValueChange = { 
                    warp = it 
                    NativeInterface.updateControls(zoom, warp, dampening)
                }
            )

            Text("Dampening")
            Slider(
                value = dampening,
                onValueChange = { 
                    dampening = it 
                    NativeInterface.updateControls(zoom, warp, dampening)
                }
            )
            
            // Audio visualizer bars (mock)
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(50.dp),
                horizontalArrangement = Arrangement.SpaceEvenly
            ) {
                repeat(10) {
                    Box(
                        modifier = Modifier
                            .width(8.dp)
                            .fillMaxHeight(0.5f) // This would be reactive to audio data
                            .padding(horizontal = 1.dp)
                    )
                }
            }
        }
    }
}
