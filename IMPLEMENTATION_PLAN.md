# Localization Advanced Features Implementation Plan

## Issue #15 Requirements
1. ✅ Plural forms editing in UI
2. ✅ PO/XLIFF import (already implemented in backend)
3. ✅ PO/XLIFF export (PO exists, need XLIFF)
4. ✅ Export missing strings (already implemented in backend)
5. ✅ Preview with variables
6. ✅ RTL preview mode
7. ✅ Async operations for large tables
8. ✅ Code quality/RAII
9. ✅ Stable serialization and clear errors

## Implementation Details

### 1. Plural Forms Editing
- Add UI for editing plural forms (Zero, One, Two, Few, Many, Other)
- Add dialog for plural form editing
- Update table to show plural indicator
- Connect to LocalizedString forms map

### 2. Export Missing Strings
- Add "Export Missing..." button
- Use exportMissingStrings from LocalizationManager
- Support CSV, JSON, PO formats

### 3. Preview with Variables
- Add preview panel/section
- Text input for variable values
- Real-time interpolation display

### 4. RTL Preview Mode
- Add RTL toggle button
- Switch table text direction
- Use isRightToLeft from LocalizationManager

### 5. XLIFF Export
- Implement exportXLIFF in LocalizationManager
- Match existing exportPO pattern

### 6. Async Operations
- Use QFutureWatcher for import/export
- Show progress dialog
- Don't block UI on large files

## Files to Modify
1. `editor/include/NovelMind/editor/qt/panels/nm_localization_panel.hpp` - Add new methods/members
2. `editor/src/qt/panels/nm_localization_panel.cpp` - Implement features
3. `engine_core/include/NovelMind/localization/localization_manager.hpp` - Add XLIFF export declaration
4. `engine_core/src/localization/localization_manager.cpp` - Implement XLIFF export
