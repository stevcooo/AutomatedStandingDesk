# ESP32 DS3231 Clock + OLED + Daily Alarm (Wi-Fi AP Web UI)

This project runs on an **ESP32** connected to a **DS3231 RTC** and a **128×64 SSD1306 OLED**.  
The ESP32 creates a Wi-Fi Access Point, hosts a small web UI, and lets you:

- Set **date & time** on the DS3231  
- Set a **daily alarm** (HH:MM or HH:MM:SS)  
- Display current time on an OLED  
- Trigger an interrupt when the alarm fires (for your custom action)

---

## Features

- **Self-hosted AP + Web UI** (no router required)  
- **RTC-backed timekeeping** (DS3231 battery keeps time when ESP32 is off)  
- **Daily repeating alarm** using DS3231 Alarm1 + INT pin  
- **OLED live clock** update every second  
- Alarm time parser accepts both `HH:MM` and `HH:MM:SS`

---

## Hardware

### Required
- ESP32 dev board
- DS3231 RTC module
- SSD1306 128×64 I2C OLED (0x3C typical)
- Jumper wires
- (Optional) DS3231 coin cell battery (CR2032)

---

## Wiring / Schematics

### I2C Bus Connections

Both the DS3231 and OLED share the same I2C bus:

| ESP32 Pin | DS3231 Pin | OLED Pin | Notes |
|----------:|------------|----------|------|
| 3V3       | VCC        | VCC      | Use 3.3V unless your modules explicitly require 5V |
| GND       | GND        | GND      | Common ground |
| GPIO21    | SDA        | SDA      | I2C data |
| GPIO22    | SCL        | SCL      | I2C clock |

### Alarm Interrupt Connection

| ESP32 Pin | DS3231 Pin | Notes |
|----------:|------------|------|
| GPIO27    | SQW / INT  | Alarm output, active LOW. ESP32 uses INPUT_PULLUP. |

### ASCII Schematic

```
                +------------------+
                |      ESP32       |
                |                  |
3V3  ---------- | 3V3          GND | ---------- GND
GND  ---------- | GND              |
SDA (GPIO21) -- | GPIO21 (SDA)     |
SCL (GPIO22) -- | GPIO22 (SCL)     |
INT (GPIO27) -- | GPIO27           |
                +------------------+
                     |   |   |
                     |   |   |
                     |   |   +-------------------+
                     |   |                       |
                     |   +-----------+           |
                     |               |           |
          +----------------+   +----------------------+
          |    DS3231      |   |   SSD1306 OLED       |
          |                |   |                      |
          | VCC   GND SDA  |   | VCC  GND  SDA  SCL   |
          |  |     |   |   |   |  |    |    |    |    |
          +--|-----|---|---+   +--|----|----|----|----+
             |     |   |          |    |    |    |
             |     |   +----------+    |    |    |
             |     +-------------------+    |    |
             +------------------------------+    |
                                    +------------+
```

**Notes**
- Most DS3231 + SSD1306 I2C modules work fine at 3.3V. If yours is 5V-only, use 5V VCC but ensure SDA/SCL have pullups compatible with ESP32 (usually still OK because modules use open-drain).  
- INT/SQW on DS3231 is **open-drain active-low**, so ESP32 internal pull-up is correct.

---

## How It Works

1. ESP32 boots and initializes I2C, OLED, and RTC.
2. It disables and clears any old alarms on the DS3231.
3. ESP32 starts a **Wi-Fi Access Point**:
   - SSID: `ESP32-Clock`
   - Password: `12345678`
4. A web server runs at port 80 with:
   - `/` a setup UI
   - `/setTime` to set RTC time
   - `/setAlarm` to set daily alarm
   - `/alarmOff` to disable alarm
   - `/status` to read current time
5. When Alarm1 fires, DS3231 pulls INT low → ESP32 interrupt sets a flag → loop clears alarm and runs your action.

---

## Web Endpoints

### Set Date & Time
**GET /setTime?d=YYYY-MM-DD&t=HH:MM:SS**

Example:
```
/setTime?d=2025-12-01&t=14:30:00
```

### Set Daily Alarm
**GET /setAlarm?a=HH:MM** or **HH:MM:SS**

Examples:
```
/setAlarm?a=07:00
/setAlarm?a=07:00:30
```

If seconds are missing, they default to `00`.

### Disable Alarm
**GET /alarmOff**

### Read Current Time
**GET /status**

Returns:
```
YYYY-MM-DD HH:MM:SS
```

---

## Finding the AP IP Address

When ESP32 creates the AP, it prints the IP to Serial:

```cpp
Serial.print("AP IP: ");
Serial.println(WiFi.softAPIP());
```

### Default IP
ESP32 SoftAP commonly uses:
```
192.168.4.1
```
…but you should **trust the Serial output**.

### If the IP doesn’t match / you can’t connect
1. **Open Serial Monitor** at `115200 baud`
2. Reset the ESP32
3. Look for:
   ```
   AP IP: x.x.x.x
   ```
4. Use that IP in your browser:
   ```
   http://x.x.x.x/
   ```

If you still can’t see the network:
- Ensure ESP32 is in AP mode (`WiFi.mode(WIFI_AP)`)
- Confirm SSID/password in code
- Try forgetting the network on your device and reconnecting

---

## Customizing Alarm Action

In `loop()`:

```cpp
if (alarmFiredFlag) {
  alarmFiredFlag = false;
  rtc.clearAlarm(1);
  // Alarm action here
  Serial.println("ALARM!");
}
```

Replace the comment with anything you want (servo, relay, buzzer, etc).

---

## Troubleshooting

### OLED does not display
- Check OLED I2C address (0x3C vs 0x3D).
- Use an I2C scanner if unsure.
- Confirm SDA/SCL wiring and shared ground.

### Alarm never fires
- Ensure SQW/INT pin is wired to GPIO27.
- Ensure `rtc.writeSqwPinMode(DS3231_OFF)` is called.
- Ensure you cleared old alarms before setting new ones.

### “Bad alarm format”
- Your browser might submit `HH:MM` only.  
  This project supports both `HH:MM` and `HH:MM:SS`.
- If you hit it manually, keep the correct format:
  ```
  /setAlarm?a=07:30
  ```

---

## Library Dependencies

Install via Arduino Library Manager:

- **RTClib** (Adafruit)
- **Adafruit SSD1306**
- **Adafruit GFX Library**
- **WiFi** (ESP32 core)
- **WebServer** (ESP32 core)

---

## License

MIT (or whatever you prefer)
