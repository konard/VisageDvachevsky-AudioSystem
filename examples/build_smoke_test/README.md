# Build Smoke Test Project

This is a minimal NovelMind project designed to test the build system pipeline.

## Purpose

This project serves as a regression test to ensure the build system can:

1. **Validate** the project structure
2. **Compile** NMScript files to bytecode
3. **Process** assets (images, audio, fonts)
4. **Generate** resource packs (.nmres files)
5. **Bundle** the runtime executable
6. **Verify** the final build

## Project Structure

```
build_smoke_test/
├── project.json         # Project configuration
├── scripts/
│   └── main.nms        # Main game script
├── assets/
│   ├── images/         # Image assets (backgrounds, sprites)
│   ├── audio/          # Audio assets (music, sfx, voice)
│   └── fonts/          # Font files
└── localization/
    └── en.json         # English localization strings
```

## Running the Test

### From the Editor

1. Open this project in the NovelMind Editor
2. Go to **Build > Build Settings**
3. Select your target platform
4. Click **Build Project**
5. Verify the build completes without errors

### From Command Line

```bash
# Build with default settings
nmc build --project ./examples/build_smoke_test --output ./build/smoke_test

# Build with specific profile
nmc build --project ./examples/build_smoke_test --output ./build/smoke_test --profile Debug
```

## Expected Output

A successful build should produce:

```
build/smoke_test/
├── MyGame.exe (or platform-appropriate executable)
├── packs/
│   ├── Base.nmres
│   └── packs_index.json
├── config/
│   └── runtime_config.json
├── saves/
└── logs/
```

## Verification Checklist

- [ ] Build completes without errors
- [ ] All packs are created and valid
- [ ] Runtime configuration is correct
- [ ] Executable launches (if runtime is available)
- [ ] Resources load correctly through VFS

## Troubleshooting

If the build fails:

1. Check the build log for specific error messages
2. Verify all required files exist (project.json, scripts/, assets/)
3. Ensure no syntax errors in .nms script files
4. Check for missing asset references
5. Verify localization files are valid JSON
