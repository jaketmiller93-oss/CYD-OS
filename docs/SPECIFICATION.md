# Project Specification: CYD OS — Wireless ESP32 Application Development Environment

## 1. Project Overview

Develop a lightweight, modular, touchscreen-based operating environment for ESP32 development boards called **CYD OS**.

The primary objective is to eliminate the repetitive process of physically connecting an ESP32 to a computer, flashing firmware over USB, resetting the device, and disconnecting it every time an application is modified.

CYD OS should allow applications to be developed using Codex on a Linux desktop, transferred wirelessly to the ESP32, and immediately tested on the device.

The ESP32 should remain connected to a USB power supply or battery throughout development.

CYD OS should provide a smartphone-like experience with a graphical home screen, installed applications, project folders, version management, file storage, settings, and a wireless development interface.

The system should support two different types of software:

1. **CYD OS applications:** Installable applications developed specifically for the operating environment, capable of running without reflashing or rebooting the ESP32.
2. **Standalone ESP32 firmware:** Conventional compiled firmware images installed using ESP-IDF's OTA functionality, requiring a reboot.

The first category is the primary focus. The second is an advanced feature to implement after the core system is working reliably.

This is not intended to be a Linux distribution, Android replacement, or general-purpose desktop operating system. It is an embedded application platform built on ESP-IDF and FreeRTOS.

---

## 2. Target Hardware

### Primary Target

Elecrow CrowPanel Advance 4.3-inch ESP32-S3 HMI touchscreen.

Expected hardware:

- ESP32-S3 dual-core processor.
- 800 × 480 capacitive touchscreen.
- 16 MB flash, subject to verification.
- External PSRAM, capacity to be detected.
- MicroSD card slot.
- Wi-Fi and Bluetooth LE.
- USB-C power and programming.
- Exposed UART and I²C interfaces.
- Onboard peripherals such as RTC, audio, and battery circuitry where available.

Use the exact manufacturer's schematic, pin definitions, board revision, and display examples when configuring the hardware.

Do not assume the board uses a conventional SPI display. Verify its actual display interface and driver requirements.

### Secondary Target

NM-CYD-C5 ESP32-C5 development board.

Reported hardware:

- ESP32-C5 RISC-V processor.
- 16 MB flash.
- Reported 8 MB RAM/PSRAM, to be verified.
- Integrated touchscreen.
- MicroSD slot.
- Wi-Fi connectivity.

### Cross-Platform Requirement

Maintain one shared CYD OS codebase with board-specific hardware abstraction layers.

Proposed project structure:

```text
cyd-os/
├── components/
│   ├── app_runtime/
│   ├── app_manager/
│   ├── storage/
│   ├── network/
│   ├── web_api/
│   ├── ota_manager/
│   ├── hardware_api/
│   └── ui/
├── boards/
│   ├── crowpanel_s3/
│   └── nm_cyd_c5/
├── apps/
│   ├── settings/
│   ├── file_manager/
│   └── diagnostics/
├── tools/
│   └── cydctl/
├── docs/
└── README.md
```

Both boards should share the application package format, deployment protocol, project organization, and core application APIs wherever hardware permits.

Hardware-specific capabilities should be exposed through feature detection.

Do not assume all applications can run on both boards without modification.

---

## 3. Overall System Architecture

The system should have five major layers.

### Layer 1: Hardware Abstraction

Responsible for:

- Display initialization.
- Touchscreen input.
- SD card access.
- Wi-Fi connectivity.
- GPIO management.
- UART access.
- I²C access.
- Battery status when supported.
- Audio when supported.
- Device-specific hardware configuration.

### Layer 2: Core System Services

Responsible for:

- Application installation.
- Application lifecycle management.
- Project organization.
- File management.
- Network management.
- Logging and diagnostics.
- Persistent settings.
- OTA firmware updates.
- Recovery behavior.

### Layer 3: Application Runtime

Execute compatible application packages without reflashing firmware.

Use an embedded scripting runtime, preferably Lua, as the initial implementation.

Expose hardware and graphical functions through a controlled application API.

### Layer 4: Graphical Shell

Provide:

- Home screen.
- Application launcher.
- Projects folder.
- Application management.
- Settings.
- File browser.
- Notifications.
- Status indicators.

### Layer 5: Wireless Development Interface

Provide:

- HTTP-based deployment API.
- Browser development dashboard.
- Remote application management.
- Log streaming.
- Application package uploads.
- Version history.
- Project backups and exports.
- OTA maintenance operations.

---

## 4. Graphical User Interface

Use LVGL as the primary graphical framework.

Design the interface to look and operate like a compact smartphone or handheld computer.

### Home Screen

Provide a grid of application icons.

Examples of built-in applications:

- Projects.
- Files.
- Settings.
- Diagnostics.
- App Manager.
- Terminal / Logs.

User-installed applications should appear on the home screen only when explicitly promoted or pinned.

Support:

- Tap to open.
- Long press for application options.
- Application folders.
- Scrolling.
- Custom application icons.
- Application names.
- Home-screen organization.

Preserve the home-screen layout between restarts.

### System Status Bar

Display:

- Wi-Fi connection status.
- Device IP address through a status/details screen.
- SD card status.
- Available storage.
- Battery state when hardware supports valid measurements.
- System time when available.

### Navigation

Provide a persistent way to return to the home screen.

Prefer an on-screen Home control.

Applications should not be allowed to permanently remove system navigation controls.

Include a means of recovering from an unresponsive application.

### Visual Design

Use a modern dark theme initially.

Support:

- Consistent typography.
- Rounded application icons.
- Touch-friendly controls.
- Smooth scrolling.
- Responsive layouts.
- Clear error messages.
- Loading indicators.
- Progress bars for uploads and installations.

The interface must scale appropriately between the 800 × 480 CrowPanel and the smaller C5 display.

Prioritize performance and touch responsiveness over elaborate animations.

---

## 5. Application Runtime

This is the most important component of the project.

### Objective

Allow new applications to be installed, started, stopped, updated, and removed without flashing new firmware to the ESP32.

### Initial Runtime

Implement a Lua interpreter integrated with ESP-IDF.

Each application should have its own execution context and allocated resources.

Applications should interact with hardware and system services through defined APIs rather than directly manipulating hardware registers.

### Application Lifecycle

Support the following states:

- Installed.
- Starting.
- Running.
- Suspended, if supported.
- Stopping.
- Stopped.
- Crashed.

The system must provide lifecycle functions equivalent to:

```text
install()
start()
stop()
restart()
uninstall()
```

The manager should release application resources when an application stops.

Applications must not retain display objects, event handlers, hardware locks, or unnecessary memory after termination.

### Important Runtime Limitations

Lua applications are not equivalent to independently compiled ESP32 firmware.

They can only access capabilities exposed by the CYD OS runtime.

Adding a new low-level hardware driver or ESP-IDF functionality may require updating the core OS.

The first release should run one foreground third-party application at a time.

Background applications and multitasking can be considered later.

Use execution limits, memory accounting, watchdogs, and cooperative cancellation where practical. Do not claim Lua scripts are fully isolated or secure merely because they use separate interpreter states.

---

## 6. Application API

Create a documented, versioned API for CYD OS applications.

Example namespaces:

```lua
cyd.ui
cyd.storage
cyd.system
cyd.network
cyd.gpio
cyd.uart
cyd.i2c
cyd.time
cyd.log
```

### UI API

Allow apps to create:

- Text labels.
- Buttons.
- Images.
- Lists.
- Sliders.
- Text input.
- Simple charts.
- Dialog boxes.
- Application-specific screens.

Example conceptual API:

```lua
local screen = cyd.ui.create_screen()

local label = screen:add_label({
    text = "Hello from CYD OS",
    x = 20,
    y = 20
})

screen:add_button({
    text = "Press Me",
    x = 20,
    y = 80,
    on_click = function()
        label:set_text("Button pressed")
    end
})

cyd.ui.show(screen)
```

This is a proposed API, not an existing implemented library.

Define its behavior clearly before implementing applications.

### Storage API

Allow each application access to its own private data directory.

Example:

```lua
cyd.storage.write("settings.json", data)
```

Prevent ordinary apps from accidentally overwriting other applications' files or core system data.

### Hardware API

Expose supported devices through controlled interfaces.

Examples:

```lua
cyd.uart.open(...)
cyd.i2c.read(...)
cyd.gpio.write(...)
```

Use a permission and resource-ownership system to avoid multiple applications conflicting over hardware.

System drivers should retain control over peripherals shared with the display, touchscreen, SD card, or other core services.

---

## 7. Application Package Format

Create a custom installable package format:

**`.cydapp`**

This can initially be a ZIP archive containing application scripts, metadata, icons, and resources.

Example:

```text
GPSNavigator.cydapp
├── manifest.json
├── main.lua
├── icon.png
└── assets/
```

Example manifest:

```json
{
  "package_id": "com.local.gpsnavigator",
  "name": "GPS Navigator",
  "version": "0.1.0",
  "api_version": 1,
  "entry_point": "main.lua",
  "icon": "icon.png",
  "target": ["esp32s3", "esp32c5"],
  "permissions": [
    "uart",
    "i2c",
    "storage"
  ]
}
```

The application installer must validate:

- Package structure.
- Manifest schema.
- Application identifier.
- API compatibility.
- Device compatibility.
- Required permissions.
- Available storage.
- Archive integrity.

Reject malformed, oversized, or incompatible packages.

Prevent ZIP path traversal and unauthorized filesystem writes.

A package should not require the ESP32 to recompile source code.

---

## 8. Project Management

CYD OS must distinguish between finished applications and development projects.

### Home Screen Applications

Only approved or promoted applications appear on the primary home screen.

### Projects Folder

Provide a dedicated Projects application.

Inside Projects, organize software by project name.

Example:

```text
Projects/
├── GPS Navigator/
│   ├── v0.1/
│   ├── v0.2/
│   ├── v0.3/
│   └── v0.4/
├── WiFi Scanner/
│   ├── v0.1/
│   └── v0.2/
└── Sensor Dashboard/
    └── v0.1/
```

### Version Management

Each deployment should optionally create a new saved version.

The system should track:

- Project name.
- Package identifier.
- Version number.
- Upload time.
- Package size.
- Deployment status.
- User notes.
- Last known working version.

Support:

- Launch selected version.
- Restore a previous version.
- Duplicate a version.
- Rename project.
- Archive version.
- Export project.
- Delete version.
- Delete project.
- Promote selected version to home screen.

When a project reaches a stable alpha release, the user should be able to keep that version and remove earlier prototypes.

Versions should remain available for rollback until explicitly deleted or pruned according to a configurable retention policy.

Use safe file replacement and maintain a last-known-working version where possible.

---

## 9. Wireless Connectivity

Implement two Wi-Fi operating modes.

### Station Mode

The device connects to an existing Wi-Fi network.

Example device address:

```text
http://cyd.local
```

Use mDNS when supported and reliable.

Also display the actual IP address for networks where mDNS resolution fails.

### Access Point Mode

The CYD creates its own Wi-Fi network.

Example:

```text
SSID: CYD-OS
Address: http://192.168.4.1
```

Use a configurable, protected wireless network rather than an open access point by default.

### Network Configuration

Provide touchscreen settings for:

- Scanning available networks.
- Selecting a network.
- Entering the password.
- Saving network credentials.
- Reconnecting automatically.
- Switching to access-point mode.

The system should remain usable offline.

---

## 10. Web Development Dashboard

Create a browser-based development dashboard hosted directly by the ESP32.

It should work from a Linux desktop, iPad, or smartphone.

### Dashboard Features

Display:

- Device model.
- Firmware version.
- IP address.
- Wi-Fi status.
- Memory usage.
- Flash usage.
- SD card usage.
- Running application.
- Installed applications.
- Development projects.

### Project Management

Allow users to:

- Upload application packages.
- Select a project.
- View saved versions.
- Deploy a new version.
- Run an application.
- Stop an application.
- Restart an application.
- Download stored versions.
- Delete old versions.
- Export backups.
- View deployment history.

### Remote Debugging

Include a live log viewer showing:

- Application output.
- Lua runtime errors.
- Application crashes.
- Memory allocation failures.
- Hardware access errors.
- Application lifecycle events.

Prefer WebSocket log streaming where feasible, with polling as an acceptable initial fallback.

Display both the error message and relevant application context.

Do not expose credentials or sensitive configuration values in logs.

### Security

Require authentication for operations that upload, execute, modify, or delete software.

Do not expose an unauthenticated code execution API.

Validate upload sizes, content, and filesystem paths.

Keep deployment access restricted to trusted networks or authenticated users.

---

## 11. Codex Integration

Create a command-line deployment tool for Linux named:

**`cydctl`**

The goal is to let Codex deploy applications with a single command.

Example:

```bash
cydctl deploy ./projects/gps-navigator \
  --device cyd.local \
  --run
```

The command should:

1. Read the application's manifest.
2. Validate the package structure.
3. Package the project.
4. Create a new development revision.
5. Upload it over Wi-Fi.
6. Ask the CYD to install it.
7. Stop the currently running version if required.
8. Launch the newly deployed version.
9. Return deployment status.
10. Optionally display application logs.

Additional proposed commands:

```bash
cydctl devices
cydctl status
cydctl projects
cydctl versions gps-navigator
cydctl logs --follow
cydctl run gps-navigator
cydctl stop
cydctl rollback gps-navigator
cydctl backup gps-navigator
```

These commands are proposed functionality to implement, not existing commands.

The CLI should provide machine-readable JSON output as an option so Codex can inspect deployment results and errors.

### Desired Development Workflow

The user will often be sitting on a couch with an iPad remotely accessing a Linux desktop.

Codex runs on the Linux desktop.

The CYD sits nearby, powered through USB or a battery.

The intended workflow:

1. User instructs Codex to modify an application.
2. Codex edits the source files.
3. Codex invokes `cydctl deploy`.
4. The application uploads over Wi-Fi.
5. CYD OS saves the previous version.
6. CYD OS installs the new version.
7. CYD OS launches it automatically.
8. The user immediately tests the application on the touchscreen.
9. Codex retrieves logs if there is a problem.
10. The process repeats without physically connecting the ESP32 to the desktop.

**This workflow is the central purpose of the project.**

---

## 12. Hot Reloading

Support rapid application updates without resetting the microcontroller.

For Lua applications, the initial implementation may use an application-level restart rather than true live code replacement.

Expected behavior:

1. Receive an updated package.
2. Validate it.
3. Stage the package safely.
4. Request the running application to stop.
5. Release its resources.
6. Replace the active application package.
7. Create a fresh Lua execution context.
8. Start the updated application.
9. Report success or failure.

Preserve application data separately from executable code.

If startup fails, provide an option to restore the previous working version.

Do not attempt to hot-swap arbitrary compiled ESP32 binaries.

---

## 13. Firmware OTA and Recovery

CYD OS itself must be updateable over Wi-Fi.

Use ESP-IDF's supported OTA mechanisms.

Prefer an A/B partition layout with rollback support.

### OTA Requirements

- Validate application image compatibility.
- Verify integrity.
- Verify available partition capacity.
- Use supported ESP-IDF OTA APIs.
- Report update progress.
- Reboot into the updated OS.
- Confirm successful startup.
- Roll back when a new OS image fails validation.

### Recovery

Provide an independent recovery path.

Investigate the board's hardware BOOT and RESET controls and ESP-IDF's available bootloader recovery mechanisms.

A hardware recovery gesture should be available even when the normal user interface fails to start, if the selected board and bootloader design permit it.

Do not assume that detecting five BOOT-button presses in an application provides reliable recovery from a completely broken application.

Maintain a documented USB recovery method.

### Conventional Firmware Testing

Support for standalone firmware images is an optional later phase.

The user wants to experiment with firmware such as Bruce and other ESP32 projects.

Investigate a controlled method to install compatible application images to an OTA partition.

Do not blindly flash arbitrary `merged.bin` files over the CYD OS bootloader or partition table.

Third-party firmware may require different hardware, flash layouts, or bootloaders.

Preserving CYD OS while launching arbitrary existing firmware cannot be guaranteed.

The reliable Lua application workflow takes priority over this feature.

---

## 14. Filesystem and Storage

Use the microSD card for application packages, development versions, project assets, and exports.

Use internal flash/NVS for essential device configuration where appropriate.

Proposed layout:

```text
/cydos/
├── packages/
├── apps/
├── projects/
├── backups/
├── logs/
├── icons/
└── system/
    ├── settings.json
    └── registry.json
```

The filesystem should support:

- Atomic or recoverable updates where practical.
- Detection of incomplete deployments.
- Missing-card handling.
- Storage capacity monitoring.
- Version cleanup.
- Configuration validation.
- Corrupt-file recovery.
- Backup and restore.

Avoid writing excessively to internal flash.

Keep source-code repositories and Git history on the Linux desktop. The CYD's SD card should primarily hold deployable packages and testing versions.

---

## 15. Stability and Performance

The operating environment should remain responsive during:

- Wi-Fi transfers.
- SD card operations.
- Application launch.
- Application termination.
- Log streaming.
- Sensor polling.

Use FreeRTOS tasks and queues where appropriate.

On ESP32-S3, distribute suitable workloads across both cores, but avoid unnecessarily hard-coding every service to a particular core.

Only one foreground third-party application is required for the initial release.

Monitor:

- Free heap.
- Largest free allocation.
- PSRAM usage.
- Application memory consumption.
- Task stack usage.
- Watchdog resets.
- Application crashes.

Implement reasonable limits on script execution and resource allocation.

Ensure LVGL operations occur through the designated UI task or an appropriately synchronized interface.

---

## 16. Development Phases

Implement this project incrementally.

### Phase 1 — Hardware Foundation

- Create the ESP-IDF project.
- Confirm the exact board revision.
- Initialize display and touchscreen.
- Initialize SD card.
- Initialize Wi-Fi.
- Build a basic graphical home screen.
- Display diagnostics.

**Acceptance:** The CYD boots reliably to a responsive home screen.

### Phase 2 — Wireless File Manager

- Host the local web dashboard.
- Implement authenticated package uploads.
- Save files to SD.
- Download and delete files.
- Show storage usage.
- Provide a basic deployment API.

**Acceptance:** Files can be transferred between the Linux desktop and CYD without USB.

### Phase 3 — Lua Runtime

- Integrate Lua.
- Load a script from SD.
- Execute a simple application.
- Expose basic UI APIs.
- Stop and restart applications.
- Return to the home screen.

**Acceptance:** A Hello World application can be modified and relaunched without reflashing the device.

### Phase 4 — Codex Deployment

- Implement `cydctl`.
- Package applications automatically.
- Deploy over Wi-Fi.
- Run newly uploaded versions.
- Retrieve diagnostic logs.

**Acceptance:** Codex can edit, upload, launch, and troubleshoot a Lua application from the Linux desktop.

### Phase 5 — Project and Version Management

- Create project folders.
- Save multiple versions.
- Launch previous versions.
- Archive and delete versions.
- Export project packages.
- Promote applications to the home screen.

**Acceptance:** Multiple iterations can be managed without cluttering the home screen.

### Phase 6 — Hardware APIs

- Add UART API.
- Add I²C API.
- Add GPIO API.
- Add persistent app storage.
- Add hardware permissions.
- Add peripheral ownership rules.

**Acceptance:** A Lua application can communicate with supported external sensors without reflashing CYD OS.

### Phase 7 — OS OTA Updates

- Add OTA partition support.
- Update core firmware over Wi-Fi.
- Add update validation.
- Add rollback.
- Document emergency recovery.

**Acceptance:** CYD OS can update itself wirelessly and recover from a failed update when supported by the boot configuration.

### Phase 8 — Secondary Hardware Support

Port the system to the NM-CYD-C5.

Retain shared application APIs and deployment tooling.

Provide device-specific drivers and capability detection.

**Acceptance:** Compatible `.cydapp` packages can be deployed to both ESP32-S3 and ESP32-C5 versions of CYD OS.

---

## 17. Initial Deliverables

Begin with a realistic, working minimum viable product.

The first deliverables should be:

1. A compilable ESP-IDF project for the exact CrowPanel ESP32-S3 hardware.
2. An operational LVGL home screen.
3. Working touchscreen input.
4. Working SD card access.
5. Working Wi-Fi connectivity.
6. A minimal authenticated web interface for uploading files.
7. A simple Lua application that can be loaded from SD.
8. A functional way to stop the Lua application and return to the home screen.
9. Build, flashing, and debugging instructions for Linux.
10. A README explaining the architecture and current limitations.

Do not attempt to implement every advanced capability immediately.

Prioritize reliable hardware initialization, file uploads, and app execution.

---

## 18. Instructions to Codex

You are responsible for implementing this project as actual, compilable embedded software, not merely generating a conceptual design.

Before writing hardware-specific code:

- Identify the exact board model and revision.
- Locate and use the manufacturer's official schematics, pin definitions, and display examples.
- Verify library compatibility with the selected ESP-IDF version.
- Verify the touchscreen and display interfaces.
- Verify flash and PSRAM configuration.
- Identify any peripheral pin-sharing or hardware conflicts.

If the board model or a required hardware detail cannot be verified, document the uncertainty instead of inventing pin assignments.

Use established ESP-IDF components wherever practical.

Maintain clean module boundaries and readable source code.

Implement the work in phases and keep the project buildable after each phase.

Do not claim that hardware has been tested unless the actual device was accessed and tested.

Provide explicit instructions for anything requiring physical hardware testing.

The most important milestone is:

**I want to modify an app using Codex on my Linux desktop, wirelessly deploy it to my ESP32, and test it immediately without reconnecting USB, reflashing the entire OS, or resetting the board.**

Every major architectural decision should prioritize that development experience.