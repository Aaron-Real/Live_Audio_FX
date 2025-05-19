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
import androidx.compose.foundation.layout.fillMaxSize
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
            MaterialTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    EqualizerUI()
                }
            }
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
        // EQ state
        var lowGain by remember { mutableFloatStateOf(1.0f) }
        var midGain by remember { mutableFloatStateOf(1.0f) }
        var highGain by remember { mutableFloatStateOf(1.0f) }
        var isAudioRunning by remember { mutableStateOf(false) }

        // Compressor state
        var compOn by remember { mutableStateOf(false) }
        var threshold by remember { mutableFloatStateOf(-20.0f) }
        var gainRed by remember { mutableFloatStateOf(0.0f) }

        // Pitch correction state
        var pitchOn by remember { mutableStateOf(false) }
        var scale by remember { mutableIntStateOf(1) }   // 0=CHROMATIC,1=CMAJOR,2=AMINOR…
        var range by remember { mutableIntStateOf(0) }   // 0=WIDE,1=BASS,2=TENOR…
        var speed by remember { mutableIntStateOf(2) }   // 0=SUBTLE,1=MEDIUM,2=EXTREME
        var clamp by remember { mutableIntStateOf(1) }   // 0=OFF,1=LOOSE,2=TIGHT
        var freqA by remember { mutableFloatStateOf(440f) }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
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

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
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

            HorizontalDivider()

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

                LaunchedEffect(compOn) {
                    while (compOn) {
                        gainRed = getCompressorGainReduction()
                        delay(100L)
                    }
                }
                Text(text = "Gain reduction: ${"%.2f".format(gainRed)} dB")
            }

            HorizontalDivider()

            // Pitch Correction controls
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "Auto-Tune")
                Switch(
                    checked = pitchOn,
                    onCheckedChange = {
                        pitchOn = it
                        setPitchCorrectionEnabled(it)
                    }
                )
            }
            if (pitchOn) {
                ParameterDropdown(
                    label = "Scale",
                    options = listOf(
                        "Chromatic" to 0,
                        "C Major" to 1,
                        "A Minor" to 2,
                        "Custom" to 26
                    ),
                    selected = scale,
                    onSelect = {
                        scale = it
                        setPitchCorrectionScale(it)
                    }
                )

                ParameterDropdown(
                    label = "Range",
                    options = listOf(
                        "Wide" to 0,
                        "Bass" to 1,
                        "Tenor" to 2,
                        "Alto" to 3,
                        "Soprano" to 4
                    ),
                    selected = range,
                    onSelect = {
                        range = it
                        setPitchCorrectionRange(it)
                    }
                )

                ParameterDropdown(
                    label = "Speed",
                    options = listOf(
                        "Subtle" to 0,
                        "Medium" to 1,
                        "Extreme" to 2
                    ),
                    selected = speed,
                    onSelect = {
                        speed = it
                        setPitchCorrectionSpeed(it)
                    }
                )

                ParameterDropdown(
                    label = "Clamp",
                    options = listOf(
                        "Off" to 0,
                        "Loose" to 1,
                        "Tight" to 2
                    ),
                    selected = clamp,
                    onSelect = {
                        clamp = it
                        setPitchCorrectionClamp(it)
                    }
                )

                Text(text = "Freq of A: ${freqA.toInt()} Hz")
                Slider(
                    value = freqA,
                    onValueChange = {
                        freqA = it
                        setPitchCorrectionFrequencyOfA(it)
                    },
                    valueRange = 410f..470f
                )

                Button(onClick = { resetPitchCorrection() }) {
                    Text("Reset Auto-Tune")
                }
            }
        }
    }

    // JNI bindings
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

    // Pitch Correction JNI bindings
    private external fun setPitchCorrectionEnabled(enabled: Boolean)
    private external fun setPitchCorrectionScale(scale: Int)
    private external fun setPitchCorrectionRange(range: Int)
    private external fun setPitchCorrectionSpeed(speed: Int)
    private external fun setPitchCorrectionClamp(clamp: Int)
    private external fun setPitchCorrectionFrequencyOfA(freq: Float)
    private external fun getCustomScaleNote(note: Int): Boolean
    private external fun setCustomScaleNote(note: Int, enabled: Boolean)
    private external fun resetPitchCorrection()

    // Helper dropdown composable
    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    fun ParameterDropdown(
        label: String,
        options: List<Pair<String, Int>>,
        selected: Int,
        onSelect: (Int) -> Unit
    ) {
        var expanded by remember { mutableStateOf(false) }
        val selectedText = options.firstOrNull { it.second == selected }?.first ?: ""
        ExposedDropdownMenuBox(
            expanded = expanded,
            onExpandedChange = { expanded = !expanded }
        ) {
            TextField(
                value = selectedText,
                onValueChange = {},
                label = { Text(label) },
                readOnly = true,
                trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
                modifier = Modifier.fillMaxWidth()
            )
            ExposedDropdownMenu(
                expanded = expanded,
                onDismissRequest = { expanded = false }
            ) {
                options.forEach { (text, value) ->
                    DropdownMenuItem(
                        text = { Text(text) },
                        onClick = {
                            onSelect(value)
                            expanded = false
                        }
                    )
                }
            }
        }
    }
}
