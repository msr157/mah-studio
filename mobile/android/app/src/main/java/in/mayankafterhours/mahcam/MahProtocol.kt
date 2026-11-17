package `in`.mayankafterhours.mahcam

import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets

/**
 * MAH1 client — see mah/PROTOCOL.md in mah-studio.
 * Media: [u8 'V'|'H'|'A'][u32be len][payload]
 */
class MahProtocol(
    private val host: String,
    private val port: Int,
    private val pairing: String,
    private val width: Int,
    private val height: Int,
    private val withAudio: Boolean = true,
    private val useH264: Boolean = false,
) : AutoCloseable {

    private var socket: Socket? = null
    private var out: BufferedOutputStream? = null

    fun connect(timeoutMs: Int = 8_000) {
        val s = Socket()
        s.tcpNoDelay = true
        s.connect(InetSocketAddress(host, port), timeoutMs)
        s.soTimeout = 15_000
        val o = BufferedOutputStream(s.getOutputStream())
        val i = BufferedInputStream(s.getInputStream())

        val codec = when {
            useH264 && withAudio -> "h264+pcm"
            useH264 -> "h264"
            withAudio -> "mjpeg+pcm"
            else -> "mjpeg"
        }
        val hello = "MAH1 $pairing ${width}x${height} $codec\n"
        o.write(hello.toByteArray(StandardCharsets.UTF_8))
        o.flush()

        val reply = readLine(i, 256) ?: error("No handshake reply")
        if (!reply.startsWith("MAH1 OK")) {
            s.close()
            error(reply.trim())
        }

        socket = s
        out = o
    }

    fun sendJpeg(jpeg: ByteArray) = sendTyped('V'.code, jpeg)

    fun sendH264(annexB: ByteArray) = sendTyped('H'.code, annexB)

    fun sendPcm(pcm: ByteArray) = sendTyped('A'.code, pcm)

    private fun sendTyped(type: Int, payload: ByteArray) {
        val o = out ?: error("Not connected")
        o.write(type)
        val header = ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN).putInt(payload.size).array()
        o.write(header)
        o.write(payload)
        o.flush()
    }

    override fun close() {
        try {
            out?.flush()
        } catch (_: Exception) {
        }
        try {
            socket?.close()
        } catch (_: Exception) {
        }
        socket = null
        out = null
    }

    companion object {
        const val DISCOVER_PORT = 28888

        data class HostInfo(val ip: String, val port: Int, val pairing: String)

        fun discover(timeoutMs: Int = 2500): HostInfo? {
            DatagramSocket().use { sock ->
                sock.broadcast = true
                sock.soTimeout = timeoutMs
                val payload = "MAH1 DISCOVER\n".toByteArray(StandardCharsets.UTF_8)
                val packet = DatagramPacket(
                    payload,
                    payload.size,
                    InetAddress.getByName("255.255.255.255"),
                    DISCOVER_PORT,
                )
                sock.send(packet)

                val buf = ByteArray(256)
                val resp = DatagramPacket(buf, buf.size)
                sock.receive(resp)
                val text = String(resp.data, 0, resp.length, StandardCharsets.UTF_8).trim()
                val parts = text.split(Regex("\\s+"))
                if (parts.size >= 5 && parts[0] == "MAH1" && parts[1] == "HOST") {
                    return HostInfo(parts[2], parts[3].toInt(), parts[4])
                }
                return null
            }
        }

        private fun readLine(input: BufferedInputStream, max: Int): String? {
            val sb = StringBuilder()
            while (sb.length < max) {
                val b = input.read()
                if (b < 0) return if (sb.isEmpty()) null else sb.toString()
                if (b == '\n'.code) break
                if (b != '\r'.code) sb.append(b.toChar())
            }
            return sb.toString()
        }
    }
}
