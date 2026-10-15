import storage
import board
import digitalio
import usb_midi
import usb_cdc

# Disable unused USB MIDI to free up endpoints for the WebSerial data port
usb_midi.disable()
usb_cdc.enable(console=True, data=True)

# Hold down the top-left key (KEY1) while plugging in the USB cable
# if you need the drive to appear as writable in Windows Explorer.
button = digitalio.DigitalInOut(board.KEY1)
button.switch_to_input(pull=digitalio.Pull.UP)
key_not_pressed = button.value
button.deinit()

if key_not_pressed:
    # Normal mode: Hide the CIRCUITPY popup from Windows so the pad manages its own files
    storage.disable_usb_drive()
    storage.remount("/", readonly=False)
