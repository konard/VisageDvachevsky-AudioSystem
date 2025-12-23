/**
 * @file build_system.cpp
 * @brief Build System implementation for NovelMind
 *
 * Implements the complete build pipeline:
 * - Stage 0: Preflight/Validation
 * - Stage 1: Script Compilation
 * - Stage 2: Resource Index Generation
 * - Stage 3: Pack Building (Multi-Pack VFS)
 * - Stage 4: Runtime Bundling
 * - Stage 5: Post-build Verification
 */

#include "NovelMind/editor/build_system.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// BuildUtils Implementation
// ============================================================================

namespace BuildUtils {

std::string getPlatformName(BuildPlatform platform) {
  switch (platform) {
  case BuildPlatform::Windows:
    return "Windows";
  case BuildPlatform::Linux:
    return "Linux";
  case BuildPlatform::MacOS:
    return "macOS";
  case BuildPlatform::All:
    return "All Platforms";
  }
  return "Unknown";
}

std::string getExecutableExtension(BuildPlatform platform) {
  switch (platform) {
  case BuildPlatform::Windows:
    return ".exe";
  case BuildPlatform::Linux:
  case BuildPlatform::MacOS:
    return "";
  case BuildPlatform::All:
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
  }
  return "";
}

BuildPlatform getCurrentPlatform() {
#ifdef _WIN32
  return BuildPlatform::Windows;
#elif defined(__APPLE__)
  return BuildPlatform::MacOS;
#else
  return BuildPlatform::Linux;
#endif
}

std::string formatFileSize(i64 bytes) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  i32 unitIndex = 0;
  f64 size = static_cast<f64>(bytes);

  while (size >= 1024.0 && unitIndex < 4) {
    size /= 1024.0;
    unitIndex++;
  }

  std::ostringstream oss;
  if (unitIndex == 0) {
    oss << bytes << " " << units[unitIndex];
  } else {
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
  }
  return oss.str();
}

std::string formatDuration(f64 milliseconds) {
  if (milliseconds < 1000) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << milliseconds << " ms";
    return oss.str();
  }

  f64 seconds = milliseconds / 1000.0;
  if (seconds < 60) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << seconds << " s";
    return oss.str();
  }

  i32 minutes = static_cast<i32>(seconds) / 60;
  i32 secs = static_cast<i32>(seconds) % 60;
  std::ostringstream oss;
  oss << minutes << " min " << secs << " s";
  return oss.str();
}

i64 calculateDirectorySize(const std::string &path) {
  i64 totalSize = 0;
  try {
    for (const auto &entry : fs::recursive_directory_iterator(path)) {
      if (entry.is_regular_file()) {
        totalSize += static_cast<i64>(entry.file_size());
      }
    }
  } catch (const fs::filesystem_error &) {
    // Directory doesn't exist or permission denied
  }
  return totalSize;
}

Result<void> copyDirectory(const std::string &source,
                           const std::string &destination) {
  try {
    fs::copy(source, destination,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    return Result<void>::ok();
  } catch (const fs::filesystem_error &e) {
    return Result<void>::error(std::string("Failed to copy directory: ") +
                               e.what());
  }
}

Result<void> deleteDirectory(const std::string &path) {
  try {
    if (fs::exists(path)) {
      fs::remove_all(path);
    }
    return Result<void>::ok();
  } catch (const fs::filesystem_error &e) {
    return Result<void>::error(std::string("Failed to delete directory: ") +
                               e.what());
  }
}

Result<void> createDirectories(const std::string &path) {
  try {
    fs::create_directories(path);
    return Result<void>::ok();
  } catch (const fs::filesystem_error &e) {
    return Result<void>::error(std::string("Failed to create directories: ") +
                               e.what());
  }
}

} // namespace BuildUtils

// ============================================================================
// BuildSystem Implementation
// ============================================================================

BuildSystem::BuildSystem() = default;

BuildSystem::~BuildSystem() {
  cancelBuild();
  if (m_buildThread && m_buildThread->joinable()) {
    m_buildThread->join();
  }
}

Result<void> BuildSystem::startBuild(const BuildConfig &config) {
  if (m_buildInProgress) {
    return Result<void>::error("Build already in progress");
  }

  // Validate configuration
  if (config.projectPath.empty()) {
    return Result<void>::error("Project path is required");
  }
  if (config.outputPath.empty()) {
    return Result<void>::error("Output path is required");
  }

  // Check project exists
  if (!fs::exists(config.projectPath)) {
    return Result<void>::error("Project path does not exist: " +
                               config.projectPath);
  }

  m_config = config;
  m_buildInProgress = true;
  m_cancelRequested = false;

  // Reset progress
  m_progress = BuildProgress{};
  m_progress.isRunning = true;

  // Initialize build steps
  m_progress.steps = {
      {"Preflight", "Validating project structure", 0.05f, false, true, "", 0.0},
      {"Compile", "Compiling scripts", 0.15f, false, true, "", 0.0},
      {"Index", "Building resource index", 0.10f, false, true, "", 0.0},
      {"Pack", "Creating resource packs", 0.35f, false, true, "", 0.0},
      {"Bundle", "Bundling runtime", 0.25f, false, true, "", 0.0},
      {"Verify", "Verifying build", 0.10f, false, true, "", 0.0}};

  // Start build thread
  m_buildThread = std::make_unique<std::thread>([this]() { runBuildPipeline(); });

  return Result<void>::ok();
}

void BuildSystem::cancelBuild() {
  if (m_buildInProgress) {
    m_cancelRequested = true;
    logMessage("Build cancellation requested...", false);
  }
}

Result<std::vector<std::string>>
BuildSystem::validateProject(const std::string &projectPath) {
  std::vector<std::string> errors;

  // Check project directory exists
  if (!fs::exists(projectPath)) {
    errors.push_back("Project directory does not exist: " + projectPath);
    return Result<std::vector<std::string>>::ok(errors);
  }

  // Check for project.json
  fs::path projectFile = fs::path(projectPath) / "project.json";
  if (!fs::exists(projectFile)) {
    errors.push_back("Missing project.json in project directory");
  }

  // Check for required directories
  std::vector<std::string> requiredDirs = {"scripts", "assets"};
  for (const auto &dir : requiredDirs) {
    fs::path dirPath = fs::path(projectPath) / dir;
    if (!fs::exists(dirPath)) {
      errors.push_back("Missing required directory: " + dir);
    }
  }

  return Result<std::vector<std::string>>::ok(errors);
}

f64 BuildSystem::estimateBuildTime(const BuildConfig &config) const {
  // Simple estimation based on project size
  i64 projectSize = BuildUtils::calculateDirectorySize(config.projectPath);

  // Base time: 5 seconds
  f64 estimatedMs = 5000.0;

  // Add time based on size (roughly 1 second per MB)
  estimatedMs += static_cast<f64>(projectSize) / (1024.0 * 1024.0) * 1000.0;

  // Adjust for compression level
  switch (config.compression) {
  case CompressionLevel::None:
    break;
  case CompressionLevel::Fast:
    estimatedMs *= 1.2;
    break;
  case CompressionLevel::Balanced:
    estimatedMs *= 1.5;
    break;
  case CompressionLevel::Maximum:
    estimatedMs *= 2.0;
    break;
  }

  // Adjust for encryption
  if (config.encryptAssets) {
    estimatedMs *= 1.3;
  }

  return estimatedMs;
}

void BuildSystem::setOnProgressUpdate(
    std::function<void(const BuildProgress &)> callback) {
  m_onProgressUpdate = std::move(callback);
}

void BuildSystem::setOnStepComplete(
    std::function<void(const BuildStep &)> callback) {
  m_onStepComplete = std::move(callback);
}

void BuildSystem::setOnBuildComplete(
    std::function<void(const BuildResult &)> callback) {
  m_onBuildComplete = std::move(callback);
}

void BuildSystem::setOnLogMessage(
    std::function<void(const std::string &, bool)> callback) {
  m_onLogMessage = std::move(callback);
}

// ============================================================================
// Private Implementation
// ============================================================================

void BuildSystem::runBuildPipeline() {
  auto startTime = std::chrono::steady_clock::now();
  bool success = true;
  std::string errorMessage;

  // Create staging directory
  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";

  try {
    // Clean and create staging directory
    if (fs::exists(stagingDir)) {
      fs::remove_all(stagingDir);
    }
    fs::create_directories(stagingDir);

    // Stage 0: Preflight
    if (!m_cancelRequested) {
      auto result = prepareOutputDirectory();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 1: Compile Scripts
    if (success && !m_cancelRequested) {
      auto result = compileScripts();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 2: Build Resource Index (part of processAssets)
    if (success && !m_cancelRequested) {
      auto result = processAssets();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 3: Pack Resources
    if (success && !m_cancelRequested) {
      auto result = packResources();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 4: Generate Executable
    if (success && !m_cancelRequested) {
      auto result = generateExecutable();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 5: Sign and Finalize
    if (success && !m_cancelRequested) {
      auto result = signAndFinalize();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Atomic move from staging to final output
    if (success && !m_cancelRequested) {
      fs::path finalOutput = m_config.outputPath;

      // Ensure output parent directory exists
      if (finalOutput.has_parent_path()) {
        fs::create_directories(finalOutput.parent_path());
      }

      // Remove existing output if present
      if (fs::exists(finalOutput) && finalOutput != stagingDir) {
        fs::remove_all(finalOutput);
      }

      // Move staging contents to final location
      for (const auto &entry : fs::directory_iterator(stagingDir)) {
        fs::path dest = finalOutput / entry.path().filename();
        if (fs::exists(dest)) {
          fs::remove_all(dest);
        }
        fs::rename(entry.path(), dest);
      }

      // Remove staging directory
      fs::remove_all(stagingDir);
    }

  } catch (const std::exception &e) {
    success = false;
    errorMessage = std::string("Build exception: ") + e.what();
  }

  // Cleanup on failure
  auto cleanupResult = cleanup();
  if (cleanupResult.isError()) {
    logMessage("Cleanup warning: " + cleanupResult.error(), true);
  }

  // Calculate elapsed time
  auto endTime = std::chrono::steady_clock::now();
  f64 elapsedMs = std::chrono::duration<f64, std::milli>(endTime - startTime).count();

  // Prepare result
  m_lastResult = BuildResult{};
  m_lastResult.success = success && !m_cancelRequested;
  m_lastResult.outputPath = m_config.outputPath;
  m_lastResult.errorMessage = m_cancelRequested ? "Build cancelled" : errorMessage;
  m_lastResult.buildTimeMs = elapsedMs;
  m_lastResult.scriptsCompiled = static_cast<i32>(m_scriptFiles.size());
  m_lastResult.assetsProcessed = static_cast<i32>(m_assetFiles.size());
  m_lastResult.warnings = std::vector<std::string>(m_progress.warnings.begin(),
                                                    m_progress.warnings.end());

  // Calculate output size
  if (success && fs::exists(m_config.outputPath)) {
    m_lastResult.totalSize = BuildUtils::calculateDirectorySize(m_config.outputPath);
  }

  // Update progress
  m_progress.isRunning = false;
  m_progress.isComplete = true;
  m_progress.wasSuccessful = success && !m_cancelRequested;
  m_progress.wasCancelled = m_cancelRequested.load();
  m_progress.elapsedMs = elapsedMs;

  // Notify completion
  if (m_onBuildComplete) {
    m_onBuildComplete(m_lastResult);
  }

  m_buildInProgress = false;
}

Result<void> BuildSystem::prepareOutputDirectory() {
  beginStep("Preflight", "Validating project and preparing output");

  // Validate project
  updateProgress(0.1f, "Validating project structure...");
  auto validationResult = validateProject(m_config.projectPath);
  if (validationResult.isError()) {
    endStep(false, validationResult.error());
    return Result<void>::error(validationResult.error());
  }

  auto errors = validationResult.value();
  if (!errors.empty()) {
    std::string errorMsg;
    for (const auto &err : errors) {
      errorMsg += err + "\n";
      m_progress.errors.push_back(err);
    }
    endStep(false, "Project validation failed");
    return Result<void>::error("Project validation failed:\n" + errorMsg);
  }

  // Create staging directory structure
  updateProgress(0.5f, "Creating output directories...");
  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";

  try {
    fs::create_directories(stagingDir / "packs");
    fs::create_directories(stagingDir / "config");
    fs::create_directories(stagingDir / "logs");
    fs::create_directories(stagingDir / "saves");
  } catch (const fs::filesystem_error &e) {
    endStep(false, e.what());
    return Result<void>::error(std::string("Failed to create directories: ") +
                               e.what());
  }

  // Collect script files
  updateProgress(0.7f, "Scanning script files...");
  m_scriptFiles.clear();
  fs::path scriptsDir = fs::path(m_config.projectPath) / "scripts";
  if (fs::exists(scriptsDir)) {
    for (const auto &entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nmscript") {
          m_scriptFiles.push_back(entry.path().string());
        }
      }
    }
  }

  // Collect asset files
  updateProgress(0.9f, "Scanning asset files...");
  m_assetFiles.clear();
  fs::path assetsDir = fs::path(m_config.projectPath) / "assets";
  if (fs::exists(assetsDir)) {
    for (const auto &entry : fs::recursive_directory_iterator(assetsDir)) {
      if (entry.is_regular_file()) {
        m_assetFiles.push_back(entry.path().string());
      }
    }
  }

  logMessage("Found " + std::to_string(m_scriptFiles.size()) +
                 " script files and " + std::to_string(m_assetFiles.size()) +
                 " asset files",
             false);

  m_progress.totalFiles =
      static_cast<i32>(m_scriptFiles.size() + m_assetFiles.size());

  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::compileScripts() {
  beginStep("Compile", "Compiling NMScript files");

  if (m_scriptFiles.empty()) {
    logMessage("No script files to compile", false);
    endStep(true);
    return Result<void>::ok();
  }

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";
  fs::path compiledDir = stagingDir / "compiled";
  fs::create_directories(compiledDir);

  i32 compiled = 0;
  std::vector<ScriptCompileResult> results;

  for (const auto &scriptPath : m_scriptFiles) {
    if (m_cancelRequested) {
      endStep(false, "Cancelled");
      return Result<void>::error("Build cancelled");
    }

    f32 progress = static_cast<f32>(compiled) /
                   static_cast<f32>(m_scriptFiles.size());
    updateProgress(progress, "Compiling: " + fs::path(scriptPath).filename().string());

    auto result = compileScript(scriptPath);
    results.push_back(result);

    if (!result.success) {
      for (const auto &err : result.errors) {
        m_progress.errors.push_back(scriptPath + ": " + err);
      }
    }

    for (const auto &warn : result.warnings) {
      m_progress.warnings.push_back(scriptPath + ": " + warn);
    }

    compiled++;
    m_progress.filesProcessed++;
  }

  // Check for compilation errors
  bool hasErrors = false;
  for (const auto &result : results) {
    if (!result.success) {
      hasErrors = true;
      break;
    }
  }

  if (hasErrors) {
    endStep(false, "Script compilation failed");
    return Result<void>::error("One or more scripts failed to compile");
  }

  // Generate compiled bytecode bundle
  auto bundleResult = compileBytecode((compiledDir / "compiled_scripts.bin").string());
  if (bundleResult.isError()) {
    endStep(false, bundleResult.error());
    return bundleResult;
  }

  logMessage("Compiled " + std::to_string(compiled) + " scripts successfully", false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::processAssets() {
  beginStep("Index", "Processing and indexing assets");

  if (m_assetFiles.empty()) {
    logMessage("No assets to process", false);
    endStep(true);
    return Result<void>::ok();
  }

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";
  fs::path assetsDir = stagingDir / "assets";
  fs::create_directories(assetsDir);

  i32 processed = 0;
  m_assetMapping.clear();

  for (const auto &assetPath : m_assetFiles) {
    if (m_cancelRequested) {
      endStep(false, "Cancelled");
      return Result<void>::error("Build cancelled");
    }

    f32 progress = static_cast<f32>(processed) /
                   static_cast<f32>(m_assetFiles.size());
    updateProgress(progress, "Processing: " + fs::path(assetPath).filename().string());

    // Determine asset type and process accordingly
    std::string ext = fs::path(assetPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    fs::path relativePath =
        fs::relative(assetPath, fs::path(m_config.projectPath) / "assets");
    fs::path outputPath = assetsDir / relativePath;

    // Create output directory
    fs::create_directories(outputPath.parent_path());

    AssetProcessResult result;

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
      result = processImage(assetPath, outputPath.string());
    } else if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
      result = processAudio(assetPath, outputPath.string());
    } else if (ext == ".ttf" || ext == ".otf") {
      result = processFont(assetPath, outputPath.string());
    } else {
      result = processData(assetPath, outputPath.string());
    }

    if (!result.success) {
      m_progress.warnings.push_back("Asset processing warning: " +
                                    assetPath + " - " + result.errorMessage);
    }

    // Map original path to VFS path
    std::string vfsPath = relativePath.string();
    std::replace(vfsPath.begin(), vfsPath.end(), '\\', '/');
    m_assetMapping[assetPath] = vfsPath;

    processed++;
    m_progress.filesProcessed++;
    m_progress.bytesProcessed += result.processedSize;
  }

  // Generate resource manifest
  fs::path manifestPath = stagingDir / "resource_manifest.json";
  std::ofstream manifestFile(manifestPath);
  if (manifestFile.is_open()) {
    manifestFile << "{\n";
    manifestFile << "  \"version\": \"1.0\",\n";
    manifestFile << "  \"resource_count\": " << m_assetMapping.size() << ",\n";
    manifestFile << "  \"resources\": [\n";

    bool first = true;
    for (const auto &[sourcePath, vfsPath] : m_assetMapping) {
      if (!first)
        manifestFile << ",\n";
      first = false;
      manifestFile << "    {\"source\": \"" << sourcePath
                   << "\", \"vfs_path\": \"" << vfsPath << "\"}";
    }

    manifestFile << "\n  ]\n";
    manifestFile << "}\n";
    manifestFile.close();
  }

  logMessage("Processed " + std::to_string(processed) + " assets", false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::packResources() {
  beginStep("Pack", "Creating resource packs");

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";
  fs::path packsDir = stagingDir / "packs";
  fs::create_directories(packsDir);

  if (!m_config.packAssets) {
    // Just copy assets directly without packing
    logMessage("Skipping pack creation (packAssets=false)", false);
    endStep(true);
    return Result<void>::ok();
  }

  updateProgress(0.1f, "Building Base pack...");

  // Build base pack
  std::vector<std::string> baseFiles;
  for (const auto &[sourcePath, vfsPath] : m_assetMapping) {
    // Check if this is a locale-specific file
    bool isLocaleSpecific = false;
    for (const auto &lang : m_config.includedLanguages) {
      if (vfsPath.find("/" + lang + "/") != std::string::npos ||
          vfsPath.find(lang + "/") == 0) {
        isLocaleSpecific = true;
        break;
      }
    }

    if (!isLocaleSpecific) {
      fs::path processedPath =
          stagingDir / "assets" /
          fs::relative(sourcePath, fs::path(m_config.projectPath) / "assets");
      if (fs::exists(processedPath)) {
        baseFiles.push_back(processedPath.string());
      }
    }
  }

  auto baseResult =
      buildPack((packsDir / "Base.nmres").string(), baseFiles,
                m_config.encryptAssets,
                m_config.compression != CompressionLevel::None);
  if (baseResult.isError()) {
    endStep(false, baseResult.error());
    return baseResult;
  }

  // Build language packs
  i32 langPacksBuilt = 0;
  for (const auto &lang : m_config.includedLanguages) {
    if (m_cancelRequested) {
      endStep(false, "Cancelled");
      return Result<void>::error("Build cancelled");
    }

    f32 progress = 0.3f + 0.6f * static_cast<f32>(langPacksBuilt) /
                              static_cast<f32>(m_config.includedLanguages.size());
    updateProgress(progress, "Building language pack: " + lang);

    std::vector<std::string> langFiles;
    for (const auto &[sourcePath, vfsPath] : m_assetMapping) {
      if (vfsPath.find("/" + lang + "/") != std::string::npos ||
          vfsPath.find(lang + "/") == 0) {
        fs::path processedPath =
            stagingDir / "assets" /
            fs::relative(sourcePath, fs::path(m_config.projectPath) / "assets");
        if (fs::exists(processedPath)) {
          langFiles.push_back(processedPath.string());
        }
      }
    }

    if (!langFiles.empty()) {
      std::string packName = "Lang_" + lang + ".nmres";
      auto langResult =
          buildPack((packsDir / packName).string(), langFiles,
                    m_config.encryptAssets,
                    m_config.compression != CompressionLevel::None);
      if (langResult.isError()) {
        m_progress.warnings.push_back("Failed to create language pack for " +
                                      lang + ": " + langResult.error());
      } else {
        langPacksBuilt++;
      }
    }
  }

  // Generate packs_index.json
  updateProgress(0.95f, "Generating pack index...");

  fs::path indexPath = packsDir / "packs_index.json";
  std::ofstream indexFile(indexPath);
  if (indexFile.is_open()) {
    indexFile << "{\n";
    indexFile << "  \"version\": \"1.0\",\n";
    indexFile << "  \"packs\": [\n";

    indexFile << "    {\n";
    indexFile << "      \"id\": \"base\",\n";
    indexFile << "      \"filename\": \"Base.nmres\",\n";
    indexFile << "      \"type\": \"Base\",\n";
    indexFile << "      \"priority\": 0,\n";
    indexFile << "      \"encrypted\": " << (m_config.encryptAssets ? "true" : "false") << "\n";
    indexFile << "    }";

    for (const auto &lang : m_config.includedLanguages) {
      std::string packName = "Lang_" + lang + ".nmres";
      if (fs::exists(packsDir / packName)) {
        indexFile << ",\n";
        indexFile << "    {\n";
        indexFile << "      \"id\": \"lang_" << lang << "\",\n";
        indexFile << "      \"filename\": \"" << packName << "\",\n";
        indexFile << "      \"type\": \"Language\",\n";
        indexFile << "      \"priority\": 3,\n";
        indexFile << "      \"locale\": \"" << lang << "\",\n";
        indexFile << "      \"encrypted\": " << (m_config.encryptAssets ? "true" : "false") << "\n";
        indexFile << "    }";
      }
    }

    indexFile << "\n  ],\n";
    indexFile << "  \"default_locale\": \"" << m_config.defaultLanguage << "\"\n";
    indexFile << "}\n";
    indexFile.close();
  }

  logMessage("Created Base pack and " + std::to_string(langPacksBuilt) +
                 " language packs",
             false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::generateExecutable() {
  beginStep("Bundle", "Creating runtime bundle");

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";

  updateProgress(0.2f, "Preparing runtime executable...");

  // Determine executable name
  std::string exeName = m_config.executableName.empty()
                            ? "NovelMindRuntime"
                            : m_config.executableName;
  exeName += BuildUtils::getExecutableExtension(m_config.platform);

  // Platform-specific bundling
  Result<void> result;
  switch (m_config.platform) {
  case BuildPlatform::Windows:
    result = buildWindowsExecutable(stagingDir.string());
    break;
  case BuildPlatform::Linux:
    result = buildLinuxExecutable(stagingDir.string());
    break;
  case BuildPlatform::MacOS:
    result = buildMacOSBundle(stagingDir.string());
    break;
  case BuildPlatform::All:
    // Build for current platform
    result = buildLinuxExecutable(stagingDir.string());
    break;
  }

  if (result.isError()) {
    endStep(false, result.error());
    return result;
  }

  // Generate runtime_config.json
  updateProgress(0.8f, "Generating runtime configuration...");

  fs::path configDir = stagingDir / "config";
  fs::create_directories(configDir);

  fs::path configPath = configDir / "runtime_config.json";
  std::ofstream configFile(configPath);
  if (configFile.is_open()) {
    configFile << "{\n";
    configFile << "  \"version\": \"1.0\",\n";
    configFile << "  \"game\": {\n";
    configFile << "    \"name\": \"" << m_config.executableName << "\",\n";
    configFile << "    \"version\": \"" << m_config.version << "\"\n";
    configFile << "  },\n";
    configFile << "  \"localization\": {\n";
    configFile << "    \"default_locale\": \"" << m_config.defaultLanguage << "\",\n";
    configFile << "    \"available_locales\": [";

    bool first = true;
    for (const auto &lang : m_config.includedLanguages) {
      if (!first)
        configFile << ", ";
      first = false;
      configFile << "\"" << lang << "\"";
    }
    configFile << "]\n";
    configFile << "  },\n";
    configFile << "  \"packs\": {\n";
    configFile << "    \"directory\": \"packs\",\n";
    configFile << "    \"index_file\": \"packs_index.json\",\n";
    configFile << "    \"encrypted\": " << (m_config.encryptAssets ? "true" : "false") << "\n";
    configFile << "  },\n";
    configFile << "  \"runtime\": {\n";
    configFile << "    \"enable_logging\": " << (m_config.enableLogging ? "true" : "false") << ",\n";
    configFile << "    \"enable_debug_console\": " << (m_config.includeDebugConsole ? "true" : "false") << "\n";
    configFile << "  }\n";
    configFile << "}\n";
    configFile.close();
  }

  logMessage("Runtime bundle created for " +
                 BuildUtils::getPlatformName(m_config.platform),
             false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::signAndFinalize() {
  beginStep("Verify", "Verifying and finalizing build");

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";

  // Verify pack integrity
  updateProgress(0.2f, "Verifying pack integrity...");

  fs::path packsDir = stagingDir / "packs";
  if (fs::exists(packsDir)) {
    for (const auto &entry : fs::directory_iterator(packsDir)) {
      if (entry.path().extension() == ".nmres") {
        // Basic file integrity check - verify file is readable
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file.is_open()) {
          endStep(false, "Cannot read pack file: " + entry.path().string());
          return Result<void>::error("Pack file verification failed: " +
                                     entry.path().string());
        }

        // Read magic number (should be "NMRS")
        char magic[4];
        file.read(magic, 4);
        if (file.gcount() == 4) {
          if (std::strncmp(magic, "NMRS", 4) != 0) {
            m_progress.warnings.push_back(
                "Pack file has invalid magic number: " + entry.path().string());
          }
        }
        file.close();
      }
    }
  }

  // Verify config files
  updateProgress(0.5f, "Verifying configuration...");

  fs::path configPath = stagingDir / "config" / "runtime_config.json";
  if (fs::exists(configPath)) {
    std::ifstream configFile(configPath);
    if (!configFile.is_open()) {
      m_progress.warnings.push_back("Cannot read runtime_config.json");
    }
    configFile.close();
  }

  // Sign executable if requested
  if (m_config.signExecutable && !m_config.signingCertificate.empty()) {
    updateProgress(0.7f, "Signing executable...");
    logMessage("Code signing not yet implemented - skipping", false);
    m_progress.warnings.push_back(
        "Code signing requested but not yet implemented");
  }

  // Calculate final sizes
  updateProgress(0.9f, "Calculating build statistics...");

  i64 totalSize = 0;
  i64 compressedSize = 0;

  if (fs::exists(packsDir)) {
    for (const auto &entry : fs::recursive_directory_iterator(packsDir)) {
      if (entry.is_regular_file()) {
        compressedSize += static_cast<i64>(entry.file_size());
      }
    }
  }

  totalSize = BuildUtils::calculateDirectorySize(stagingDir.string());

  m_lastResult.totalSize = totalSize;
  m_lastResult.compressedSize = compressedSize;

  logMessage("Build verification complete. Total size: " +
                 BuildUtils::formatFileSize(totalSize),
             false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::cleanup() {
  // Clean up temporary files if build failed
  if (!m_progress.wasSuccessful) {
    fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";
    if (fs::exists(stagingDir)) {
      try {
        fs::remove_all(stagingDir);
      } catch (const fs::filesystem_error &e) {
        return Result<void>::error(std::string("Cleanup failed: ") + e.what());
      }
    }
  }
  return Result<void>::ok();
}

void BuildSystem::updateProgress(f32 stepProgress, const std::string &task) {
  if (m_progress.currentStepIndex >= 0 &&
      m_progress.currentStepIndex < static_cast<i32>(m_progress.steps.size())) {

    // Calculate overall progress
    f32 completedWeight = 0.0f;
    for (i32 i = 0; i < m_progress.currentStepIndex; i++) {
      completedWeight += m_progress.steps[i].progressWeight;
    }

    f32 currentWeight = m_progress.steps[m_progress.currentStepIndex].progressWeight;
    m_progress.progress = completedWeight + (currentWeight * stepProgress);
  }

  m_progress.currentTask = task;

  if (m_onProgressUpdate) {
    m_onProgressUpdate(m_progress);
  }
}

void BuildSystem::logMessage(const std::string &message, bool isError) {
  if (isError) {
    m_progress.errors.push_back(message);
  } else {
    m_progress.infoMessages.push_back(message);
  }

  if (m_onLogMessage) {
    m_onLogMessage(message, isError);
  }
}

void BuildSystem::beginStep(const std::string &name,
                            const std::string &description) {
  // Find step by name
  for (i32 i = 0; i < static_cast<i32>(m_progress.steps.size()); i++) {
    if (m_progress.steps[i].name == name) {
      m_progress.currentStepIndex = i;
      m_progress.currentStep = name;
      m_progress.steps[i].description = description;
      break;
    }
  }

  logMessage("Starting: " + name + " - " + description, false);
  updateProgress(0.0f, description);
}

void BuildSystem::endStep(bool success, const std::string &errorMessage) {
  if (m_progress.currentStepIndex >= 0 &&
      m_progress.currentStepIndex < static_cast<i32>(m_progress.steps.size())) {
    auto &step = m_progress.steps[m_progress.currentStepIndex];
    step.completed = true;
    step.success = success;
    step.errorMessage = errorMessage;

    if (m_onStepComplete) {
      m_onStepComplete(step);
    }
  }

  if (!success) {
    logMessage("Step failed: " + errorMessage, true);
  }
}

ScriptCompileResult BuildSystem::compileScript(const std::string &scriptPath) {
  ScriptCompileResult result;
  result.sourcePath = scriptPath;
  result.success = true;

  try {
    // Read script file
    std::ifstream file(scriptPath);
    if (!file.is_open()) {
      result.success = false;
      result.errors.push_back("Cannot open file: " + scriptPath);
      return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    // Basic syntax validation (placeholder for full compilation)
    // In a real implementation, this would use the scripting::Compiler
    if (source.empty()) {
      result.warnings.push_back("Script file is empty");
    }

    // Check for basic syntax errors
    i32 braceCount = 0;
    i32 parenCount = 0;
    for (char c : source) {
      if (c == '{')
        braceCount++;
      else if (c == '}')
        braceCount--;
      else if (c == '(')
        parenCount++;
      else if (c == ')')
        parenCount--;
    }

    if (braceCount != 0) {
      result.warnings.push_back("Unbalanced braces detected");
    }
    if (parenCount != 0) {
      result.warnings.push_back("Unbalanced parentheses detected");
    }

    result.bytecodeSize = static_cast<i32>(source.size()); // Placeholder

  } catch (const std::exception &e) {
    result.success = false;
    result.errors.push_back(std::string("Compilation error: ") + e.what());
  }

  return result;
}

Result<void> BuildSystem::compileBytecode(const std::string &outputPath) {
  try {
    // Create a combined bytecode file (placeholder implementation)
    std::ofstream output(outputPath, std::ios::binary);
    if (!output.is_open()) {
      return Result<void>::error("Cannot create bytecode file: " + outputPath);
    }

    // Write header
    const char magic[] = "NMBC"; // NovelMind ByteCode
    output.write(magic, 4);

    u32 version = 1;
    output.write(reinterpret_cast<const char *>(&version), sizeof(version));

    u32 scriptCount = static_cast<u32>(m_scriptFiles.size());
    output.write(reinterpret_cast<const char *>(&scriptCount), sizeof(scriptCount));

    // Write placeholder bytecode for each script
    for (const auto &scriptPath : m_scriptFiles) {
      std::ifstream file(scriptPath);
      if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();
        file.close();

        u32 size = static_cast<u32>(source.size());
        output.write(reinterpret_cast<const char *>(&size), sizeof(size));
        output.write(source.data(), source.size());
      }
    }

    output.close();
    return Result<void>::ok();

  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Bytecode generation failed: ") +
                               e.what());
  }
}

AssetProcessResult BuildSystem::processImage(const std::string &sourcePath,
                                             const std::string &outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    // For now, just copy the file (in production, would apply optimization)
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);

    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = e.what();
  }

  return result;
}

AssetProcessResult BuildSystem::processAudio(const std::string &sourcePath,
                                             const std::string &outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    // For now, just copy the file (in production, would apply compression)
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);

    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = e.what();
  }

  return result;
}

AssetProcessResult BuildSystem::processFont(const std::string &sourcePath,
                                            const std::string &outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    // Fonts are typically copied as-is
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);

    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = e.what();
  }

  return result;
}

AssetProcessResult BuildSystem::processData(const std::string &sourcePath,
                                            const std::string &outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    // Copy generic data files
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);

    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = e.what();
  }

  return result;
}

Result<void> BuildSystem::buildPack(const std::string &outputPath,
                                    const std::vector<std::string> &files,
                                    bool encrypt, bool compress) {
  try {
    std::ofstream output(outputPath, std::ios::binary);
    if (!output.is_open()) {
      return Result<void>::error("Cannot create pack file: " + outputPath);
    }

    // Write pack header
    const char magic[] = "NMRS"; // NovelMind Resource Pack
    output.write(magic, 4);

    u16 versionMajor = 1;
    u16 versionMinor = 0;
    output.write(reinterpret_cast<const char *>(&versionMajor), sizeof(versionMajor));
    output.write(reinterpret_cast<const char *>(&versionMinor), sizeof(versionMinor));

    u32 flags = 0;
    if (encrypt)
      flags |= 0x01; // ENCRYPTED flag
    if (compress)
      flags |= 0x02; // COMPRESSED flag
    output.write(reinterpret_cast<const char *>(&flags), sizeof(flags));

    u32 resourceCount = static_cast<u32>(files.size());
    output.write(reinterpret_cast<const char *>(&resourceCount), sizeof(resourceCount));

    // Reserve space for table offsets (will be filled later)
    u64 resourceTableOffset = 0;
    u64 stringTableOffset = 0;
    u64 dataOffset = 0;
    auto headerPos = output.tellp();
    output.write(reinterpret_cast<const char *>(&resourceTableOffset), sizeof(resourceTableOffset));
    output.write(reinterpret_cast<const char *>(&stringTableOffset), sizeof(stringTableOffset));
    output.write(reinterpret_cast<const char *>(&dataOffset), sizeof(dataOffset));

    // Pad header to 64 bytes
    u64 totalFileSize = 0;
    output.write(reinterpret_cast<const char *>(&totalFileSize), sizeof(totalFileSize));
    char padding[16] = {0};
    output.write(padding, 16);

    // Record resource table start
    resourceTableOffset = static_cast<u64>(output.tellp());

    // Build string table
    std::vector<std::string> resourceIds;
    std::vector<u32> stringOffsets;
    u32 currentStringOffset = 0;

    for (const auto &file : files) {
      fs::path p(file);
      std::string resourceId = p.filename().string();
      resourceIds.push_back(resourceId);
      stringOffsets.push_back(currentStringOffset);
      currentStringOffset += static_cast<u32>(resourceId.size()) + 1; // +1 for null terminator
    }

    // Write resource table entries (48 bytes each)
    u64 currentDataOffset = 0;
    std::vector<std::pair<u64, u64>> resourceDataInfo; // offset, size pairs

    for (size_t i = 0; i < files.size(); i++) {
      u32 stringIdOffset = stringOffsets[i];
      output.write(reinterpret_cast<const char *>(&stringIdOffset), sizeof(stringIdOffset));

      u32 resourceType = 0x08; // Data type
      output.write(reinterpret_cast<const char *>(&resourceType), sizeof(resourceType));

      u64 dataOffsetEntry = currentDataOffset;
      output.write(reinterpret_cast<const char *>(&dataOffsetEntry), sizeof(dataOffsetEntry));

      u64 fileSize = fs::file_size(files[i]);
      u64 compressedSize = fileSize; // No actual compression for now
      u64 uncompressedSize = fileSize;

      output.write(reinterpret_cast<const char *>(&compressedSize), sizeof(compressedSize));
      output.write(reinterpret_cast<const char *>(&uncompressedSize), sizeof(uncompressedSize));

      u32 resourceFlags = 0;
      output.write(reinterpret_cast<const char *>(&resourceFlags), sizeof(resourceFlags));

      u32 crc32 = 0; // Placeholder
      output.write(reinterpret_cast<const char *>(&crc32), sizeof(crc32));

      u8 iv[8] = {0}; // Placeholder IV
      output.write(reinterpret_cast<const char *>(iv), 8);

      resourceDataInfo.emplace_back(currentDataOffset, fileSize);
      currentDataOffset += fileSize;
    }

    // Write string table
    stringTableOffset = static_cast<u64>(output.tellp());

    u32 stringCount = static_cast<u32>(resourceIds.size());
    output.write(reinterpret_cast<const char *>(&stringCount), sizeof(stringCount));

    for (u32 offset : stringOffsets) {
      output.write(reinterpret_cast<const char *>(&offset), sizeof(offset));
    }

    for (const auto &id : resourceIds) {
      output.write(id.c_str(), id.size() + 1); // Include null terminator
    }

    // Write resource data
    dataOffset = static_cast<u64>(output.tellp());

    for (const auto &file : files) {
      std::ifstream input(file, std::ios::binary);
      if (input.is_open()) {
        output << input.rdbuf();
        input.close();
      }
    }

    // Write footer
    const char footerMagic[] = "NMRF";
    output.write(footerMagic, 4);

    u32 tableCrc = 0; // Placeholder
    output.write(reinterpret_cast<const char *>(&tableCrc), sizeof(tableCrc));

    u64 timestamp =
        static_cast<u64>(std::chrono::system_clock::now().time_since_epoch().count());
    output.write(reinterpret_cast<const char *>(&timestamp), sizeof(timestamp));

    u32 buildNumber = 1;
    output.write(reinterpret_cast<const char *>(&buildNumber), sizeof(buildNumber));

    char footerPadding[12] = {0};
    output.write(footerPadding, 12);

    // Update header with correct offsets
    totalFileSize = static_cast<u64>(output.tellp());
    output.seekp(headerPos);
    output.write(reinterpret_cast<const char *>(&resourceTableOffset), sizeof(resourceTableOffset));
    output.write(reinterpret_cast<const char *>(&stringTableOffset), sizeof(stringTableOffset));
    output.write(reinterpret_cast<const char *>(&dataOffset), sizeof(dataOffset));
    output.write(reinterpret_cast<const char *>(&totalFileSize), sizeof(totalFileSize));

    output.close();
    return Result<void>::ok();

  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Pack creation failed: ") + e.what());
  }
}

Result<void> BuildSystem::buildWindowsExecutable(const std::string &outputPath) {
  // Create a launcher script/placeholder for Windows
  fs::path exePath =
      fs::path(outputPath) /
      (m_config.executableName + BuildUtils::getExecutableExtension(BuildPlatform::Windows));

  // In a real implementation, this would copy the runtime executable
  // For now, create a placeholder batch file
  fs::path batchPath =
      fs::path(outputPath) / (m_config.executableName + "_launcher.bat");

  std::ofstream batch(batchPath);
  if (batch.is_open()) {
    batch << "@echo off\n";
    batch << "echo NovelMind Runtime - " << m_config.executableName << "\n";
    batch << "echo Version: " << m_config.version << "\n";
    batch << "echo.\n";
    batch << "echo This is a placeholder launcher.\n";
    batch << "echo In production, this would start the game runtime.\n";
    batch << "pause\n";
    batch.close();
  }

  return Result<void>::ok();
}

Result<void> BuildSystem::buildLinuxExecutable(const std::string &outputPath) {
  // Create a launcher script/placeholder for Linux
  fs::path scriptPath =
      fs::path(outputPath) /
      (m_config.executableName + "_launcher.sh");

  std::ofstream script(scriptPath);
  if (script.is_open()) {
    script << "#!/bin/bash\n";
    script << "echo \"NovelMind Runtime - " << m_config.executableName << "\"\n";
    script << "echo \"Version: " << m_config.version << "\"\n";
    script << "echo \"\"\n";
    script << "echo \"This is a placeholder launcher.\"\n";
    script << "echo \"In production, this would start the game runtime.\"\n";
    script.close();

    // Make executable
    fs::permissions(scriptPath, fs::perms::owner_exec | fs::perms::group_exec |
                                    fs::perms::others_exec,
                    fs::perm_options::add);
  }

  return Result<void>::ok();
}

Result<void> BuildSystem::buildMacOSBundle(const std::string &outputPath) {
  // Create a basic .app bundle structure
  std::string appName = m_config.executableName + ".app";
  fs::path appPath = fs::path(outputPath) / appName;
  fs::path contentsPath = appPath / "Contents";
  fs::path macOSPath = contentsPath / "MacOS";
  fs::path resourcesPath = contentsPath / "Resources";

  fs::create_directories(macOSPath);
  fs::create_directories(resourcesPath);

  // Create Info.plist
  fs::path plistPath = contentsPath / "Info.plist";
  std::ofstream plist(plistPath);
  if (plist.is_open()) {
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    plist << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    plist << "<plist version=\"1.0\">\n";
    plist << "<dict>\n";
    plist << "  <key>CFBundleExecutable</key>\n";
    plist << "  <string>" << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleIdentifier</key>\n";
    plist << "  <string>com.novelmind." << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleName</key>\n";
    plist << "  <string>" << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleShortVersionString</key>\n";
    plist << "  <string>" << m_config.version << "</string>\n";
    plist << "  <key>CFBundleVersion</key>\n";
    plist << "  <string>" << m_config.version << "</string>\n";
    plist << "  <key>CFBundlePackageType</key>\n";
    plist << "  <string>APPL</string>\n";
    plist << "</dict>\n";
    plist << "</plist>\n";
    plist.close();
  }

  // Create placeholder executable
  fs::path exePath = macOSPath / m_config.executableName;
  std::ofstream exe(exePath);
  if (exe.is_open()) {
    exe << "#!/bin/bash\n";
    exe << "echo \"NovelMind Runtime - " << m_config.executableName << "\"\n";
    exe << "echo \"Version: " << m_config.version << "\"\n";
    exe.close();

    fs::permissions(exePath, fs::perms::owner_exec | fs::perms::group_exec |
                                 fs::perms::others_exec,
                    fs::perm_options::add);
  }

  // Copy packs and config to Resources
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  fs::path stagingConfig = fs::path(outputPath) / "config";

  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, resourcesPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  if (fs::exists(stagingConfig)) {
    fs::copy(stagingConfig, resourcesPath / "config",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  return Result<void>::ok();
}

// ============================================================================
// AssetProcessor Implementation
// ============================================================================

AssetProcessor::AssetProcessor() = default;
AssetProcessor::~AssetProcessor() = default;

Result<AssetProcessResult>
AssetProcessor::processImage(const std::string &sourcePath,
                             const std::string &outputPath, bool optimize) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    if (optimize) {
      // In production, apply image optimization
      // For now, just copy
    }
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);
    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));
  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = e.what();
    return Result<AssetProcessResult>::error(e.what());
  }

  return Result<AssetProcessResult>::ok(result);
}

Result<AssetProcessResult>
AssetProcessor::processAudio(const std::string &sourcePath,
                             const std::string &outputPath, bool compress) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    if (compress) {
      // In production, apply audio compression
    }
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);
    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));
  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = e.what();
    return Result<AssetProcessResult>::error(e.what());
  }

  return Result<AssetProcessResult>::ok(result);
}

Result<AssetProcessResult>
AssetProcessor::processFont(const std::string &sourcePath,
                            const std::string &outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);
    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));
  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = e.what();
    return Result<AssetProcessResult>::error(e.what());
  }

  return Result<AssetProcessResult>::ok(result);
}

Result<std::string>
AssetProcessor::generateTextureAtlas(const std::vector<std::string> &images,
                                     const std::string &outputPath,
                                     i32 maxSize) {
  // Placeholder for texture atlas generation
  // In production, this would use a proper atlas packing algorithm
  (void)images;
  (void)maxSize;
  return Result<std::string>::error("Texture atlas generation not yet implemented");
}

std::string AssetProcessor::getAssetType(const std::string &path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
      ext == ".gif") {
    return "image";
  }
  if (ext == ".ogg" || ext == ".wav" || ext == ".mp3" || ext == ".flac") {
    return "audio";
  }
  if (ext == ".ttf" || ext == ".otf" || ext == ".woff" || ext == ".woff2") {
    return "font";
  }
  if (ext == ".nms" || ext == ".nmscript") {
    return "script";
  }
  if (ext == ".json" || ext == ".xml" || ext == ".yaml") {
    return "data";
  }
  return "other";
}

bool AssetProcessor::needsProcessing(const std::string &sourcePath,
                                     const std::string &outputPath) const {
  if (!fs::exists(outputPath)) {
    return true;
  }

  auto sourceTime = fs::last_write_time(sourcePath);
  auto outputTime = fs::last_write_time(outputPath);

  return sourceTime > outputTime;
}

Result<void> AssetProcessor::resizeImage(const std::string &input,
                                         const std::string &output,
                                         i32 maxWidth, i32 maxHeight) {
  (void)input;
  (void)output;
  (void)maxWidth;
  (void)maxHeight;
  return Result<void>::error("Image resizing not yet implemented");
}

Result<void> AssetProcessor::compressImage(const std::string &input,
                                           const std::string &output,
                                           i32 quality) {
  (void)input;
  (void)output;
  (void)quality;
  return Result<void>::error("Image compression not yet implemented");
}

Result<void> AssetProcessor::convertImageFormat(const std::string &input,
                                                const std::string &output,
                                                const std::string &format) {
  (void)input;
  (void)output;
  (void)format;
  return Result<void>::error("Image format conversion not yet implemented");
}

Result<void> AssetProcessor::convertAudioFormat(const std::string &input,
                                                const std::string &output,
                                                const std::string &format) {
  (void)input;
  (void)output;
  (void)format;
  return Result<void>::error("Audio format conversion not yet implemented");
}

Result<void> AssetProcessor::normalizeAudio(const std::string &input,
                                            const std::string &output) {
  (void)input;
  (void)output;
  return Result<void>::error("Audio normalization not yet implemented");
}

// ============================================================================
// PackBuilder Implementation
// ============================================================================

PackBuilder::PackBuilder() = default;
PackBuilder::~PackBuilder() = default;

Result<void> PackBuilder::beginPack(const std::string &outputPath) {
  m_outputPath = outputPath;
  m_entries.clear();
  return Result<void>::ok();
}

Result<void> PackBuilder::addFile(const std::string &sourcePath,
                                  const std::string &packPath) {
  try {
    std::ifstream file(sourcePath, std::ios::binary);
    if (!file.is_open()) {
      return Result<void>::error("Cannot open file: " + sourcePath);
    }

    std::vector<u8> data((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    PackEntry entry;
    entry.path = packPath;
    entry.data = std::move(data);
    entry.originalSize = static_cast<i64>(entry.data.size());

    m_entries.push_back(std::move(entry));
    return Result<void>::ok();

  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to add file: ") + e.what());
  }
}

Result<void> PackBuilder::addData(const std::string &packPath,
                                  const std::vector<u8> &data) {
  PackEntry entry;
  entry.path = packPath;
  entry.data = data;
  entry.originalSize = static_cast<i64>(data.size());

  m_entries.push_back(std::move(entry));
  return Result<void>::ok();
}

Result<void> PackBuilder::finalizePack() {
  if (m_outputPath.empty()) {
    return Result<void>::error("Pack not initialized - call beginPack first");
  }

  try {
    std::ofstream output(m_outputPath, std::ios::binary);
    if (!output.is_open()) {
      return Result<void>::error("Cannot create pack file: " + m_outputPath);
    }

    // Write pack header (simplified)
    const char magic[] = "NMRS";
    output.write(magic, 4);

    u32 entryCount = static_cast<u32>(m_entries.size());
    output.write(reinterpret_cast<const char *>(&entryCount), sizeof(entryCount));

    // Write entries
    for (const auto &entry : m_entries) {
      // Write path length and path
      u32 pathLen = static_cast<u32>(entry.path.size());
      output.write(reinterpret_cast<const char *>(&pathLen), sizeof(pathLen));
      output.write(entry.path.c_str(), pathLen);

      // Write data
      std::vector<u8> processedData = entry.data;

      // Apply compression if enabled
      if (m_compressionLevel != CompressionLevel::None) {
        auto compressResult = compressData(entry.data);
        if (compressResult.isOk()) {
          processedData = compressResult.value();
        }
      }

      // Apply encryption if key is set
      if (!m_encryptionKey.empty()) {
        auto encryptResult = encryptData(processedData);
        if (encryptResult.isOk()) {
          processedData = encryptResult.value();
        }
      }

      u64 dataSize = processedData.size();
      output.write(reinterpret_cast<const char *>(&dataSize), sizeof(dataSize));
      output.write(reinterpret_cast<const char *>(processedData.data()), dataSize);
    }

    output.close();
    return Result<void>::ok();

  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Pack finalization failed: ") + e.what());
  }
}

void PackBuilder::setEncryptionKey(const std::string &key) {
  m_encryptionKey = key;
}

void PackBuilder::setCompressionLevel(CompressionLevel level) {
  m_compressionLevel = level;
}

PackBuilder::PackStats PackBuilder::getStats() const {
  PackStats stats;
  stats.fileCount = static_cast<i32>(m_entries.size());
  stats.uncompressedSize = 0;
  stats.compressedSize = 0;

  for (const auto &entry : m_entries) {
    stats.uncompressedSize += entry.originalSize;
    stats.compressedSize += static_cast<i64>(entry.data.size());
  }

  if (stats.uncompressedSize > 0) {
    stats.compressionRatio =
        static_cast<f32>(stats.compressedSize) /
        static_cast<f32>(stats.uncompressedSize);
  } else {
    stats.compressionRatio = 1.0f;
  }

  return stats;
}

Result<std::vector<u8>> PackBuilder::compressData(const std::vector<u8> &data) {
  // Placeholder - in production would use zlib
  return Result<std::vector<u8>>::ok(data);
}

Result<std::vector<u8>> PackBuilder::encryptData(const std::vector<u8> &data) {
  // Placeholder - in production would use AES-256-GCM
  return Result<std::vector<u8>>::ok(data);
}

// ============================================================================
// IntegrityChecker Implementation
// ============================================================================

IntegrityChecker::IntegrityChecker() = default;
IntegrityChecker::~IntegrityChecker() = default;

Result<std::vector<IntegrityChecker::Issue>>
IntegrityChecker::checkProject(const std::string &projectPath) {
  std::vector<Issue> allIssues;

  // Run all checks
  auto missingAssets = checkMissingAssets(projectPath);
  allIssues.insert(allIssues.end(), missingAssets.begin(), missingAssets.end());

  auto scriptIssues = checkScripts(projectPath);
  allIssues.insert(allIssues.end(), scriptIssues.begin(), scriptIssues.end());

  auto localizationIssues = checkLocalization(projectPath);
  allIssues.insert(allIssues.end(), localizationIssues.begin(),
                   localizationIssues.end());

  auto unreachableIssues = checkUnreachableContent(projectPath);
  allIssues.insert(allIssues.end(), unreachableIssues.begin(),
                   unreachableIssues.end());

  auto circularIssues = checkCircularReferences(projectPath);
  allIssues.insert(allIssues.end(), circularIssues.begin(),
                   circularIssues.end());

  return Result<std::vector<Issue>>::ok(allIssues);
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkMissingAssets(const std::string &projectPath) {
  std::vector<Issue> issues;

  // Scan for referenced assets in scripts and scenes
  m_referencedAssets.clear();
  m_existingAssets.clear();

  // Collect existing assets
  fs::path assetsDir = fs::path(projectPath) / "assets";
  if (fs::exists(assetsDir)) {
    for (const auto &entry : fs::recursive_directory_iterator(assetsDir)) {
      if (entry.is_regular_file()) {
        m_existingAssets.push_back(
            fs::relative(entry.path(), assetsDir).string());
      }
    }
  }

  // Check for missing required directories
  std::vector<std::string> requiredDirs = {"assets", "scripts"};
  for (const auto &dir : requiredDirs) {
    if (!fs::exists(fs::path(projectPath) / dir)) {
      Issue issue;
      issue.severity = Issue::Severity::Error;
      issue.message = "Missing required directory: " + dir;
      issue.file = projectPath;
      issues.push_back(issue);
    }
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkScripts(const std::string &projectPath) {
  std::vector<Issue> issues;

  fs::path scriptsDir = fs::path(projectPath) / "scripts";
  if (!fs::exists(scriptsDir)) {
    return issues;
  }

  for (const auto &entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".nms" || ext == ".nmscript") {
        // Basic syntax check
        std::ifstream file(entry.path());
        if (!file.is_open()) {
          Issue issue;
          issue.severity = Issue::Severity::Error;
          issue.message = "Cannot open script file";
          issue.file = entry.path().string();
          issues.push_back(issue);
          continue;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();

        // Check for balanced braces
        i32 braceCount = 0;
        i32 line = 1;
        for (char c : content) {
          if (c == '\n')
            line++;
          if (c == '{')
            braceCount++;
          if (c == '}')
            braceCount--;
        }

        if (braceCount != 0) {
          Issue issue;
          issue.severity = Issue::Severity::Warning;
          issue.message = "Unbalanced braces detected";
          issue.file = entry.path().string();
          issues.push_back(issue);
        }
      }
    }
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkLocalization(const std::string &projectPath) {
  std::vector<Issue> issues;

  fs::path localizationDir = fs::path(projectPath) / "localization";
  if (!fs::exists(localizationDir)) {
    Issue issue;
    issue.severity = Issue::Severity::Info;
    issue.message = "No localization directory found";
    issue.file = projectPath;
    issues.push_back(issue);
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkUnreachableContent(const std::string &projectPath) {
  std::vector<Issue> issues;
  // Placeholder - would analyze scene graph for unreachable nodes
  (void)projectPath;
  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkCircularReferences(const std::string &projectPath) {
  std::vector<Issue> issues;
  // Placeholder - would check for circular scene/script references
  (void)projectPath;
  return issues;
}

} // namespace NovelMind::editor
