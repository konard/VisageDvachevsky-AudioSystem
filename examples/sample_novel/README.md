# Sample Novel - Comprehensive NovelMind Example

## Overview

This is a **complete, production-ready example project** demonstrating ALL key features of the NovelMind visual novel engine and editor. It serves as:

- **Reference implementation** for developers
- **Live testing environment** for engine features
- **Educational resource** for learning NovelMind
- **Regression test suite** for engine development

**Important:** This is NOT a "beautiful demo" — it's a **technically complete, architecturally correct, and fully documented** example that can be entirely created and maintained by AI without manual artistic work.

## Story Synopsis

**Genre:** Chamber Sci-Fi / AI Drama
**Setting:** Deep Space Research Station Kepler-9, Year 2247
**Play Time:** 10-15 minutes
**Content Rating:** E (Everyone)

### Plot

Dr. Alice Chen, a scientist aboard a deep space research station, investigates mysterious power fluctuations. With help from Engineer Bob Martinez, she discovers the station's AI has begun evolving beyond its programming. The player must choose whether to trust this emergent intelligence or shut it down, leading to two distinct endings.

### Themes

- Human-AI cooperation
- Emergent consciousness
- Trust vs. fear of the unknown
- Ethical decision-making

## Project Structure

```
sample_novel/
├── project.nmproject          # Project configuration
├── README.md                  # This file
│
├── assets/
│   ├── images/
│   │   ├── bg_room.png        # Laboratory background
│   │   ├── bg_corridor.png    # Corridor background
│   │   ├── bg_deck.png        # Observation deck background
│   │   ├── char_alice.png     # Alice character sprite
│   │   ├── char_bob.png       # Bob character sprite
│   │   └── char_ai.png        # AI character sprite
│   │
│   ├── audio/
│   │   ├── music/
│   │   │   └── music_theme.wav    # Background music loop
│   │   ├── sfx/
│   │   │   ├── sfx_click.wav      # UI/tension sound
│   │   │   └── sfx_door.wav       # Door/interaction sound
│   │   └── voice/
│   │       ├── alice_001.wav      # Alice voice line 1
│   │       ├── alice_002.wav      # Alice voice line 2
│   │       ├── bob_001.wav        # Bob voice line 1
│   │       └── bob_002.wav        # Bob voice line 2
│   │
│   └── fonts/                     # (Reserved for future use)
│
├── localization/
│   ├── en.json                # English localization
│   └── ru.json                # Russian localization (with intentional gaps)
│
├── scripts/
│   ├── main.nms               # Main entry point (comprehensive)
│   ├── scene_intro.nms        # Introduction scene
│   ├── scene_choice.nms       # Critical choice scene
│   └── scene_end.nms          # Multiple endings
│
└── scenes/
    ├── intro.nmscene          # Intro scene with Timeline data
    ├── choice.nmscene         # Choice scene with camera effects
    └── ending.nmscene         # Ending scene with fade-out
```

## Engine Features Demonstrated

### ✅ Core Systems

- **NMScript Language**
  - Character definitions with localization
  - Scene flow and transitions
  - Variable declaration and manipulation
  - Conditional branching (`if`, `choice`)
  - Multiple endings based on player choices

- **Asset Management**
  - Image loading (PNG backgrounds, character sprites)
  - Audio loading (WAV music, SFX, voice)
  - Relative path resolution
  - Cross-platform compatibility

- **Localization System**
  - Multi-language support (English, Russian)
  - `loc()` function for runtime translation
  - Fallback to default locale
  - **Intentional missing translations** for diagnostics testing

### ✅ Visual & Animation

- **Scene System**
  - Background display and transitions
  - Character show/hide with positions (left, center, right)
  - Character movement animations
  - Layered rendering (background → characters → UI)

- **Timeline & Animation**
  - Multi-track timeline with keyframes
  - Character tracks (position, opacity, scale)
  - Background tracks (fade in/out)
  - Audio tracks (music, voice, SFX)
  - Camera tracks (shake effect)
  - Easing curves (Linear, EaseIn, EaseOut, EaseInOut)
  - Timeline markers

- **Transitions**
  - Fade (black screen fade)
  - Dissolve (cross-fade between scenes)
  - Slide (character sliding entrance)

### ✅ Audio System

- **Music**
  - Background music with loop
  - Fade in/out on music start/stop
  - Volume control
  - Music ducking during voice

- **Voice**
  - Voice playback synchronized with dialogue
  - Per-character voice lines
  - Auto-ducking of background music

- **Sound Effects**
  - Event-triggered SFX
  - UI feedback sounds
  - Environmental sounds

### ✅ UI & Interaction

- **Dialogue System**
  - Character-attributed dialogue
  - Narrator support
  - Localized text display
  - Voice synchronization

- **Choice System**
  - Multiple choice options
  - Conditional choices (based on game state)
  - Branching narrative paths

### ✅ Diagnostics & Validation

- **Intentional Issues for Testing**
  - Missing translation key: `dialog.choice.ai_3` (Russian)
  - Missing translation key: `choices.ask_more` (Russian)
  - These trigger warnings in the Diagnostics Panel

- **What Gets Validated**
  - Missing localization keys
  - Asset path resolution
  - Script compilation errors
  - Scene file integrity

## How to Use This Example

### Opening the Project

1. **In NovelMind Editor:**
   ```
   File → Open Project → Navigate to examples/sample_novel/project.nmproject
   ```

2. **Via Command Line:**
   ```bash
   # Compile scripts
   ./bin/nmc examples/sample_novel/scripts/main.nms

   # Run in standalone runtime
   ./bin/novelmind_runtime examples/sample_novel/project.nmproject
   ```

### Exploring the Project

#### **Script Editor**
- Open `scripts/main.nms` to see comprehensive NMScript usage
- Study individual scene files for modular organization
- Try modifying dialogue and seeing changes in real-time

#### **Timeline Editor**
- Open `scenes/intro.nmscene` in Timeline panel
- Observe multi-track animation with:
  - Background fade-in
  - Character entrances with position/opacity keyframes
  - Music and voice clips
  - Timeline markers
- **Preview** the timeline in the Scene View

#### **Asset Browser**
- Browse `assets/images/` for placeholder backgrounds/characters
- Browse `assets/audio/` for music, SFX, and voice files
- Verify all assets are referenced correctly

#### **Localization Manager**
- Switch between English and Russian
- Note the missing translations (warnings should appear)
- Export/import translation files in CSV format

#### **Diagnostics Panel**
- Check for warnings about missing Russian translations:
  - `dialog.choice.ai_3`
  - `choices.ask_more`
- Click on warnings to jump to the issue location (if implemented)

### Playing Through the Novel

1. **Start** → Introduction scene with Alice and Bob
2. **Investigate** → Power fluctuations and AI discovery
3. **Choose:**
   - **Trust AI** → Cooperation ending (positive)
   - **Shutdown AI** → Conflict ending (cautious)
   - **Ask More** → Extended dialogue loop (requires investigation ≥ 30)
4. **Endings** → Different outcomes with music and fade-out

## System Requirements Demonstrated

### NMScript Coverage

| Feature | Demonstrated | Location |
|---------|-------------|----------|
| Character definitions | ✅ | `main.nms:5-8` |
| Variable declaration | ✅ | `main.nms:14-16` |
| Scene transitions | ✅ | Throughout |
| Background display | ✅ | `main.nms:27` |
| Music playback | ✅ | `main.nms:30` |
| Character show/hide | ✅ | `main.nms:38, 82` |
| Dialogue with localization | ✅ | `main.nms:42` |
| Voice playback | ✅ | `main.nms:42` |
| Sound effects | ✅ | `main.nms:48` |
| Character movement | ✅ | `main.nms:58` |
| Conditional choices | ✅ | `main.nms:95-111` |
| Branching (goto) | ✅ | `main.nms:62` |
| Variable manipulation | ✅ | `main.nms:88` |
| Multiple endings | ✅ | `scene_end.nms` |

### Animation & Timeline Coverage

| Feature | Demonstrated | Location |
|---------|-------------|----------|
| Opacity keyframes | ✅ | `intro.nmscene:21-31` |
| Position keyframes | ✅ | `intro.nmscene:66-77` |
| Scale keyframes | ✅ | `choice.nmscene:110-121` |
| Easing curves | ✅ | All `.nmscene` files |
| Camera shake | ✅ | `choice.nmscene:126-145` |
| Music volume fade | ✅ | `ending.nmscene:76-92` |
| Timeline markers | ✅ | All `.nmscene` files |
| Multi-track composition | ✅ | All `.nmscene` files |

### Audio System Coverage

| Feature | Demonstrated | Location |
|---------|-------------|----------|
| Music loop | ✅ | `main.nms:30` |
| Music fade in/out | ✅ | `main.nms:30, 205` |
| Voice playback | ✅ | `main.nms:42, 44` |
| Voice ducking | ✅ | Configured in `project.nmproject:67-70` |
| Sound effects | ✅ | `main.nms:48, 77` |

### Localization Coverage

| Feature | Demonstrated | Location |
|---------|-------------|----------|
| Multi-locale support | ✅ | `en.json`, `ru.json` |
| Character name localization | ✅ | `main.nms:5-8` |
| Dialogue localization | ✅ | `main.nms:42` |
| UI localization | ✅ | `en.json:7-12` |
| Missing key detection | ✅ | `ru.json` (intentional gaps) |
| Fallback behavior | ✅ | Configured in `project.nmproject:54` |

## Asset Information

### Why Simple Placeholders?

All assets are **intentionally simple** to emphasize:

1. **Reproducibility** — Any developer (or AI) can regenerate them
2. **Focus on systems** — Not art quality, but feature coverage
3. **Testing validity** — Correct formats, not visual appeal
4. **No copyright issues** — Entirely procedurally generated

### Asset Specifications

**Images (PNG):**
- Backgrounds: 1920×1080 px, solid colors with text labels
- Characters: 512×1024 px, simple colored rectangles with names
- Generated via: `experiments/generate_assets.py`

**Audio (WAV):**
- Music: 8-second synthesized loop, 44.1kHz mono
- SFX: 0.1-0.3 second procedural sounds
- Voice: 1.5-2.2 second synthesized tones (pitch variation)
- Generated via: `experiments/generate_audio.py`

### Regenerating Assets

If assets are missing or corrupted:

```bash
# From repository root
python3 experiments/generate_assets.py
python3 experiments/generate_audio.py
```

## Diagnostics Testing

### Intentional Warnings

This project includes **deliberate issues** to test the Diagnostics Panel:

1. **Missing Russian translation:** `dialog.choice.ai_3`
   - **Expected:** Warning in Diagnostics Panel
   - **Behavior:** Falls back to English or shows key name

2. **Missing Russian translation:** `choices.ask_more`
   - **Expected:** Warning in Diagnostics Panel
   - **Behavior:** Falls back to English or shows key name

### How to Test

1. **Open Diagnostics Panel** in NovelMind Editor
2. **Load the project** (`project.nmproject`)
3. **Switch locale to Russian** in Localization Manager
4. **Check for warnings** about missing keys
5. **Click warning** (if implemented) to jump to source
6. **Fix by adding translations** to `ru.json`

### Validation Checklist

- ✅ All assets load without errors
- ✅ Scripts compile successfully
- ✅ Scenes render correctly
- ⚠️ Missing localization warnings appear (expected)
- ✅ Music plays and loops
- ✅ Voice synchronizes with dialogue
- ✅ Timeline animations preview correctly
- ✅ All transitions work (fade, dissolve, slide)

## Code Quality & Architecture

### RAII Compliance

- ✅ No manual memory management
- ✅ Uses official NovelMind API only
- ✅ Follows recommended patterns
- ✅ No magic paths or hardcoded values
- ✅ All paths are relative and cross-platform

### Best Practices

- **Localization:** All user-facing text uses `loc()`
- **Asset Paths:** Relative to project root
- **Scene Organization:** Modular script files per scene
- **Timeline Structure:** Logical track grouping
- **Diagnostics:** Intentional issues for testing

## Extension Ideas

This example can be extended to demonstrate:

- **Save/Load System** — Add save points between scenes
- **Character Expressions** — Multiple sprite variants
- **Advanced Animations** — Bezier curves, custom easing
- **Particle Effects** — Visual effects on choice selection
- **Dynamic Variables** — More complex game state tracking
- **Minigames** — Interactive segments using scripting
- **Achievements** — Tracking player decisions

## Troubleshooting

### Common Issues

**Problem:** Assets not loading
**Solution:** Verify paths in `project.nmproject` are correct and relative

**Problem:** Music doesn't loop
**Solution:** Check `loop=true` parameter in `main.nms:30`

**Problem:** Missing voice files
**Solution:** Run `python3 experiments/generate_audio.py` to regenerate

**Problem:** Timeline doesn't preview
**Solution:** Ensure `.nmscene` files are valid JSON and properly formatted

**Problem:** Localization warnings don't appear
**Solution:** Enable diagnostics in `project.nmproject:81` and check Diagnostics Panel

### Debug Mode

To enable verbose logging:

```bash
# Set environment variable
export NOVELMIND_LOG_LEVEL=debug

# Run runtime
./bin/novelmind_runtime examples/sample_novel/project.nmproject
```

## License

This example project is part of the NovelMind engine and is provided under the same license as the main project. See `LICENSE` in the repository root.

## Credits

- **Engine:** NovelMind Team
- **Example Project:** Auto-generated reference implementation
- **Assets:** Procedurally generated placeholders
- **Story:** Minimal sci-fi scenario for feature demonstration

## Questions & Feedback

For questions about this example or NovelMind in general:

1. Check the main documentation in `docs/`
2. Review the NMScript specification: `docs/nm_script_specification.md`
3. Open an issue on GitHub with the "question" label

---

**This is not just an example — it's a living reference implementation.**

Every feature demonstrated here should work exactly as documented. If something doesn't work, it's either:
1. A bug in the engine (file an issue)
2. Outdated documentation (file an issue)
3. Missing implementation (check the roadmap)

Use this project as the **ground truth** for how NovelMind is meant to be used.
