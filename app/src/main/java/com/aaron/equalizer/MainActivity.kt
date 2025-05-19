package com.aaron.equalizer

import android.Manifest
import android.content.pm.PackageManager
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import kotlinx.coroutines.delay

class MainActivity : ComponentActivity() {
    companion object {
        init {
            System.loadLibrary("equalizer")
        }
    }

    private val licenseKey = "ExampleLicenseKey-WillExpire-OnNextUpdate" // official license key for free version of Superpowered

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Register permission request launcher
        val requestPermissionLauncher = registerForActivityResult(
            ActivityResultContracts.RequestPermission()
        ) { isGranted: Boolean ->
            if (isGranted) {
                initializeAudio()
            }
        }

        // Check and request RECORD_AUDIO permission
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            == PackageManager.PERMISSION_GRANTED
        ) {
            initializeAudio()
        } else {
            requestPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
        }

        setContent {
            EqualizerUI()
        }
    }

    private fun initializeAudio() {
        val audioManager = getSystemService(AUDIO_SERVICE) as AudioManager
        audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
        audioManager.isMicrophoneMute = false

        // Route audio to speaker for API 31+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val speakerDevice = audioManager.availableCommunicationDevices.find {
                it.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
            }
            if (speakerDevice != null) {
                audioManager.setCommunicationDevice(speakerDevice)
            }
        } else {
            @Suppress("DEPRECATION")
            audioManager.isSpeakerphoneOn = true
        }

        initEqualizer(licenseKey)
    }

    override fun onDestroy() {
        super.onDestroy()
        cleanup()
    }

    @Composable
    fun EqualizerUI() {
        var lowGain by remember { mutableFloatStateOf(1.0f) }
        var midGain by remember { mutableFloatStateOf(1.0f) }
        var highGain by remember { mutableFloatStateOf(1.0f) }
        var isAudioRunning by remember { mutableStateOf(false) }

        // Compressor state
        var compOn by remember { mutableStateOf(false) }
        var threshold by remember { mutableFloatStateOf(-20.0f) }
        var gainRed by remember { mutableStateOf(0.0f) }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(text = "Equalizer")
            Slider(
                value = lowGain,
                onValueChange = {
                    lowGain = it
                    setLowGain(it)
                },
                valueRange = 0.0f..2.0f
            )
            Slider(
                value = midGain,
                onValueChange = {
                    midGain = it
                    setMidGain(it)
                },
                valueRange = 0.0f..2.0f
            )
            Slider(
                value = highGain,
                onValueChange = {
                    highGain = it
                    setHighGain(it)
                },
                valueRange = 0.0f..2.0f
            )
            Button(onClick = {
                if (!isAudioRunning) {
                    startAudio()
                    isAudioRunning = true
                }
            }) {
                Text("Start Mic")
            }
            Button(onClick = {
                if (isAudioRunning) {
                    stopAudio()
                    isAudioRunning = false
                }
            }) {
                Text("Stop")
            }

            Divider(modifier = Modifier.padding(vertical = 8.dp))

            // Compressor controls
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "Compressor")
                Switch(
                    checked = compOn,
                    onCheckedChange = {
                        compOn = it
                        setCompressorEnabled(it)
                    }
                )
            }
            if (compOn) {
                Slider(
                    value = threshold,
                    onValueChange = {
                        threshold = it
                        setCompressorThreshold(it)
                    },
                    valueRange = -60.0f..0.0f
                )
                Text(text = "Threshold: ${"%.1f".format(threshold)} dB")

                // Live gain reduction meter
                LaunchedEffect(compOn) {
                    while (compOn) {
                        gainRed = getCompressorGainReduction()
                        delay(100L)
                    }
                }
                Text(text = "Gain reduction: ${"%.2f".format(gainRed)} dB")
            }
        }
    }

    private external fun initEqualizer(licenseKey: String)
    private external fun setLowGain(gain: Float)
    private external fun setMidGain(gain: Float)
    private external fun setHighGain(gain: Float)
    private external fun startAudio()
    private external fun stopAudio()
    private external fun cleanup()

    // Compressor JNI bindings
    private external fun setCompressorEnabled(enabled: Boolean)
    private external fun setCompressorThreshold(threshold: Float)
    private external fun getCompressorGainReduction(): Float
}
