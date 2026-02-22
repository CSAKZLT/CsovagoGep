package com.example.csovagogep

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.ListView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat

class MainActivity : AppCompatActivity() {
    //Bluetooth permissions
    private val permissionsAbove31 = arrayOf(
        Manifest.permission.BLUETOOTH_SCAN,
        Manifest.permission.BLUETOOTH_CONNECT
    )
    private fun checkAndRequestPermissions() {
        val permissionsToRequest = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionsAbove31.filter {
                ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
            }
        } else {
            Toast.makeText( this, "Elavult Android verzió! /Min. Android 12 szükséges/",
                Toast.LENGTH_LONG ).show()
            return
        }
        if (permissionsToRequest.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, permissionsToRequest.toTypedArray(), 1001)
        }
    }

    //Bluetooth connect
    private lateinit var bluetoothAdapter: BluetoothAdapter
    private lateinit var deviceListAdapter: ArrayAdapter<String>
    private val foundDevices = mutableListOf<String>()
    private var selectedAddress: String? = null

    //XML elements
    private lateinit var instrukcio: TextView
    private lateinit var cbKezut: CheckBox
    private lateinit var tvKezut: TextView
    private lateinit var btnTovabb: Button
    private lateinit var btScanbutton: Button
    private lateinit var btEszkozkivallTovabb: Button
    private lateinit var devicelist: ListView

    private var cb = false

    //reciver object
    private val receiver = object : BroadcastReceiver() {
        @SuppressLint("MissingPermission")
        override fun onReceive(context: Context?, intent: Intent?) {
            if (BluetoothDevice.ACTION_FOUND == intent?.action) {
                if (ActivityCompat.checkSelfPermission( this@MainActivity,
                        Manifest.permission.BLUETOOTH_SCAN ) != PackageManager.PERMISSION_GRANTED )
                    return
                val device: BluetoothDevice? = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                device?.let {
                    val name = it.name ?: "Ismeretlen eszköz"
                    val address = it.address
                    val deviceInfo = "$name\n$address"
                    if (!foundDevices.contains(deviceInfo)) {
                        foundDevices.add(deviceInfo)
                        deviceListAdapter.notifyDataSetChanged()
                    }
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        val bluetoothManager = getSystemService(BluetoothManager::class.java)

        instrukcio = findViewById(R.id.tvInstrukcio)
        cbKezut = findViewById(R.id.cbKezesFelUt)
        tvKezut = findViewById(R.id.tvKezEsHaszUt)
        btnTovabb = findViewById(R.id.btnTovabb)
        btScanbutton = findViewById(R.id.btnScan)
        btEszkozkivallTovabb = findViewById(R.id.btnEszkozKivalalTovabb)
        devicelist = findViewById<ListView>(R.id.deviceList)

        bluetoothAdapter = bluetoothManager.adapter
        deviceListAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, foundDevices)
        devicelist.adapter = deviceListAdapter


        devicelist.setOnItemClickListener { parent, _, position, _ ->
            val selectedItem = parent.getItemAtPosition(position) as String
            val address = selectedItem.substringAfter("\n")
            selectedAddress = address
            Toast.makeText(this, "Kiválasztott: $selectedItem", Toast.LENGTH_SHORT).show()
            //Toast.makeText(this, address, Toast.LENGTH_SHORT).show()
            btEszkozkivallTovabb.visibility = View.VISIBLE
        }

        tvKezut.setOnClickListener { val intent = Intent(this, KezelesiEsHasznalatiUtmutato::class.java)
            startActivity(intent)
        }

        cbKezut.setOnClickListener {
            cb = !cb
            if (cb){
                btnTovabb.visibility = View.VISIBLE
            }else{
                btnTovabb.visibility = View.INVISIBLE
            }
        }

        btnTovabb.setOnClickListener {
            instrukcio.visibility = View.INVISIBLE
            cbKezut.visibility = View.INVISIBLE
            tvKezut.visibility = View.INVISIBLE
            btnTovabb.visibility = View.INVISIBLE

            btScanbutton.visibility = View.VISIBLE
            devicelist.visibility = View.VISIBLE

        }

        checkAndRequestPermissions()

        btScanbutton.setOnClickListener {
            startDiscovery()
        }

        registerReceiver(receiver, IntentFilter(BluetoothDevice.ACTION_FOUND))

        btEszkozkivallTovabb.setOnClickListener { val intent = Intent(this, ParameterMegadas::class.java)
            intent.putExtra("MAC_ADDRESS", selectedAddress)
            startActivity(intent)
        }

    }

    private fun startDiscovery() {
        if (!bluetoothAdapter.isEnabled) {
            startActivity(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
            return
        }
        if (ActivityCompat.checkSelfPermission( this,
                Manifest.permission.BLUETOOTH_SCAN ) != PackageManager.PERMISSION_GRANTED )
            return
        if (bluetoothAdapter.isDiscovering) {
            bluetoothAdapter.cancelDiscovery()
        }
        foundDevices.clear()
        deviceListAdapter.notifyDataSetChanged()

        bluetoothAdapter.startDiscovery()
    }


    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(receiver)
    }
}