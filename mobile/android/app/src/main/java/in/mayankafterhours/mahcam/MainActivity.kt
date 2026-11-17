package `in`.mayankafterhours.mahcam

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.core.CameraSelector
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import `in`.mayankafterhours.mahcam.databinding.ActivityMainBinding
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private var streaming = false
    private val uiScope = CoroutineScope(Dispatchers.Main + Job())

    private val statusReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val msg = intent?.getStringExtra(StreamService.EXTRA_STATUS) ?: return
            binding.status.text = msg
            if (msg.startsWith("Error") || msg == "Stopped" || msg.startsWith("Disconnected")) {
                streaming = false
                binding.toggle.text = getString(R.string.start_stream)
            }
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { granted ->
        if (granted.values.all { it }) {
            bindPreview()
        } else {
            Toast.makeText(this, "Camera permission required", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val prefs = getSharedPreferences("mahcam", MODE_PRIVATE)
        binding.host.setText(prefs.getString("host", ""))
        binding.port.setText(prefs.getString("port", "28887"))
        binding.pairing.setText(prefs.getString("pairing", "MAH1"))

        binding.discover.setOnClickListener { discover() }
        binding.toggle.setOnClickListener { toggleStream() }

        ensurePermissions()
    }

    override fun onStart() {
        super.onStart()
        val filter = IntentFilter(StreamService.ACTION_STATUS)
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(statusReceiver, filter, RECEIVER_NOT_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            registerReceiver(statusReceiver, filter)
        }
    }

    override fun onStop() {
        unregisterReceiver(statusReceiver)
        super.onStop()
    }

    private fun ensurePermissions() {
        val need = mutableListOf(
            Manifest.permission.CAMERA,
            Manifest.permission.RECORD_AUDIO,
        )
        if (Build.VERSION.SDK_INT >= 33) {
            need += Manifest.permission.POST_NOTIFICATIONS
        }
        val missing = need.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) bindPreview()
        else permissionLauncher.launch(missing.toTypedArray())
    }

    private fun bindPreview() {
        val future = ProcessCameraProvider.getInstance(this)
        future.addListener({
            val provider = future.get()
            val preview = Preview.Builder().build().also {
                it.surfaceProvider = binding.preview.surfaceProvider
            }
            provider.unbindAll()
            provider.bindToLifecycle(this, CameraSelector.DEFAULT_FRONT_CAMERA, preview)
        }, ContextCompat.getMainExecutor(this))
    }

    private fun discover() {
        binding.status.text = "Searching LAN for MAH Studio…"
        uiScope.launch {
            val host = withContext(Dispatchers.IO) {
                runCatching { MahProtocol.discover() }.getOrNull()
            }
            if (host == null) {
                binding.status.text = "No desktop found — enter IP manually (same Wi‑Fi / USB tether)"
            } else {
                binding.host.setText(host.ip)
                binding.port.setText(host.port.toString())
                binding.pairing.setText(host.pairing)
                binding.status.text = "Found ${host.ip}:${host.port}  code ${host.pairing}"
            }
        }
    }

    private fun toggleStream() {
        if (streaming) {
            StreamService.stop(this)
            streaming = false
            binding.toggle.text = getString(R.string.start_stream)
            binding.status.text = getString(R.string.status_idle)
            return
        }

        val host = binding.host.text?.toString()?.trim().orEmpty()
        val port = binding.port.text?.toString()?.toIntOrNull() ?: 28887
        val pairing = binding.pairing.text?.toString()?.trim().orEmpty().ifEmpty { "MAH1" }
        if (host.isEmpty()) {
            Toast.makeText(this, "Enter desktop IP or tap Find", Toast.LENGTH_SHORT).show()
            return
        }

        getSharedPreferences("mahcam", MODE_PRIVATE).edit()
            .putString("host", host)
            .putString("port", port.toString())
            .putString("pairing", pairing)
            .apply()

        StreamService.start(this, host, port, pairing)
        streaming = true
        binding.toggle.text = getString(R.string.stop_stream)
        binding.status.text = "Starting…"
    }
}
