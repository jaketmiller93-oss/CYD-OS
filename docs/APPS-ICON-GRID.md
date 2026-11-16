# Apps icon grid — C5 0.4.4

The device Apps browser uses a responsive icon grid instead of full-width list
buttons. Folders have amber folder icons and app-count badges; app groups have
stable colored initial icons and version-count badges when applicable. Names
appear below each icon. Portrait C5 uses three columns; landscape uses four.
The grid scrolls vertically and recalculates when display orientation changes.

Tap a folder to open it; tap an app to select a saved revision and run it. The
folder toolbar gear opens Rename/Delete. The root plus creates a folder. Moving
an app uses the same folder grid. Catalog format, revision handling, existing
folders/apps, and Safari dashboard are unchanged.

Validation: 135 host regressions passed. Actual LVGL with ASan/UBSan covers icon
hit testing, folder create/rename/delete, moves, saved-version launch, empty
folders, stale refreshes, and live resizing at 240×320, 320×240, 800×480 and
480×800. Preview images contain sample catalog entries, not bundled apps.
Credential-free C5 OTA 0.4.4 built with ESP-IDF 5.5.4. No physical installation
or display/touch acceptance has been performed. S3 bring-up remains deferred.

Includes 0.4.1 iPad file-picker, 0.4.2 app-memory and 0.4.3 Lua ABI corrections.
User confirmed Calculator launches successfully with 0.4.3 before this change.
