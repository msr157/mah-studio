package `in`.mayankafterhours.mahcam

import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.util.Log
import java.io.FileInputStream
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Android Open Accessory session — same MAH1 framing as TCP, over USB bulk FD.
 *
 * Desktop runs `tools/mah-aoa-host` which puts the phone in accessory mode and
 * bridges bulk ↔ localhost:28887 (mah-mobile-cam listen port).
 */
class AoaSession(
    private val manager: UsbManager,
    private val accessory: UsbAccessory,
) : AutoCloseable {

    private var input: FileInputStream? = null
    private var output: FileOutputStream? = null

    fun open(): Boolean {
        val pfd = manager.openAccessory(accessory) ?: return false
        input = FileInputStream(pfd.fileDescriptor)
        output = FileOutputStream(pfd.fileDescriptor)
        Log.i(TAG, "AOA accessory open: ${accessory.model}")
        return true
    }

    /** Client→host: write MAH1 hello then typed frames (same as MahProtocol). */
    fun writeAll(bytes: ByteArray) {
        output?.write(bytes)
        output?.flush()
    }

    fun writeTyped(type: Byte, payload: ByteArray) {
        val header = ByteBuffer.allocate(5).order(ByteOrder.BIG_ENDIAN)
            .put(type)
            .putInt(payload.size)
            .array()
        writeAll(header + payload)
    }

    fun readExact(n: Int): ByteArray? {
        val buf = ByteArray(n)
        var got = 0
        val inp = input ?: return null
        while (got < n) {
            val r = inp.read(buf, got, n - got)
            if (r <= 0) return null
            got += r
        }
        return buf
    }

    override fun close() {
        try {
            input?.close()
        } catch (_: Exception) {
        }
        try {
            output?.close()
        } catch (_: Exception) {
        }
        input = null
        output = null
    }

    companion object {
        private const val TAG = "MAHCamAOA"
        const val MANUFACTURER = "MayankAfterHours"
        const val MODEL = "MAHCam"
        const val VERSION = "1.0"
    }
}
