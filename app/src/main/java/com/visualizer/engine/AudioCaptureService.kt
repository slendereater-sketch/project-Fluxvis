package com.visualizer.engine

import android.app.*
import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioPlaybackCaptureConfiguration
import android.media.AudioRecord
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.IBinder
import androidx.core.app.NotificationCompat

class AudioCaptureService : Service() {
    private var mediaProjection: MediaProjection? = null
    private var audioRecord: AudioRecord? = null
    private var isCapturing = false

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val resultCode = intent?.getIntExtra("RESULT_CODE", Activity.RESULT_CANCELED) ?: Activity.RESULT_CANCELED
        val data = intent?.getParcelableExtra<Intent>("DATA")

        if (resultCode == Activity.RESULT_OK && data != null) {
            createNotificationChannel()
            val notification = NotificationCompat.Builder(this, "CHANNEL_ID")
                .setContentTitle("Fluxvis Audio Capture")
                .setContentText("Capturing system audio...")
                .setSmallIcon(android.R.drawable.ic_media_play)
                .build()

            startForeground(1, notification)

            val projectionManager = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
            mediaProjection = projectionManager.getMediaProjection(resultCode, data)
            startAudioCapture()
        }

        return START_NOT_STICKY
    }

    private fun startAudioCapture() {
        val mp = mediaProjection ?: return
        
        val config = AudioPlaybackCaptureConfiguration.Builder(mp)
            .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
            .addMatchingUsage(AudioAttributes.USAGE_GAME)
            .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
            .build()

        val audioFormat = AudioFormat.Builder()
            .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
            .setSampleRate(44100)
            .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
            .build()

        try {
            audioRecord = AudioRecord.Builder()
                .setAudioPlaybackCaptureConfig(config)
                .setAudioFormat(audioFormat)
                .build()

            audioRecord?.startRecording()
            isCapturing = true

            Thread {
                val buffer = FloatArray(1024)
                while (isCapturing) {
                    val read = audioRecord?.read(buffer, 0, buffer.size, AudioRecord.READ_BLOCKING) ?: 0
                    if (read > 0) {
                        NativeInterface.pushAudioData(buffer.sliceArray(0 until read))
                    }
                }
            }.start()
        } catch (e: Exception) {
            e.printStackTrace()
            stopSelf()
        }
    }

    private fun createNotificationChannel() {
        val serviceChannel = NotificationChannel(
            "CHANNEL_ID",
            "Audio Capture Channel",
            NotificationManager.IMPORTANCE_LOW
        )
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(serviceChannel)
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        super.onDestroy()
        isCapturing = false
        audioRecord?.stop()
        audioRecord?.release()
        mediaProjection?.stop()
    }
}
