# NovelMind - Troubleshooting Guide

## Audio Issues (GStreamer Errors)

### Symptoms
```
qt.multimedia.audiooutput: Invalid audio device
qt.multimedia.audiooutput: Failed to create a gst element for the audio device
GStreamer-CRITICAL: gst_element_unlink: assertion 'GST_IS_ELEMENT (dest)' failed
```

### Cause
Qt Multimedia uses GStreamer on Linux/WSL. Missing or incorrectly configured GStreamer plugins.

### Solutions

#### Ubuntu/Debian/WSL:
```bash
sudo apt-get update
sudo apt-get install -y \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-pulseaudio \
    gstreamer1.0-alsa \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev
```

#### For WSL2 with WSLg (Windows 11):
PulseAudio is already configured through WSLg! You should see:
```
N: [pulseaudio] main.c: User-configured server at unix:/mnt/wslg/PulseServer
```
This is normal and means audio should work.

#### For older WSL or without WSLg:
```bash
# Install PulseAudio
sudo apt-get install pulseaudio

# Start PulseAudio
pulseaudio --start

# Set default audio output
export PULSE_SERVER=tcp:localhost
```

#### Verify GStreamer Installation:
```bash
gst-inspect-1.0 --version
gst-inspect-1.0 | grep audio
```

### Alternative: Use miniaudio Backend
If GStreamer issues persist, the audio system uses miniaudio as fallback (already compiled in).

---

## Fixed Issues

### ✓ Parse Error "Expected scene name"
**Fixed in:** v1.0.1
**Cause:** Duplicate scene declarations in `main.nms` and separate scene files
**Solution:** Removed scene definitions from `main.nms`, kept only in modular files

### ✓ Localization Panel Text Visibility
**Fixed in:** v1.0.1
**Cause:** Missing text color definition for table cells
**Solution:** Added `color: %1` to `QTableWidget#LocalizationTable::item` in stylesheet

---

## Tips

1. **Running Examples**:
   - Use File → Open Project
   - Navigate to `examples/sample_novel/`
   - Click Play button in toolbar

2. **Audio Testing**:
   - Check system audio works: `speaker-test -t wav`
   - Test with simple audio file first

3. **Performance**:
   - WSL2: Slower file I/O than native Windows
   - Use Release build for better performance
