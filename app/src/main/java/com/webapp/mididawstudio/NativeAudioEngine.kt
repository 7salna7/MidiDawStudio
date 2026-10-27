package com.webapp.mididawstudio

import android.content.Context
import android.media.midi.MidiDevice
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiManager
import android.os.Handler
import android.os.Looper

object NativeAudioEngine {
    init {
        System.loadLibrary("mididawnative")
    }

    external fun nativeInit()
    external fun nativeTriggerPad(channelId: Int, pcmData: FloatArray, size: Int, volume: Float)
    external fun nativeSetAutomationVolume(channelId: Int, volume: Float)

    fun startMidiOtgMonitoring(context: Context) {
        val midiManager = context.getSystemService(Context.MIDI_SERVICE) as MidiManager
        midiManager.registerDeviceCallback(object : MidiManager.DeviceCallback() {
            override fun onDeviceAdded(deviceInfo: MidiDeviceInfo) {
                if (deviceInfo.type == MidiDeviceInfo.TYPE_USB) {
                    midiManager.openDevice(deviceInfo, { device ->
                        bindMidiDevicePorts(device)
                    }, Handler(Looper.getMainLooper()))
                }
            }
        }, null)
    }

    private fun bindMidiDevicePorts(device: MidiDevice) {
        val outputPort = device.openOutputPort(0) ?: return
        outputPort.connect(object : android.media.midi.MidiReceiver() {
            override fun onSend(msg: ByteArray, offset: Int, count: Int, timestamp: Long) {
                val status = msg[offset].toInt() and 0xFF
                if (status >= 0x80 && count >= 3) {
                    val note = msg[offset + 1].toInt() and 0xFF
                    val velocity = msg[offset + 2].toInt() and 0xFF
                    
                    if (status and 0xF0 == 0x90 && velocity > 0) {
                        // Managed by native routing
                    }
                }
            }
        })
    }
}
