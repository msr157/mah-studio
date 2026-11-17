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
 */
class MahProtocol(
    private val host: String,
    private val port: Int,
    private val pairing: String,
    private val width: Int,
    private val height: Int,
) : AutoCloseable {

    private var socket: Socket? = null
    private var out: BufferedOutputStream? = null
    private var input: BufferedInputStream? = null

    fun connect(timeoutMs: Int = 8_000) {
        val s = Socket()
        s.tcpNoDelay = true
        s.connect(InetSocketAddress(host, port), timeoutMs)
        s.soTimeout = 15_000
        val o = BufferedOutputStream(s.getOutputStream())
        val i = BufferedInputStream(s.getInputStream())

        val hello = "MAH1 $pairing ${width}x${height} mjpeg\n"
        o.write(hello.toByteArray(StandardCharsets.UTF_8))
        o.flush()

        val reply = readLine(i, 256) ?: error("No handshake reply")
        if (!reply.startsWith("MAH1 OK")) {
            s.close()
            error(reply.trim())
        }

        socket = s
        out = o
        input = i
    }

    fun sendJpeg(jpeg: ByteArray) {
        val o = out ?: error("Not connected")
        val header = ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN).putInt(jpeg.size).array()
        o.write(header)
        o.write(jpeg)
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
        input = null
    }

    companion object {
        const val DISCOVER_PORT = 28888

        data class HostInfo(val ip: String, val port: Int, val pairing: String)

        /** UDP broadcast discover — desktop replies MAH1 HOST ip port pairing */
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
                // MAH1 HOST <ip> <port> <pairing>
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
