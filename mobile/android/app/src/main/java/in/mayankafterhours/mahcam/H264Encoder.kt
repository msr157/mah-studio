package `in`.mayankafterhours.mahcam

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.util.Log

/**
 * MediaCodec H.264 encoder → Annex-B access units for MAH1 type 'H'.
 */
class H264Encoder(
    private val width: Int,
    private val height: Int,
    private val bitrate: Int = 2_500_000,
    private val fps: Int = 15,
) : AutoCloseable {

    private var codec: MediaCodec? = null
    private var started = false

    fun start(): Boolean {
        return try {
            val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height)
            format.setInteger(
                MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible,
            )
            format.setInteger(MediaFormat.KEY_BIT_RATE, bitrate)
            format.setInteger(MediaFormat.KEY_FRAME_RATE, fps)
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 2)
            val c = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
            c.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            c.start()
            codec = c
            started = true
            true
        } catch (e: Exception) {
            Log.e(TAG, "H264 start failed", e)
            false
        }
    }

    fun encodeNv21(nv21: ByteArray, presentationUs: Long): List<ByteArray> {
        val c = codec ?: return emptyList()
        if (!started) return emptyList()

        val inIndex = c.dequeueInputBuffer(10_000)
        if (inIndex >= 0) {
            val inBuf = c.getInputBuffer(inIndex) ?: return emptyList()
            inBuf.clear()
            val i420 = nv21ToI420(nv21, width, height)
            inBuf.put(i420)
            c.queueInputBuffer(inIndex, 0, i420.size, presentationUs, 0)
        }

        val out = ArrayList<ByteArray>()
        val info = MediaCodec.BufferInfo()
        while (true) {
            val outIndex = c.dequeueOutputBuffer(info, 0)
            if (outIndex == MediaCodec.INFO_TRY_AGAIN_LATER) break
            if (outIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) continue
            if (outIndex < 0) break
            val buf = c.getOutputBuffer(outIndex)
            if (buf != null && info.size > 0) {
                buf.position(info.offset)
                buf.limit(info.offset + info.size)
                val bytes = ByteArray(info.size)
                buf.get(bytes)
                out.add(ensureAnnexB(bytes))
            }
            c.releaseOutputBuffer(outIndex, false)
            if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) break
        }
        return out
    }

    override fun close() {
        started = false
        try {
            codec?.stop()
        } catch (_: Exception) {
        }
        try {
            codec?.release()
        } catch (_: Exception) {
        }
        codec = null
    }

    companion object {
        private const val TAG = "MAHH264"

        fun nv21ToI420(nv21: ByteArray, width: Int, height: Int): ByteArray {
            val frameSize = width * height
            val chroma = frameSize / 4
            val out = ByteArray(frameSize + frameSize / 2)
            System.arraycopy(nv21, 0, out, 0, frameSize)
            var uv = frameSize
            var ui = 0
            var vi = 0
            while (uv < nv21.size - 1 && ui < chroma) {
                out[frameSize + chroma + vi] = nv21[uv] // V
                out[frameSize + ui] = nv21[uv + 1] // U
                uv += 2
                ui++
                vi++
            }
            return out
        }

        fun ensureAnnexB(nal: ByteArray): ByteArray {
            if (nal.size >= 4 && nal[0] == 0.toByte() && nal[1] == 0.toByte() &&
                (nal[2] == 1.toByte() || (nal[2] == 0.toByte() && nal[3] == 1.toByte()))
            ) {
                return nal
            }
            return byteArrayOf(0, 0, 0, 1) + nal
        }
    }
}
