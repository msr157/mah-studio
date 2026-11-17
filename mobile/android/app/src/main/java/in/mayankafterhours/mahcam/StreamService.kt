package `in`.mayankafterhours.mahcam

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.graphics.ImageFormat
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Build
import android.os.IBinder
import android.util.Log
import android.util.Size
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import java.io.ByteArrayOutputStream
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Matrix
import android.graphics.Rect
import android.graphics.YuvImage
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

class StreamService : Service(), LifecycleOwner {

    private val lifecycleRegistry = LifecycleRegistry(this)
    override val lifecycle: Lifecycle get() = lifecycleRegistry

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val cameraExecutor = Executors.newSingleThreadExecutor()
    private val streaming = AtomicBoolean(false)

    private var protocol: MahProtocol? = null
    private var h264: H264Encoder? = null
    private var useH264 = false
    private var audioRecord: AudioRecord? = null
    private var audioJob: Job? = null
    private var lastSentNs = 0L
    private var targetFps = 15
    private var jpegQuality = 70
    private var width = 1280
    private var height = 720

    override fun onCreate() {
        super.onCreate()
        lifecycleRegistry.currentState = Lifecycle.State.CREATED
        createChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                stopStream()
                stopSelf()
                return START_NOT_STICKY
            }
            ACTION_START -> {
                val host = intent.getStringExtra(EXTRA_HOST).orEmpty()
                val port = intent.getIntExtra(EXTRA_PORT, 28887)
                val pairing = intent.getStringExtra(EXTRA_PAIRING) ?: "MAH1"
                width = intent.getIntExtra(EXTRA_WIDTH, 1280)
                height = intent.getIntExtra(EXTRA_HEIGHT, 720)
                useH264 = intent.getBooleanExtra(EXTRA_H264, false)
                startForeground(NOTIF_ID, buildNotification("Connecting to $host:$port…"))
                lifecycleRegistry.currentState = Lifecycle.State.STARTED
                scope.launch { startStream(host, port, pairing) }
            }
        }
        return START_STICKY
    }

    private suspend fun startStream(host: String, port: Int, pairing: String) {
        if (!streaming.compareAndSet(false, true)) return
        try {
            val client = MahProtocol(host, port, pairing, width, height, withAudio = true, useH264 = useH264)
            client.connect()
            protocol = client
            if (useH264) {
                val enc = H264Encoder(width, height, fps = targetFps)
                if (enc.start()) {
                    h264 = enc
                    broadcastStatus("Connected — H.264 + mic")
                } else {
                    enc.close()
                    useH264 = false
                    broadcastStatus("H.264 failed — falling back needs reconnect as MJPEG")
                }
            } else {
                broadcastStatus("Connected — MJPEG + mic")
            }
            updateNotification("Streaming to $host:$port")
            startMic()
            bindCamera()
        } catch (e: Exception) {
            Log.e(TAG, "connect failed", e)
            broadcastStatus("Error: ${e.message}")
            streaming.set(false)
            stopSelf()
        }
    }

    private fun startMic() {
        val rate = 48000
        val channel = AudioFormat.CHANNEL_IN_MONO
        val encoding = AudioFormat.ENCODING_PCM_16BIT
        val minBuf = AudioRecord.getMinBufferSize(rate, channel, encoding)
        if (minBuf <= 0) return
        val bufSize = minBuf * 2
        val recorder = try {
            AudioRecord(MediaRecorder.AudioSource.VOICE_COMMUNICATION, rate, channel, encoding, bufSize)
        } catch (e: SecurityException) {
            Log.w(TAG, "mic permission missing", e)
            return
        }
        if (recorder.state != AudioRecord.STATE_INITIALIZED) {
            recorder.release()
            return
        }
        audioRecord = recorder
        recorder.startRecording()
        audioJob = scope.launch {
            val buf = ByteArray(bufSize)
            while (streaming.get()) {
                val n = recorder.read(buf, 0, buf.size)
                if (n > 0) {
                    try {
                        protocol?.sendPcm(buf.copyOf(n))
                    } catch (e: Exception) {
                        Log.e(TAG, "audio send failed", e)
                        break
                    }
                }
            }
        }
    }

    private fun bindCamera() {
        val future = ProcessCameraProvider.getInstance(this)
        future.addListener({
            try {
                val provider = future.get()
                provider.unbindAll()

                val analysis = ImageAnalysis.Builder()
                    .setTargetResolution(Size(width, height))
                    .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                    .build()

                analysis.setAnalyzer(cameraExecutor) { image ->
                    analyzeFrame(image)
                }

                provider.bindToLifecycle(
                    this,
                    CameraSelector.DEFAULT_FRONT_CAMERA,
                    analysis,
                )
            } catch (e: Exception) {
                Log.e(TAG, "camera bind failed", e)
                broadcastStatus("Camera error: ${e.message}")
                stopStream()
                stopSelf()
            }
        }, ContextCompat.getMainExecutor(this))
    }

    private fun analyzeFrame(image: ImageProxy) {
        try {
            if (!streaming.get()) return
            val minGap = 1_000_000_000L / targetFps
            val now = System.nanoTime()
            if (now - lastSentNs < minGap) return
            lastSentNs = now

            try {
                if (useH264 && h264 != null) {
                    val nv21 = imageProxyToNv21(image) ?: return
                    val aus = h264!!.encodeNv21(nv21, now / 1000)
                    for (au in aus) protocol?.sendH264(au)
                } else {
                    val jpeg = imageProxyToJpeg(image, jpegQuality) ?: return
                    protocol?.sendJpeg(jpeg)
                }
            } catch (e: Exception) {
                Log.e(TAG, "send failed", e)
                broadcastStatus("Disconnected: ${e.message}")
                stopStream()
                stopSelf()
            }
        } finally {
            image.close()
        }
    }

    private fun stopStream() {
        streaming.set(false)
        audioJob?.cancel()
        audioJob = null
        try {
            audioRecord?.stop()
        } catch (_: Exception) {
        }
        audioRecord?.release()
        audioRecord = null
        try {
            ProcessCameraProvider.getInstance(this).get().unbindAll()
        } catch (_: Exception) {
        }
        h264?.close()
        h264 = null
        protocol?.close()
        protocol = null
        broadcastStatus("Stopped")
    }

    override fun onDestroy() {
        stopStream()
        scope.cancel()
        cameraExecutor.shutdown()
        lifecycleRegistry.currentState = Lifecycle.State.DESTROYED
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun broadcastStatus(msg: String) {
        sendBroadcast(Intent(ACTION_STATUS).setPackage(packageName).putExtra(EXTRA_STATUS, msg))
    }

    private fun createChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val ch = NotificationChannel(CHANNEL_ID, "MAH Cam", NotificationManager.IMPORTANCE_LOW)
            getSystemService(NotificationManager::class.java).createNotificationChannel(ch)
        }
    }

    private fun buildNotification(text: String): Notification {
        val stop = PendingIntent.getService(
            this,
            0,
            Intent(this, StreamService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val open = PendingIntent.getActivity(
            this,
            1,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.notif_title))
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_launcher)
            .setContentIntent(open)
            .addAction(0, getString(R.string.stop_stream), stop)
            .setOngoing(true)
            .build()
    }

    private fun updateNotification(text: String) {
        getSystemService(NotificationManager::class.java)
            .notify(NOTIF_ID, buildNotification(text))
    }

    companion object {
        private const val TAG = "MAHCam"
        const val CHANNEL_ID = "mah_cam"
        const val NOTIF_ID = 28887
        const val ACTION_START = "in.mayankafterhours.mahcam.START"
        const val ACTION_STOP = "in.mayankafterhours.mahcam.STOP"
        const val ACTION_STATUS = "in.mayankafterhours.mahcam.STATUS"
        const val EXTRA_HOST = "host"
        const val EXTRA_PORT = "port"
        const val EXTRA_PAIRING = "pairing"
        const val EXTRA_WIDTH = "width"
        const val EXTRA_HEIGHT = "height"
        const val EXTRA_H264 = "h264"
        const val EXTRA_STATUS = "status"

        fun start(
            ctx: Context,
            host: String,
            port: Int,
            pairing: String,
            w: Int = 1280,
            h: Int = 720,
            h264: Boolean = false,
        ) {
            val i = Intent(ctx, StreamService::class.java).apply {
                action = ACTION_START
                putExtra(EXTRA_HOST, host)
                putExtra(EXTRA_PORT, port)
                putExtra(EXTRA_PAIRING, pairing)
                putExtra(EXTRA_WIDTH, w)
                putExtra(EXTRA_HEIGHT, h)
                putExtra(EXTRA_H264, h264)
            }
            ContextCompat.startForegroundService(ctx, i)
        }

        fun stop(ctx: Context) {
            ctx.startService(Intent(ctx, StreamService::class.java).setAction(ACTION_STOP))
        }
    }
}

private fun imageProxyToNv21(image: ImageProxy): ByteArray? {
    val yBuffer = image.planes[0].buffer
    val uBuffer = image.planes[1].buffer
    val vBuffer = image.planes[2].buffer
    val ySize = yBuffer.remaining()
    val uSize = uBuffer.remaining()
    val vSize = vBuffer.remaining()
    val nv21 = ByteArray(ySize + uSize + vSize)
    yBuffer.get(nv21, 0, ySize)
    vBuffer.get(nv21, ySize, vSize)
    uBuffer.get(nv21, ySize + vSize, uSize)
    return nv21
}

/** NV21 → JPEG, with rotation applied. */
private fun imageProxyToJpeg(image: ImageProxy, quality: Int): ByteArray? {
    val nv21 = imageProxyToNv21(image) ?: return null

    val yuv = YuvImage(nv21, ImageFormat.NV21, image.width, image.height, null)
    val baos = ByteArrayOutputStream()
    if (!yuv.compressToJpeg(Rect(0, 0, image.width, image.height), quality, baos)) return null
    val raw = baos.toByteArray()

    val rotation = image.imageInfo.rotationDegrees
    if (rotation == 0) return raw

    val bmp = BitmapFactory.decodeByteArray(raw, 0, raw.size) ?: return raw
    val matrix = Matrix().apply { postRotate(rotation.toFloat()) }
    val rotated = Bitmap.createBitmap(bmp, 0, 0, bmp.width, bmp.height, matrix, true)
    if (rotated !== bmp) bmp.recycle()
    val out = ByteArrayOutputStream()
    rotated.compress(Bitmap.CompressFormat.JPEG, quality, out)
    rotated.recycle()
    return out.toByteArray()
}
