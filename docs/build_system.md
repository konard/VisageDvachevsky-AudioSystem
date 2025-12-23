# Build System Specification

> **See also**: [pack_file_format.md](pack_file_format.md) — Pack file binary format specification.
> **See also**: [pack_security.md](pack_security.md) — Security and encryption policies.
> **See also**: [architecture_overview.md](architecture_overview.md) — Overall system architecture.

This document describes the NovelMind build pipeline — an end-to-end system that compiles project files, processes assets, generates encrypted resource packs, and produces platform-specific game builds.

## Overview

The build system transforms a NovelMind project into a distributable game:

```
+-------------------+     +-------------------+     +-------------------+
|   Project Files   |---->|   Build System    |---->|   Game Build      |
|   (JSON/NMScript) |     |   (Pipeline)      |     |   (Executable +   |
|   + Assets        |     |                   |     |    .nmres packs)  |
+-------------------+     +-------------------+     +-------------------+
```

### Design Principles

- **Deterministic**: Same input always produces same output (byte-for-byte reproducible)
- **Stageable**: Pipeline runs in discrete stages with clear inputs/outputs
- **Cancellable**: Build can be cancelled at any stage checkpoint
- **Observable**: Real-time progress and diagnostics
- **Atomic**: No partial outputs — staging directory with final move/rename
- **Secure**: No hardcoded secrets; keys supplied externally

## Build Pipeline Stages

```
┌─────────────────────────────────────────────────────────────────────┐
│                          BUILD PIPELINE                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Stage 0: Preflight/Validation                                       │
│  ├── Validate project structure                                      │
│  ├── Check asset references                                          │
│  ├── Verify localization completeness                                │
│  └── Output: BuildReport (warnings/errors)                           │
│                           │                                          │
│                           ▼                                          │
│  Stage 1: Compile Scripts                                            │
│  ├── Parse NMScript files                                            │
│  ├── Generate bytecode/IR                                            │
│  ├── Produce script_map.json (source mapping)                        │
│  └── Output: compiled_scripts.bin + diagnostics                      │
│                           │                                          │
│                           ▼                                          │
│  Stage 2: Build Resource Index                                       │
│  ├── Collect all assets (images/audio/fonts/data)                    │
│  ├── Normalize to VFS paths                                          │
│  ├── Compute content hashes and sizes                                │
│  ├── Map locale bindings for language packs                          │
│  └── Output: resource_manifest.json                                  │
│                           │                                          │
│                           ▼                                          │
│  Stage 3: Pack Build (Multi-Pack VFS)                                │
│  ├── Generate Base.nmres (core game)                                 │
│  ├── Generate Lang_*.nmres (per-locale)                              │
│  ├── Generate Patch_*.nmres (if patch mode)                          │
│  ├── Generate DLC_*.nmres (if DLC content)                           │
│  ├── Apply encryption (release mode)                                 │
│  └── Output: .nmres files + packs_index.json                         │
│                           │                                          │
│                           ▼                                          │
│  Stage 4: Runtime Bundling                                           │
│  ├── Copy runtime executable                                         │
│  ├── Arrange platform-specific layout                                │
│  ├── Generate runtime_config.json                                    │
│  └── Output: Complete game folder                                    │
│                           │                                          │
│                           ▼                                          │
│  Stage 5: Post-build Verification                                    │
│  ├── Validate pack integrity                                         │
│  ├── Smoke test resource loading                                     │
│  ├── Verify compiled scripts load                                    │
│  └── Output: Final BuildResult                                       │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## Stage Details

### Stage 0: Preflight/Validation

**Purpose**: Ensure project consistency before starting resource-intensive operations.

**Checks performed**:
| Check | Description | Severity |
|-------|-------------|----------|
| Project structure | Valid project.json, required directories exist | Error |
| Asset references | All referenced assets exist in project | Error |
| Script syntax | NMScript files parse without errors | Error |
| Localization | All keys have translations for included locales | Warning |
| Voice mapping | Voice manifest references valid audio files | Warning |
| Circular refs | No circular scene/script dependencies | Error |
| Unreachable | Detect unreachable scenes/nodes | Warning |

**Output**: `BuildReport` with categorized warnings and errors.

**Decision**: Build is blocked if any errors are present. Warnings are logged but allow continuation.

### Stage 1: Compile Scripts

**Input**: `scripts/*.nms`, story graph definitions, project configuration

**Actions**:
1. Collect all NMScript source files
2. Parse and validate syntax
3. Perform semantic analysis
4. Generate bytecode/IR for the scripting VM
5. Produce source mapping for debugging

**Output**:
```
staging/
├── compiled_scripts.bin    # Compiled bytecode bundle
└── script_map.json         # Source location mapping
    {
      "entries": [
        {
          "bytecode_offset": 0,
          "source_file": "scripts/main.nms",
          "source_line": 1,
          "source_column": 0
        }
      ]
    }
```

### Stage 2: Build Resource Index

**Purpose**: Create a unified resource index for the VFS and ResourceManager.

**Actions**:
1. Scan all asset directories (images/, audio/, fonts/, data/)
2. Normalize paths to VFS-style paths (forward slashes, lowercase)
3. Calculate content hashes (SHA-256, first 16 bytes stored)
4. Compute compressed and uncompressed sizes
5. Determine dependencies (e.g., scenes referencing textures)
6. Map locale bindings for language-specific resources
7. Map voice bindings per locale and character

**Output**: `resource_manifest.json`
```json
{
  "version": "1.0",
  "generated": "2024-01-15T10:30:00Z",
  "resources": [
    {
      "id": "textures/bg_city",
      "type": "Texture",
      "source_path": "assets/backgrounds/city.png",
      "size": 245760,
      "hash": "a1b2c3d4e5f6...",
      "pack": "Base",
      "flags": ["PRELOAD"]
    },
    {
      "id": "audio/voice/en/narrator_001",
      "type": "Audio",
      "source_path": "assets/voice/en/narrator_001.ogg",
      "size": 102400,
      "hash": "f6e5d4c3b2a1...",
      "pack": "Lang_en",
      "locale": "en"
    }
  ],
  "locales": ["en", "ru", "ja"],
  "default_locale": "en"
}
```

### Stage 3: Pack Build (Multi-Pack VFS)

**Purpose**: Create `.nmres` pack files with proper priority and encryption.

#### Pack Types

| Pack Type | Priority | Description | Filename Pattern |
|-----------|----------|-------------|------------------|
| Base | 0 | Core game content | `Base.nmres` |
| Patch | 1 | Updates and fixes | `Patch_NNN.nmres` |
| DLC | 2 | Additional content | `DLC_<name>.nmres` |
| Language | 3 | Localization resources | `Lang_<locale>.nmres` |
| Mod | 4 | User-created content | `Mod_<name>.nmres` |

#### Priority Resolution

Resources are resolved by VFS path. When multiple packs contain the same resource:

1. Higher pack type priority wins (Mod > Language > DLC > Patch > Base)
2. Within same type, explicit priority value is used
3. Tie-breaker: pack version, then pack timestamp

**Example resolution**:
```
Request: "sprites/hero.png"

Available in:
  - Base.nmres (priority 0)
  - Patch_001.nmres (priority 1)
  - Mod_custom.nmres (priority 4)

Resolution: Mod_custom.nmres wins (highest type priority)
```

#### Encryption Policy

| Build Type | Encryption | Key Source |
|------------|------------|------------|
| Debug | None | N/A |
| Release | AES-256-GCM | Environment variable or key file |
| Distribution | AES-256-GCM + Signing | Secure key storage |

**Key management**:
- Keys are **never** stored in repository or build output
- Environment variables: `NOVELMIND_PACK_AES_KEY_HEX`, `NOVELMIND_PACK_AES_KEY_FILE`
- Runtime loads key from secure external source
- See [pack_security.md](pack_security.md) for details

**Output**:
```
staging/packs/
├── Base.nmres
├── Lang_en.nmres
├── Lang_ru.nmres
├── Lang_ja.nmres
├── Patch_001.nmres (if patch mode)
└── packs_index.json
```

**packs_index.json**:
```json
{
  "version": "1.0",
  "packs": [
    {
      "id": "base",
      "filename": "Base.nmres",
      "type": "Base",
      "priority": 0,
      "version": "1.0.0",
      "hash": "sha256:abc123...",
      "size": 52428800,
      "resource_count": 150,
      "encrypted": true,
      "signed": false
    },
    {
      "id": "lang_en",
      "filename": "Lang_en.nmres",
      "type": "Language",
      "priority": 3,
      "locale": "en",
      "version": "1.0.0",
      "hash": "sha256:def456...",
      "size": 10485760,
      "resource_count": 45,
      "encrypted": true,
      "signed": false
    }
  ],
  "load_order": ["base", "lang_en", "lang_ru", "lang_ja"]
}
```

### Stage 4: Runtime Bundling

**Purpose**: Assemble the final distributable game folder.

#### Platform-Specific Layouts

**Windows**:
```
Build/
└── MyGame/
    ├── MyGame.exe              # Runtime executable
    ├── packs/
    │   ├── Base.nmres
    │   ├── Lang_en.nmres
    │   └── packs_index.json
    ├── config/
    │   └── runtime_config.json
    ├── saves/                  # Empty, created by runtime
    └── logs/                   # Empty, created by runtime
```

**Linux**:
```
Build/
└── MyGame/
    ├── MyGame                  # Runtime executable (no extension)
    ├── lib/                    # Shared libraries if needed
    │   └── libSDL2.so
    ├── packs/
    │   ├── Base.nmres
    │   ├── Lang_en.nmres
    │   └── packs_index.json
    ├── config/
    │   └── runtime_config.json
    ├── saves/
    └── logs/
```

**macOS**:
```
Build/
└── MyGame.app/
    └── Contents/
        ├── Info.plist
        ├── MacOS/
        │   └── MyGame          # Runtime executable
        ├── Resources/
        │   ├── packs/
        │   │   ├── Base.nmres
        │   │   ├── Lang_en.nmres
        │   │   └── packs_index.json
        │   ├── config/
        │   │   └── runtime_config.json
        │   └── icon.icns
        └── Frameworks/         # Bundled frameworks
```

**runtime_config.json**:
```json
{
  "version": "1.0",
  "game": {
    "name": "My Visual Novel",
    "version": "1.0.0",
    "build_number": 42
  },
  "localization": {
    "default_locale": "en",
    "available_locales": ["en", "ru", "ja"]
  },
  "packs": {
    "directory": "packs",
    "index_file": "packs_index.json",
    "encrypted": true
  },
  "runtime": {
    "enable_logging": true,
    "log_level": "info",
    "enable_debug_console": false
  }
}
```

### Stage 5: Post-build Verification

**Purpose**: Ensure the build is valid and runnable.

**Checks**:
1. **Pack integrity**: All `.nmres` files are valid and readable
2. **Index validation**: `packs_index.json` references existing files
3. **Resource sampling**: Load a subset of resources through VFS
4. **Script loading**: Compiled scripts load without errors
5. **Config validation**: `runtime_config.json` is valid JSON

**Output**: Final `BuildResult` with success/failure status and statistics.

## Build Configuration

### BuildConfig Structure

```cpp
struct BuildConfig {
  // Paths
  std::string projectPath;      // Input project directory
  std::string outputPath;       // Output build directory
  std::string executableName;   // Game executable name
  std::string version;          // Build version string

  // Platform
  BuildPlatform platform;       // Windows, Linux, MacOS
  BuildType buildType;          // Debug, Release, Distribution

  // Asset settings
  bool packAssets;              // Create .nmres packs
  bool encryptAssets;           // Enable encryption
  std::string encryptionKey;    // Path to key file (not the key itself)
  CompressionLevel compression; // None, Fast, Balanced, Maximum

  // Features
  bool includeDebugConsole;     // Enable in-game debug console
  bool includeEditor;           // Include editor components
  bool enableLogging;           // Enable runtime logging

  // Localization
  std::vector<std::string> includedLanguages;  // Locales to include
  std::string defaultLanguage;                  // Default locale

  // Optimization
  bool stripUnusedAssets;       // Remove unreferenced assets
  bool generateSourceMap;       // Include debug source mapping

  // Signing (Distribution only)
  bool signExecutable;          // Sign the executable
  std::string signingCertificate;  // Path to certificate
};
```

### Build Profiles

Pre-configured profiles for common scenarios:

| Profile | Purpose | Settings |
|---------|---------|----------|
| Debug | Development/testing | No encryption, debug console, logging |
| Release | Production build | Encryption enabled, no debug console |
| Distribution | Store release | Encryption + signing, stripped |

## Progress Reporting

### BuildProgress Structure

```cpp
struct BuildProgress {
  // Overall
  f32 progress;           // 0.0 to 1.0
  std::string currentStep;
  std::string currentTask;

  // Step tracking
  std::vector<BuildStep> steps;
  i32 currentStepIndex;

  // Statistics
  i32 filesProcessed;
  i32 totalFiles;
  i64 bytesProcessed;
  i64 totalBytes;

  // Timing
  f64 elapsedMs;
  f64 estimatedRemainingMs;

  // Messages
  std::vector<std::string> infoMessages;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;

  // Status
  bool isRunning;
  bool isComplete;
  bool wasSuccessful;
  bool wasCancelled;
};
```

### Callbacks

```cpp
// Progress update (called frequently)
void onProgressUpdate(const BuildProgress &progress);

// Step completion (called per stage)
void onStepComplete(const BuildStep &step);

// Build completion (called once at end)
void onBuildComplete(const BuildResult &result);

// Log messages (for UI log panel)
void onLogMessage(const std::string &message, bool isError);
```

## Error Handling

### Error Categories

| Category | Example | Action |
|----------|---------|--------|
| Project Error | Missing project.json | Block build |
| Asset Error | Missing referenced file | Block build |
| Script Error | Syntax/semantic error | Block build |
| Localization Warning | Missing translation | Log warning, continue |
| Pack Error | Encryption failure | Block build |
| Platform Error | Missing runtime binary | Block build |

### Structured Errors

```cpp
struct BuildError {
  enum class Category {
    Project,
    Asset,
    Script,
    Localization,
    Pack,
    Platform,
    Internal
  };

  Category category;
  std::string code;         // e.g., "ASSET_NOT_FOUND"
  std::string message;      // Human-readable description
  std::string file;         // Related file path
  i32 line;                 // Line number (for scripts)
  i32 column;               // Column number (for scripts)
  std::string suggestion;   // Suggested fix
};
```

## UI Integration (BuildSettingsPanel)

### Required UI Elements

1. **Platform Selection**
   - Target platform dropdown (Windows, Linux, macOS)
   - Build profile dropdown (Debug, Release, Distribution)

2. **Output Configuration**
   - Output directory picker
   - Executable name input
   - Version string input

3. **Build Options**
   - Compress assets checkbox
   - Encrypt assets checkbox
   - Include debug console checkbox
   - Strip unused assets checkbox

4. **Localization**
   - Available languages list
   - Default language selector

5. **Progress Display**
   - Overall progress bar
   - Current stage indicator
   - Stage progress details
   - Elapsed/remaining time

6. **Diagnostics**
   - Warning/error list
   - Clickable navigation to source
   - Log output panel

7. **Actions**
   - Build button (starts build)
   - Cancel button (stops build)
   - Export report button (saves BuildResult as JSON)

## Thread Safety

### Build Execution Model

```
┌─────────────────┐      Events       ┌─────────────────┐
│    UI Thread    │◄────────────────  │  Build Thread   │
│  (Qt Main)      │                   │  (Worker)       │
├─────────────────┤                   ├─────────────────┤
│ - Update UI     │                   │ - Run pipeline  │
│ - Handle clicks │                   │ - Process files │
│ - Display prog. │                   │ - Report prog.  │
└─────────────────┘                   └─────────────────┘
```

- Build runs in dedicated worker thread
- Progress events queued to UI thread
- Atomic cancellation flag checked at stage boundaries
- No UI blocking during build

## File System Operations

### Atomic Output

1. All build output goes to staging directory: `<output>/.staging/`
2. On successful completion: atomic rename to final directory
3. On failure/cancel: staging directory is deleted
4. Ensures no partial builds are left behind

### Temporary Files

- Intermediate files in `<output>/.staging/temp/`
- Cleaned up automatically via RAII scope guards
- No manual cleanup required

## CLI Support

The build system can be invoked from command line for CI/CD:

```bash
# Build with default settings
nmc build --project ./MyProject --output ./Build

# Build with specific profile
nmc build --project ./MyProject --output ./Build --profile Release

# Build with custom options
nmc build --project ./MyProject --output ./Build \
  --platform linux \
  --encrypt \
  --languages en,ru,ja \
  --strip-unused
```

## CI/CD Integration

Example GitHub Actions workflow:

```yaml
name: Build Game
on: [push]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build NovelMind Project
        run: |
          nmc build \
            --project ./game \
            --output ./build \
            --profile Release \
            --platform linux

      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: game-linux
          path: ./build/
```

## Versioning

| Version | Changes |
|---------|---------|
| 1.0 | Initial build system specification |

Future versions will maintain backward compatibility where possible. The build system will refuse to process projects with incompatible versions.
