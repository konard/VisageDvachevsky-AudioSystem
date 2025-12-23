# Voice Pipeline Documentation

This document describes the voice authoring workflow and manifest format for NovelMind Editor.

## Overview

The Voice Pipeline system provides a comprehensive solution for managing voice-over content in visual novels. It replaces the unclear CSV format with a structured JSON manifest that:

- Defines clear field names (MUST/RECOMMENDED/OPTIONAL)
- Supports multiple locales
- Tracks recording status (missing, recorded, imported, needs review, approved)
- Manages multiple takes per voice line
- Integrates with the editor's recording studio

## Voice Manifest Format

### Basic Structure

```json
{
  "project": "my_visual_novel",
  "default_locale": "en",
  "locales": ["en", "ru", "ja"],
  "naming_convention": "{locale}/{id}.ogg",
  "base_path": "assets/audio/voice",
  "lines": [
    {
      "id": "intro.alex.001",
      "text_key": "dialog.intro.alex.001",
      "speaker": "alex",
      "scene": "intro",
      "tags": ["main", "calm"],
      "notes": "Speak softly, introspective mood",
      "files": {
        "en": "en/intro.alex.001.ogg",
        "ru": "ru/intro.alex.001.ogg"
      }
    }
  ]
}
```

### Field Definitions

#### Project-Level Fields

| Field | Required | Description |
|-------|----------|-------------|
| `project` | RECOMMENDED | Project name for identification |
| `default_locale` | MUST | Default locale for the project (e.g., "en") |
| `locales` | MUST | Array of supported locale codes |
| `naming_convention` | OPTIONAL | File naming template (see below) |
| `base_path` | OPTIONAL | Base directory for voice files (default: "assets/audio/voice") |
| `lines` | MUST | Array of voice line entries |

#### Voice Line Fields

| Field | Required | Description |
|-------|----------|-------------|
| `id` | MUST | Unique voice line identifier |
| `text_key` | MUST | Localization key for the dialogue text |
| `speaker` | RECOMMENDED | Character/speaker identifier |
| `scene` | RECOMMENDED | Scene identifier for organization |
| `tags` | OPTIONAL | Array of tags for filtering (e.g., "angry", "whisper") |
| `notes` | OPTIONAL | Notes for voice actors/directors |
| `duration_override` | OPTIONAL | Manual duration override in seconds |
| `source_script` | OPTIONAL | Source script file reference |
| `source_line` | OPTIONAL | Line number in source script |
| `files` | MUST | Object mapping locales to file paths |

### Naming Conventions

The `naming_convention` field supports these placeholders:

| Placeholder | Description | Example |
|-------------|-------------|---------|
| `{locale}` | Locale code | "en", "ru" |
| `{id}` | Voice line ID | "intro.alex.001" |
| `{scene}` | Scene name | "intro" |
| `{speaker}` | Speaker/character | "alex" |
| `{take}` | Take number | "1", "2" |

**Predefined Patterns:**

1. **Locale/ID Based** (default): `{locale}/{id}.ogg`
   - Result: `en/intro.alex.001.ogg`

2. **Scene/Speaker Based**: `{scene}/{speaker}/{id}_take{take}.ogg`
   - Result: `intro/alex/intro.alex.001_take1.ogg`

3. **Flat by ID**: `voice/{id}_{locale}.ogg`
   - Result: `voice/intro.alex.001_en.ogg`

## Voice Line Status

Each voice file can have one of these statuses:

| Status | Description |
|--------|-------------|
| `missing` | No audio file exists |
| `recorded` | Recorded directly in the editor |
| `imported` | Imported from external source |
| `needs_review` | Flagged for quality review |
| `approved` | Reviewed and approved for release |

## Take Management

The manifest supports multiple recording takes per voice line:

```json
{
  "files": {
    "en": {
      "locale": "en",
      "filePath": "en/intro.alex.001.ogg",
      "status": "recorded",
      "duration": 3.5,
      "takes": [
        {
          "takeNumber": 1,
          "filePath": "en/intro.alex.001_take1.ogg",
          "recordedTimestamp": 1703520000,
          "duration": 3.2,
          "isActive": false,
          "notes": "Too fast"
        },
        {
          "takeNumber": 2,
          "filePath": "en/intro.alex.001_take2.ogg",
          "recordedTimestamp": 1703520120,
          "duration": 3.5,
          "isActive": true,
          "notes": "Good pacing"
        }
      ],
      "activeTakeIndex": 1
    }
  }
}
```

## Recording Studio

The editor includes a built-in recording studio with:

### Features

1. **Device Selection**
   - Enumerate available input devices
   - Select preferred microphone
   - Configure sample rate (44.1kHz, 48kHz, 96kHz)

2. **Level Monitoring**
   - Real-time VU meter display
   - Peak and RMS level indicators
   - Clipping detection warning

3. **Recording Controls**
   - Start/Stop recording
   - Cancel recording (discard)
   - Auto-advance to next line

4. **Take Management**
   - Record multiple takes
   - Preview any take
   - Select active take
   - Add notes to takes

5. **Auto-Save**
   - Recordings automatically save to manifest
   - Status updates in real-time
   - Backup on each take

### Recording Format

Default recording settings:

| Setting | Default | Options |
|---------|---------|---------|
| Sample Rate | 48000 Hz | 44100, 48000, 96000 |
| Channels | 1 (Mono) | 1, 2 |
| Bits per Sample | 16 | 16, 24, 32 |
| Output Format | WAV | WAV, FLAC |

### Post-Processing (Optional)

| Feature | Description |
|---------|-------------|
| Auto Trim Silence | Remove silence from start/end |
| Silence Threshold | -40 dB default |
| Normalize | Normalize to target level |
| Normalize Target | -1 dB default |

## Workflow Examples

### 1. New Project Setup

```cpp
// Create manifest for new project
VoiceManifest manifest;
manifest.setProjectName("my_visual_novel");
manifest.setDefaultLocale("en");
manifest.addLocale("en");
manifest.addLocale("ru");

// Set naming convention
manifest.setNamingConvention(NamingConvention::localeIdBased());

// Generate lines from localization keys
// (Integration with LocalizationManager)
```

### 2. Import Existing CSV

```cpp
// Legacy CSV import
VoiceManifest manifest;
manifest.importFromCsv("old_voice_mapping.csv", "en");
manifest.saveToFile("voice_manifest.json");
```

### 3. Recording Session

```cpp
// Initialize recorder
AudioRecorder recorder;
recorder.initialize();

// Select device
auto devices = recorder.getInputDevices();
recorder.setInputDevice(devices[0].id);

// Set up level monitoring
recorder.setOnLevelUpdate([](const LevelMeter& level) {
    updateVUMeter(level.rmsLevelDb, level.clipping);
});

// Start recording
recorder.startRecording("output/intro.alex.001.ogg");

// ... user speaks ...

// Stop and save
recorder.stopRecording();

// Update manifest
manifest.markAsRecorded("intro.alex.001", "en", "en/intro.alex.001.ogg");
manifest.saveToFile("voice_manifest.json");
```

### 4. Export for Voice Studio

```cpp
// Export template for external recording
manifest.exportTemplate("recording_template.json");

// Export CSV for spreadsheet review
manifest.exportToCsv("voice_lines.csv", "en");
```

## API Reference

### VoiceManifest Class

```cpp
class VoiceManifest {
public:
    // Project configuration
    void setProjectName(const std::string& name);
    void setDefaultLocale(const std::string& locale);
    void addLocale(const std::string& locale);
    void setNamingConvention(const NamingConvention& convention);
    void setBasePath(const std::string& path);

    // Voice lines
    Result<void> addLine(const VoiceManifestLine& line);
    Result<void> updateLine(const VoiceManifestLine& line);
    void removeLine(const std::string& lineId);
    const VoiceManifestLine* getLine(const std::string& lineId) const;

    // Filtering
    std::vector<const VoiceManifestLine*> getLinesBySpeaker(const std::string& speaker) const;
    std::vector<const VoiceManifestLine*> getLinesByScene(const std::string& scene) const;
    std::vector<const VoiceManifestLine*> getLinesByStatus(VoiceLineStatus status) const;

    // Take management
    Result<void> addTake(const std::string& lineId, const std::string& locale,
                         const VoiceTake& take);
    Result<void> setActiveTake(const std::string& lineId, const std::string& locale,
                               u32 takeIndex);

    // Status management
    Result<void> setStatus(const std::string& lineId, const std::string& locale,
                           VoiceLineStatus status);
    Result<void> markAsRecorded(const std::string& lineId, const std::string& locale,
                                const std::string& filePath);

    // Validation
    std::vector<ManifestValidationError> validate(bool checkFiles = false) const;

    // Statistics
    CoverageStats getCoverageStats(const std::string& locale = "") const;

    // File I/O
    Result<void> loadFromFile(const std::string& filePath);
    Result<void> saveToFile(const std::string& filePath) const;
    Result<void> importFromCsv(const std::string& csvPath, const std::string& locale);
    Result<void> exportToCsv(const std::string& csvPath, const std::string& locale = "") const;

    // Generation
    u32 generateFilePaths(const std::string& locale, bool overwriteExisting = false);
    Result<void> createOutputDirectories(const std::string& locale) const;
};
```

### AudioRecorder Class

```cpp
class AudioRecorder {
public:
    // Initialization
    Result<void> initialize();
    void shutdown();

    // Device management
    std::vector<AudioDeviceInfo> getInputDevices() const;
    Result<void> setInputDevice(const std::string& deviceId);

    // Recording format
    void setRecordingFormat(const RecordingFormat& format);

    // Level monitoring
    Result<void> startMetering();
    void stopMetering();
    LevelMeter getCurrentLevel() const;

    // Recording
    Result<void> startRecording(const std::string& outputPath);
    Result<void> stopRecording();
    void cancelRecording();
    RecordingState getState() const;
    f32 getRecordingDuration() const;

    // Callbacks
    void setOnLevelUpdate(OnLevelUpdate callback);
    void setOnRecordingStateChanged(OnRecordingStateChanged callback);
    void setOnRecordingComplete(OnRecordingComplete callback);
    void setOnRecordingError(OnRecordingError callback);
};
```

## Migration from CSV

To migrate from the legacy CSV format:

1. Open the Voice Manager Panel
2. Click "Import" and select your CSV file
3. Review the imported lines in the list
4. Save as manifest using "Export" > "Save as Manifest"

The CSV import supports these column formats:

**Standard format:**
```csv
id,speaker,text_key,voice_file,scene,status
intro.alex.001,alex,dialog.intro.alex.001,en/intro.alex.001.ogg,intro,recorded
```

**Legacy format (auto-detected):**
```csv
DialogueID,ScriptPath,LineNumber,Speaker,DialogueText,VoiceFile,Actor,Status
```

## Best Practices

1. **Use meaningful IDs**: `scene.character.number` format
2. **Keep one manifest per project**: Avoid splitting voice data
3. **Commit manifest to version control**: Track voice progress
4. **Use tags for emotional variants**: "angry", "happy", "whisper"
5. **Add notes for actors**: Direction helps consistency
6. **Review before approval**: Use "needs_review" status
7. **Backup takes**: Keep all takes until final approval

## Troubleshooting

### Common Issues

1. **"Locale not in manifest locale list"**
   - Add the locale using `manifest.addLocale(locale)` before importing

2. **"Voice line ID already exists"**
   - Use `updateLine()` instead of `addLine()` for existing lines

3. **"File not found during validation"**
   - Ensure `base_path` is correct relative to project root

4. **Recording clips/distorts**
   - Lower input gain on microphone
   - Check for clipping indicator in VU meter
   - Record at lower volume and normalize later
