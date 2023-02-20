# LittleFSMock
Provides a simple mock of the LittleFS library which is part of the Arduino Espressif8266 framework

**Usage:**
- Implement your business logic and separate from device dependent logic. For example, do not include "Arduino.h" in your business logic
- Create your tests using the "native" framework
- Add this library to the dependencies.

platformio.ini:
```
[env:native]
platform = native
build_type = debug
debug_test = ...
lib_deps = 
	apsol/LittleFSMock@^0.1.0
	throwtheswitch/Unity@^2.5.2
```
- include "LittleFS.h" in your test file
