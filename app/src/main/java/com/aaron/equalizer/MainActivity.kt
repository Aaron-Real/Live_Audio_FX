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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
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
        init { System.loadLibrary("equalizer") }
    }

    private val licenseKey = "ExampleLicenseKey-WillExpire-OnNextUpdate"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val requestPermissionLauncher = registerForActivityResult(
            ActivityResultContracts.RequestPermission()
        ) { granted ->
            if (granted) initializeAudio()
        }

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

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            audioManager.availableCommunicationDevices.find {
                it.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
            }?.let { audioManager.setCommunicationDevice(it) }
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
        val scrollState = rememberScrollState()

        // Start/Stop
        var isAudioRunning by remember { mutableStateOf(false) }

        // Pitch correction
        var pitchOn by remember { mutableStateOf(false) }
        var scale by remember { mutableIntStateOf(1) }
        var range by remember { mutableIntStateOf(0) }
        var speed by remember { mutableIntStateOf(2) }
        var clamp by remember { mutableIntStateOf(1) }
        var freqA by remember { mutableFloatStateOf(440f) }

        // EQ
        var eqOn by remember { mutableStateOf(false) }
        var lowGain by remember { mutableFloatStateOf(1f) }
        var midGain by remember { mutableFloatStateOf(1f) }
        var highGain by remember { mutableFloatStateOf(1f) }

        // Compressor
        var compOn by remember { mutableStateOf(false) }
        var threshold by remember { mutableFloatStateOf(-20f) }
        var gainRed by remember { mutableFloatStateOf(0f) }

        // Distortion
        var distOn by remember { mutableStateOf(false) }
        var distInputGain by remember { mutableFloatStateOf(0f) }
        var distOutputGain by remember { mutableFloatStateOf(0f) }
        var distTone by remember { mutableFloatStateOf(1000f) }
        var distWet by remember { mutableFloatStateOf(1f) }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(scrollState)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Start/Stop Mic
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = {
                        if (!isAudioRunning) {
                            startAudio()
                            isAudioRunning = true
                        }
                    }
                ) { Text("Start Mic") }
                Button(
                    onClick = {
                        if (isAudioRunning) {
                            stopAudio()
                            isAudioRunning = false
                        }
                    }
                ) { Text("Stop") }
            }

            HorizontalDivider()

            // Pitch Correction
            Text("Auto-Tune", style = MaterialTheme.typography.titleMedium)
            Row(Modifier.fillMaxWidth(),
                Arrangement.SpaceBetween, Alignment.CenterVertically) {
                Text("Enabled")
                Switch(
                    checked = pitchOn,
                    onCheckedChange = { checked ->
                        pitchOn = checked
                        setPitchCorrectionEnabled(checked)
                    }
                    )
            }
            if (pitchOn) {
                // Scales: Chromatic, all 12 majors, A Minor, Custom
                ParameterDropdown(
                    label = "Scale",
                    options = listOf(
                        "Chromatic" to 0,
                        "C Major"    to 1,
                        "G Major"    to 2,
                        "D Major"    to 3,
                        "A Major"    to 4,
                        "E Major"    to 5,
                        "B Major"    to 6,
                        "F♯ Major"   to 7,
                        "C♯ Major"   to 8,
                        "F Major"    to 9,
                        "B♭ Major"   to 10,
                        "E♭ Major"   to 11,
                        "A Minor"    to 12,
                        "Custom"     to 26
                    ),
                    selected = scale,
                    onSelect = {
                        scale = it
                        setPitchCorrectionScale(it)
                    }
                )
                // Range: add Baritone & Mezzo-Soprano
                ParameterDropdown(
                    label = "Range",
                    options = listOf(
                        "Sub-Bass"      to 0,
                        "Bass"          to 1,
                        "Baritone"      to 2,
                        "Tenor"         to 3,
                        "Alto"          to 4,
                        "Mezzo-Soprano" to 5,
                        "Soprano"       to 6
                    ),
                    selected = range,
                    onSelect = {
                        range = it
                        setPitchCorrectionRange(it)
                    }
                )
                // Speed: add Ultra-Fast
                ParameterDropdown(
                    label = "Speed",
                    options = listOf(
                        "Subtle" to 0,
                        "Medium" to 1,
                        "Extreme" to 2,
                        "Ultra-Fast" to 3
                    ),
                    selected = speed,
                    onSelect = {
                        speed = it
                        setPitchCorrectionSpeed(it)
                    }
                )
                // Clamp: add Medium-Tight
                ParameterDropdown(
                    label = "Clamp",
                    options = listOf(
                        "Off"        to 0,
                        "Loose"      to 1,
                        "Medium-Tight" to 2,
                        "Tight"      to 3,
                        "Hard"       to 4
                    ),
                    selected = clamp,
                    onSelect = {
                        clamp = it
                        setPitchCorrectionClamp(it)
                    }
                )
                Slider(
                    value = freqA,
                    onValueChange = {
                        freqA = it
                        setPitchCorrectionFrequencyOfA(it)
                    },
                    valueRange = 410f..470f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("Freq of A: ${freqA.toInt()} Hz")
                Button(onClick = { resetPitchCorrection() }) {
                    Text("Reset Auto-Tune")
                }
            }

            HorizontalDivider()

            // Equalizer
            Text("Equalizer", style = MaterialTheme.typography.titleMedium)
            Row(Modifier.fillMaxWidth(),
                Arrangement.SpaceBetween, Alignment.CenterVertically) {
                Text("Enabled")
                Switch(
                    checked = eqOn,
                    onCheckedChange = { checked ->
                        eqOn = checked
                    }
                )
            }
            if (eqOn) {
                Slider(
                    value = lowGain,
                    onValueChange = {
                        lowGain = it
                        setLowGain(it)
                    },
                    valueRange = 0f..2f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("Low Gain: ${"%.2f".format(lowGain)}")
                Slider(
                    value = midGain,
                    onValueChange = {
                        midGain = it
                        setMidGain(it)
                    },
                    valueRange = 0f..2f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("Mid Gain: ${"%.2f".format(midGain)}")
                Slider(
                    value = highGain,
                    onValueChange = {
                        highGain = it
                        setHighGain(it)
                    },
                    valueRange = 0f..2f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("High Gain: ${"%.2f".format(highGain)}")
            }

            HorizontalDivider()

            // Compressor
            Text("Compressor", style = MaterialTheme.typography.titleMedium)
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Enabled")
                Switch(
                    checked = compOn,
                    onCheckedChange = { checked ->
                        compOn = checked
                        setCompressorEnabled(checked)
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
                    valueRange = -60f..0f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("Threshold: ${"%.1f".format(threshold)} dB")
                LaunchedEffect(compOn) {
                    while (compOn) {
                        gainRed = getCompressorGainReduction()
                        delay(100L)
                    }
                }
                Text("Gain reduction: ${"%.2f".format(gainRed)} dB")
            }

            HorizontalDivider()

            // Distortion
            Text("Distortion", style = MaterialTheme.typography.titleMedium)
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Enabled")
                Switch(
                    checked = distOn,
                    onCheckedChange = { checked ->
                        distOn = checked
                        setDistortionEnabled(checked)
                    }
                )
            }
            if (distOn) {
                Slider(
                    value = distInputGain,
                    onValueChange = {
                        distInputGain = it
                        setDistortionInputGain(it)
                    },
                    valueRange = -40f..40f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("Input Gain: ${"%.1f".format(distInputGain)} dB")

                Slider(
                    value = distOutputGain,
                    onValueChange = {
                        distOutputGain = it
                        setDistortionOutputGain(it)
                    },
                    valueRange = -40f..40f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("Output Gain: ${"%.1f".format(distOutputGain)} dB")

                Slider(
                    value = distTone,
                    onValueChange = {
                        distTone = it
                        setDistortionToneHz(it)
                    },
                    valueRange = 100f..5000f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("Tone: ${distTone.toInt()} Hz")

                Slider(
                    value = distWet,
                    onValueChange = {
                        distWet = it
                        setDistortionWet(it)
                    },
                    valueRange = 0f..1f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text("Wet: ${"%.2f".format(distWet)}")
            }
        }
    }

    // JNI bindings

    private external fun initEqualizer(licenseKey: String)
    private external fun startAudio()
    private external fun stopAudio()
    private external fun cleanup()

    private external fun setPitchCorrectionEnabled(enabled: Boolean)
    private external fun setPitchCorrectionScale(scale: Int)
    private external fun setPitchCorrectionRange(range: Int)
    private external fun setPitchCorrectionSpeed(speed: Int)
    private external fun setPitchCorrectionClamp(clamp: Int)
    private external fun setPitchCorrectionFrequencyOfA(freq: Float)
    private external fun resetPitchCorrection()

    private external fun setLowGain(gain: Float)
    private external fun setMidGain(gain: Float)
    private external fun setHighGain(gain: Float)

    private external fun setCompressorEnabled(enabled: Boolean)
    private external fun setCompressorThreshold(threshold: Float)
    private external fun getCompressorGainReduction(): Float

    private external fun setDistortionEnabled(enabled: Boolean)
    private external fun setDistortionInputGain(gain: Float)
    private external fun setDistortionOutputGain(gain: Float)
    private external fun setDistortionToneHz(hz: Float)
    private external fun setDistortionWet(wet: Float)

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
            onExpandedChange = { expanded = !expanded },
            modifier = Modifier.fillMaxWidth()
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