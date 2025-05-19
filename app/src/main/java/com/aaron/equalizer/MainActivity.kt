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
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat

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
        }
    }

    private external fun initEqualizer(licenseKey: String)
    private external fun setLowGain(gain: Float)
    private external fun setMidGain(gain: Float)
    private external fun setHighGain(gain: Float)
    private external fun startAudio()
    private external fun stopAudio()
    private external fun cleanup()
}