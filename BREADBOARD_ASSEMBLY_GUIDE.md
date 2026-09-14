# Ignis-Trace Breadboard Assembly Guide

**Goal:** Get all sensors wired, powered, and communicating. Focus on getting it working, not pretty.

---

##  What You Need

### Hardware
- Arduino UNO Q (with USB power)
- Breadboard (830+ tie-points, or two smaller ones)
- Jumper wires (solid core, assorted colors)
- **Power:**
  - USB 5V from UNO Q → 5V rail (power servo)
  - 3.3V regulator (LDO, e.g., LM1117-3.3) → 3.3V rail (sensors)
  - GND rail (common to all)

### Sensors (What You're Wiring)
1. **BME680** (Gas/VOC) — I2C @ 0x77
2. **MLX90614** (Thermal) — I2C @ 0x5A
3. **INMP441** (Microphone) — I2S (optional for breadboard)
4. **SG90 Servo** (Thermal sweep) — PWM on D9
5. **SSD1306 OLED** (Optional) — I2C @ 0x3C
6. **SX1278 LoRa** (Optional) — SPI
---

##  Power Distribution (First)

```
┌─────────────────────────────────────────────┐
│         Arduino UNO Q USB 5V                │
└────────────────────────────────────────────┬┘
                                              │
                         ┌────────────────────┴──────────┐
                         │                                │
                      [RED WIRE]                    [GND - BLACK WIRE]
                         │                                │
                    Breadboard                      Breadboard
                    5V Rail                         GND Rail
                         │                                │
         ┌───────────────┤                                │
         │               │                                │
      [SG90 Servo]   [3.3V LDO]                          │
      (power pin)        │                                │
                    [OUT → 3.3V Rail] ◄─── [GND]─────────┘

3.3V Rail:    BME680, MLX90614, INMP441, SSD1306
5V Rail:      Servo only (via separate external PSU if you want)
GND Rail:     All devices
```

### Step 1: Power Setup
1. **5V from USB** → Breadboard red rail (5V)
2. **GND from UNO** → Breadboard blue rail (GND)
3. **3.3V regulator (LM1117-3.3):**
   - IN: 5V rail
   - GND: GND rail
   - OUT: New 3.3V rail on breadboard
4. **All sensor grounds** → Common GND rail

>  **Critical:** Use separate 5V for servo if possible (external USB pack or battery). Don't draw 700 mA servo spike from UNO Q 5V. For breadboard test, you can get away with it if you power just the servo signal, not power.

---

##  Pinout Summary (UNO Q → Breadboard)

| Arduino Pin | Function | Breadboard Connection | Color |
|---|---|---|---|
| **D0** | Serial RX | (debug only) | Purple |
| **D1** | Serial TX | (debug only) | Purple |
| **D9** | Servo PWM | SG90 signal wire | Orange |
| **D20** | I2C SDA | BME680, MLX90614, SSD1306 SDA | Yellow |
| **D21** | I2C SCL | BME680, MLX90614, SSD1306 SCL | Green |
| **D11, D12, D13** | SPI (optional LoRa) | (skip for now) | Gray |
| **5V** | Power 5V | Breadboard 5V rail | Red |
| **3.3V** | (use regulator) | 3.3V rail via LDO | Red (alt) |
| **GND** | Ground | Breadboard GND rail | Black |

---

##  I2C Bus Wiring (All Sensors Share)

```
UNO Q D20 (SDA) ──[4.7kΩ pull-up]──┬── BME680 SDA
                         (to 3.3V)   ├── MLX90614 SDA
                                     ├── SSD1306 SDA
                                     └── (future devices)

UNO Q D21 (SCL) ──[4.7kΩ pull-up]──┬── BME680 SCL
                         (to 3.3V)   ├── MLX90614 SCL
                                     ├── SSD1306 SCL
                                     └── (future devices)

All sensors share GND → GND rail
```

### I2C Device Addresses
- **BME680:** 0x77 (default; CSB pin grounded)
- **MLX90614:** 0x5A (default; SA0 pin grounded)
- **SSD1306:** 0x3C or 0x3D (check sensor; usually 0x3C for 0.96")

---

##  Physical Wiring Steps

### BME680 (Gas Sensor)
```
BME680 Pinout (6-pin module):
1. GND → GND rail
2. CSB → GND rail (sets I2C address to 0x77)
3. SDI (SDA) → D20 (with 4.7kΩ pull-up)
4. SCK (SCL) → D21 (with 4.7kΩ pull-up)
5. SDO → 3.3V (optional; enables alternate address)
6. VCC → 3.3V rail
```

### MLX90614 (Thermal Sensor)
```
MLX90614 Pinout (4-pin):
1. SCL → D21 (shared I2C)
2. SDA → D20 (shared I2C)
3. GND → GND rail
4. VCC → 3.3V rail
```

### SG90 Servo (Thermal Sweep)
```
SG90 Pinout (3-wire):
- Brown/Black (GND) → GND rail
- Red (VCC) → 5V rail (or external PSU)
- Orange/Yellow (Signal) → D9 PWM
```

### INMP441 (MEMS Microphone) — Optional for Breadboard
```
INMP441 Pinout (5-pin):
- GND → GND rail
- 3V3 → 3.3V rail
- CLK → (I2S clock — from Qualcomm MPU via RPC, skip for now)
- WS → (I2S word select — from Qualcomm MPU, skip for now)
- SD → (I2S data — from Qualcomm MPU, skip for now)

For breadboard testing: leave INMP441 disconnected until you have RPC bridge working.
```

### SSD1306 OLED (0.96" Display) — I2C @ 0x3C
```
SSD1306 Pinout (4-pin):
1. GND → GND rail
2. VCC → 3.3V rail
3. SCL → D21 (shared I2C)
4. SDA → D20 (shared I2C)
```

### SX1276 LoRa (Optional — Add Later)
```
Skip for now. Add SPI wiring when you integrate LoRa.
```

---

## 💻 Minimal Test Sketch

Upload this to verify all sensors are present and responding:

```cpp
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

// Sensor objects
Adafruit_BME680 bme;
Adafruit_MLX90614 mlx;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Servo thermalServo;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("\n\n[Ignis-Trace Breadboard Test]");
  delay(1000);

  // === I2C Bus ===
  Wire.begin();
  Serial.println("[I2C] Scanning for devices...");
  
  uint8_t error, address;
  int nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("  [I2C] Device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("  [ERROR] No I2C devices found! Check wiring.");
  } else {
    Serial.print("  [OK] Found ");
    Serial.print(nDevices);
    Serial.println(" device(s)");
  }

  // === BME680 ===
  Serial.println("\n[BME680] Initializing...");
  if (!bme.begin(0x77)) {
    Serial.println("  [ERROR] BME680 not found at 0x77. Check wiring/CSB pin.");
  } else {
    Serial.println("  [OK] BME680 initialized");
    // Set up oversampling and filter modes
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320°C, 150ms
  }

  // === MLX90614 ===
  Serial.println("\n[MLX90614] Initializing...");
  if (!mlx.begin()) {
    Serial.println("  [ERROR] MLX90614 not found at 0x5A. Check wiring.");
  } else {
    Serial.println("  [OK] MLX90614 initialized");
  }

  // === SSD1306 OLED ===
  Serial.println("\n[SSD1306] Initializing...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("  [ERROR] SSD1306 not found at 0x3C. Try 0x3D.");
    // Try alternate address
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println("  [ERROR] SSD1306 not found at 0x3D either.");
    } else {
      Serial.println("  [OK] SSD1306 found at 0x3D");
    }
  } else {
    Serial.println("  [OK] SSD1306 found at 0x3C");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Ignis-Trace");
    display.println("Breadboard Test");
    display.display();
  }

  // === Servo ===
  Serial.println("\n[Servo] Attaching to D9...");
  thermalServo.attach(9);
  thermalServo.write(90); // Center position
  Serial.println("  [OK] Servo attached and centered");

  Serial.println("\n[Setup Complete] Ready for testing.");
}

void loop() {
  // === BME680 Reading ===
  if (bme.performReading()) {
    Serial.print("[BME680] Temp: ");
    Serial.print(bme.temperature);
    Serial.print("°C | Gas: ");
    Serial.print(bme.gas_resistance / 1000.0);
    Serial.print("kOhm | Humidity: ");
    Serial.print(bme.humidity);
    Serial.println("%");
  }

  // === MLX90614 Reading ===
  Serial.print("[MLX90614] Ambient: ");
  Serial.print(mlx.readAmbientTempC());
  Serial.print("°C | Object: ");
  Serial.print(mlx.readObjectTempC());
  Serial.println("°C");

  // === Servo Sweep Test ===
  static int angle = 45;
  static int direction = 1;
  thermalServo.write(angle);
  Serial.print("[Servo] Angle: ");
  Serial.println(angle);
  
  angle += direction * 15;
  if (angle >= 135) direction = -1;
  if (angle <= 45) direction = 1;

  // === Display Update ===
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C) || display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("IGNIS-TRACE TEST");
    display.print("Temp: ");
    display.print(bme.temperature, 1);
    display.println("C");
    display.print("Thermal: ");
    display.print(mlx.readObjectTempC(), 1);
    display.println("C");
    display.print("Servo: ");
    display.print(angle);
    display.println("d");
    display.display();
  }

  delay(2000); // Update every 2 seconds
}
```

---

##  Assembly Checklist

- [x] **Power wiring complete**
  - [x] 5V from USB → breadboard 5V rail
  - [x] GND from UNO → breadboard GND rail
  - [x] 3.3V LDO regulator (IN from 5V, OUT to 3.3V rail)

- [x] **I2C Bus wired**
  - [x] D20 (SDA) → BME680, MLX90614, SSD1306 SDA (with 4.7kΩ pull-up)
  - [x] D21 (SCL) → BME680, MLX90614, SSD1306 SCL (with 4.7kΩ pull-up)

- [x] **Individual sensors**
  - [x] BME680: VCC, GND, SDA, SCL (CSB to GND)
  - [x] MLX90614: VCC, GND, SDA, SCL
  - [x] SSD1306: VCC, GND, SDA, SCL
  - [x] Servo: GND, 5V, Signal (D9)

- [x] **Serial monitor ready**
  - [x] USB connected to UNO Q
  - [x] Arduino IDE serial monitor @ 115200 baud

- [x] **Test sketch uploaded**
  - [x] Copy code above → Arduino IDE
  - [x] Verify + Upload
  - [x] Open serial monitor

---

##  Troubleshooting

### No I2C devices found
1. Check that **D20/D21 are not shorted** to 5V or GND
2. Verify **pull-up resistors** (4.7kΩ) are on SDA/SCL between 3.3V and data lines
3. Check **GND rail** — all sensors must share common ground

### BME680 not found at 0x77
1. Verify **CSB pin is connected to GND** (sets address to 0x77)
2. Check power: **VCC to 3.3V rail**
3. Check SDA/SCL connections

### MLX90614 not found at 0x5A
1. Verify **SA0 pin is connected to GND** (sets address to 0x5A)
2. Check power: **VCC to 3.3V rail**
3. Verify **flat side of sensor faces the object** (it's directional)

### SSD1306 not found
1. Try both **0x3C and 0x3D** (sketch will test both)
2. Check power: **VCC to 3.3V rail**
3. Verify **SDA/SCL connections**

### Servo doesn't move
1. Check **D9 is connected** to servo signal pin
2. Verify **servo GND is on GND rail**
3. Verify **servo VCC is on 5V rail** (and USB is providing power)

### Serial monitor shows garbage
1. Check **baud rate is 115200** in IDE
2. Verify **USB cable is working** (try replugging)
3. Check **D0/D1 are not shorted** or connected to sensors

---

##  Next Steps (After Breadboard Works)

1. **Confirm all sensors communicate** via test sketch
2. **Work on Edge Impulse audio model** while hardware is running
3. **Integrate INMP441** once RPC bridge is ready
4. **Add SX1276 LoRa** (optional; not critical for local testing)
5. **Test cascade logic** (gas trigger → thermal sweep → audio verification)

---

##  Photos to Take

- [x] Breadboard overview (full layout)
- [x] I2C bus detail (pull-ups, sensor connections)
- [x] Servo mounted and wired
- [x] Serial monitor output (devices found)

Use these for the `/assets/` folder later.

---

**Good luck! Post serial output if things don't work.**
