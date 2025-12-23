/**
 * @file build_size_analyzer.cpp
 * @brief Build Size Analyzer implementation for NovelMind
 *
 * Provides comprehensive build size analysis:
 * - Per-category breakdown (images, audio, scripts, etc.)
 * - Duplicate detection via content hashing
 * - Unused asset detection
 * - Optimization suggestions
 * - Export to JSON/HTML/CSV
 */

#include "NovelMind/editor/build_size_analyzer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// SizeVisualization Helpers
// ============================================================================

namespace SizeVisualization {

std::string formatBytes(u64 bytes) {
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

renderer::Color getCategoryColor(AssetCategory category) {
  switch (category) {
  case AssetCategory::Images:
    return renderer::Color{0.3f, 0.7f, 0.3f, 1.0f}; // Green
  case AssetCategory::Audio:
    return renderer::Color{0.3f, 0.5f, 0.9f, 1.0f}; // Blue
  case AssetCategory::Scripts:
    return renderer::Color{0.9f, 0.7f, 0.2f, 1.0f}; // Yellow/Orange
  case AssetCategory::Fonts:
    return renderer::Color{0.7f, 0.3f, 0.7f, 1.0f}; // Purple
  case AssetCategory::Video:
    return renderer::Color{0.9f, 0.3f, 0.3f, 1.0f}; // Red
  case AssetCategory::Data:
    return renderer::Color{0.5f, 0.5f, 0.5f, 1.0f}; // Gray
  case AssetCategory::Other:
  default:
    return renderer::Color{0.4f, 0.4f, 0.4f, 1.0f}; // Dark Gray
  }
}

std::string getCategoryIcon(AssetCategory category) {
  switch (category) {
  case AssetCategory::Images:
    return "image";
  case AssetCategory::Audio:
    return "audio";
  case AssetCategory::Scripts:
    return "code";
  case AssetCategory::Fonts:
    return "font";
  case AssetCategory::Video:
    return "video";
  case AssetCategory::Data:
    return "database";
  case AssetCategory::Other:
  default:
    return "file";
  }
}

renderer::Color getPriorityColor(OptimizationSuggestion::Priority priority) {
  switch (priority) {
  case OptimizationSuggestion::Priority::Critical:
    return renderer::Color{0.9f, 0.2f, 0.2f, 1.0f}; // Red
  case OptimizationSuggestion::Priority::High:
    return renderer::Color{0.9f, 0.5f, 0.2f, 1.0f}; // Orange
  case OptimizationSuggestion::Priority::Medium:
    return renderer::Color{0.9f, 0.8f, 0.2f, 1.0f}; // Yellow
  case OptimizationSuggestion::Priority::Low:
  default:
    return renderer::Color{0.4f, 0.7f, 0.4f, 1.0f}; // Green
  }
}

TreemapNode buildTreemap(const BuildSizeAnalysis &analysis) {
  TreemapNode root;
  root.label = "Build";
  root.size = analysis.totalOriginalSize;
  root.color = renderer::Color{0.3f, 0.3f, 0.3f, 1.0f};

  // Group assets by category
  std::unordered_map<AssetCategory, TreemapNode> categoryNodes;

  for (const auto &asset : analysis.assets) {
    if (categoryNodes.find(asset.category) == categoryNodes.end()) {
      TreemapNode categoryNode;
      switch (asset.category) {
      case AssetCategory::Images:
        categoryNode.label = "Images";
        break;
      case AssetCategory::Audio:
        categoryNode.label = "Audio";
        break;
      case AssetCategory::Scripts:
        categoryNode.label = "Scripts";
        break;
      case AssetCategory::Fonts:
        categoryNode.label = "Fonts";
        break;
      case AssetCategory::Video:
        categoryNode.label = "Video";
        break;
      case AssetCategory::Data:
        categoryNode.label = "Data";
        break;
      case AssetCategory::Other:
      default:
        categoryNode.label = "Other";
        break;
      }
      categoryNode.size = 0;
      categoryNode.color = getCategoryColor(asset.category);
      categoryNodes[asset.category] = categoryNode;
    }

    TreemapNode assetNode;
    assetNode.label = asset.name;
    assetNode.size = asset.originalSize;
    assetNode.color = getCategoryColor(asset.category);

    categoryNodes[asset.category].children.push_back(assetNode);
    categoryNodes[asset.category].size += asset.originalSize;
  }

  // Add category nodes to root
  for (auto &[category, node] : categoryNodes) {
    root.children.push_back(std::move(node));
  }

  return root;
}

void layoutTreemap(TreemapNode &root, f32 x, f32 y, f32 width, f32 height) {
  root.x = x;
  root.y = y;
  root.width = width;
  root.height = height;

  if (root.children.empty() || root.size == 0) {
    return;
  }

  // Simple squarified treemap layout
  bool horizontal = width >= height;
  f32 offset = 0.0f;

  for (auto &child : root.children) {
    f32 ratio = static_cast<f32>(child.size) / static_cast<f32>(root.size);

    if (horizontal) {
      f32 childWidth = width * ratio;
      layoutTreemap(child, x + offset, y, childWidth, height);
      offset += childWidth;
    } else {
      f32 childHeight = height * ratio;
      layoutTreemap(child, x, y + offset, width, childHeight);
      offset += childHeight;
    }
  }
}

} // namespace SizeVisualization

// ============================================================================
// BuildSizeAnalyzer Implementation
// ============================================================================

BuildSizeAnalyzer::BuildSizeAnalyzer() = default;

void BuildSizeAnalyzer::setProjectPath(const std::string &projectPath) {
  m_projectPath = projectPath;
}

void BuildSizeAnalyzer::setConfig(const BuildSizeAnalysisConfig &config) {
  m_config = config;
}

Result<BuildSizeAnalysis> BuildSizeAnalyzer::analyze() {
  auto startTime = std::chrono::steady_clock::now();

  // Notify listeners
  for (auto *listener : m_listeners) {
    listener->onAnalysisStarted();
  }

  // Reset analysis
  m_analysis = BuildSizeAnalysis{};
  m_hashToFiles.clear();
  m_referencedAssets.clear();

  try {
    // Scan all assets
    reportProgress("Scanning assets...", 0.0f);
    scanAssets();

    // Analyze each asset
    reportProgress("Analyzing assets...", 0.2f);
    for (size_t i = 0; i < m_analysis.assets.size(); i++) {
      analyzeAsset(m_analysis.assets[i]);

      f32 progress = 0.2f + 0.3f * static_cast<f32>(i) /
                               static_cast<f32>(m_analysis.assets.size());
      reportProgress("Analyzing: " + m_analysis.assets[i].name, progress);
    }

    // Detect duplicates
    if (m_config.detectDuplicates) {
      reportProgress("Detecting duplicates...", 0.5f);
      detectDuplicates();
    }

    // Detect unused assets
    if (m_config.detectUnused) {
      reportProgress("Detecting unused assets...", 0.6f);
      detectUnused();
    }

    // Generate suggestions
    if (m_config.generateSuggestions) {
      reportProgress("Generating suggestions...", 0.8f);
      generateSuggestions();
    }

    // Calculate summaries
    reportProgress("Calculating summaries...", 0.9f);
    calculateSummaries();

    // Finalize
    auto endTime = std::chrono::steady_clock::now();
    m_analysis.analysisTimeMs =
        std::chrono::duration<f64, std::milli>(endTime - startTime).count();
    m_analysis.analysisTimestamp = static_cast<u64>(
        std::chrono::system_clock::now().time_since_epoch().count());

    reportProgress("Analysis complete", 1.0f);

    // Notify listeners
    for (auto *listener : m_listeners) {
      listener->onAnalysisCompleted(m_analysis);
    }

    return Result<BuildSizeAnalysis>::ok(m_analysis);

  } catch (const std::exception &e) {
    return Result<BuildSizeAnalysis>::error(
        std::string("Analysis failed: ") + e.what());
  }
}

void BuildSizeAnalyzer::addListener(IBuildSizeListener *listener) {
  if (listener) {
    m_listeners.push_back(listener);
  }
}

void BuildSizeAnalyzer::removeListener(IBuildSizeListener *listener) {
  m_listeners.erase(
      std::remove(m_listeners.begin(), m_listeners.end(), listener),
      m_listeners.end());
}

Result<void>
BuildSizeAnalyzer::applyOptimization(const OptimizationSuggestion &suggestion) {
  // Placeholder implementation
  (void)suggestion;
  return Result<void>::error("Optimization application not yet implemented");
}

Result<void> BuildSizeAnalyzer::applyAllAutoOptimizations() {
  i32 applied = 0;
  for (const auto &suggestion : m_analysis.suggestions) {
    if (suggestion.canAutoFix) {
      auto result = applyOptimization(suggestion);
      if (result.isOk()) {
        applied++;
      }
    }
  }

  if (applied > 0) {
    // Re-analyze after optimizations
    return analyze().isOk() ? Result<void>::ok()
                            : Result<void>::error("Re-analysis failed");
  }

  return Result<void>::ok();
}

Result<void> BuildSizeAnalyzer::removeDuplicates() {
  // Placeholder implementation
  return Result<void>::error("Duplicate removal not yet implemented");
}

Result<void> BuildSizeAnalyzer::removeUnusedAssets() {
  // Placeholder implementation
  return Result<void>::error("Unused asset removal not yet implemented");
}

Result<std::string> BuildSizeAnalyzer::exportAsJson() const {
  std::ostringstream json;
  json << "{\n";
  json << "  \"totalOriginalSize\": " << m_analysis.totalOriginalSize << ",\n";
  json << "  \"totalCompressedSize\": " << m_analysis.totalCompressedSize
       << ",\n";
  json << "  \"totalFileCount\": " << m_analysis.totalFileCount << ",\n";
  json << "  \"overallCompressionRatio\": " << m_analysis.overallCompressionRatio
       << ",\n";
  json << "  \"totalWastedSpace\": " << m_analysis.totalWastedSpace << ",\n";
  json << "  \"unusedSpace\": " << m_analysis.unusedSpace << ",\n";
  json << "  \"potentialSavings\": " << m_analysis.potentialSavings << ",\n";
  json << "  \"analysisTimeMs\": " << m_analysis.analysisTimeMs << ",\n";

  // Categories
  json << "  \"categories\": [\n";
  for (size_t i = 0; i < m_analysis.categorySummaries.size(); i++) {
    const auto &cat = m_analysis.categorySummaries[i];
    json << "    {\n";
    json << "      \"category\": " << static_cast<int>(cat.category) << ",\n";
    json << "      \"fileCount\": " << cat.fileCount << ",\n";
    json << "      \"totalOriginalSize\": " << cat.totalOriginalSize << ",\n";
    json << "      \"totalCompressedSize\": " << cat.totalCompressedSize << ",\n";
    json << "      \"percentageOfTotal\": " << cat.percentageOfTotal << "\n";
    json << "    }";
    if (i < m_analysis.categorySummaries.size() - 1)
      json << ",";
    json << "\n";
  }
  json << "  ],\n";

  // Duplicates count
  json << "  \"duplicateGroups\": " << m_analysis.duplicates.size() << ",\n";

  // Unused assets count
  json << "  \"unusedAssets\": " << m_analysis.unusedAssets.size() << ",\n";

  // Suggestions count
  json << "  \"suggestions\": " << m_analysis.suggestions.size() << "\n";

  json << "}\n";

  return Result<std::string>::ok(json.str());
}

Result<void> BuildSizeAnalyzer::exportAsHtml(const std::string &outputPath) const {
  std::ofstream file(outputPath);
  if (!file.is_open()) {
    return Result<void>::error("Cannot create HTML file: " + outputPath);
  }

  file << "<!DOCTYPE html>\n";
  file << "<html>\n";
  file << "<head>\n";
  file << "  <title>Build Size Analysis Report</title>\n";
  file << "  <style>\n";
  file << "    body { font-family: Arial, sans-serif; margin: 20px; background: #1e1e1e; color: #d4d4d4; }\n";
  file << "    h1 { color: #569cd6; }\n";
  file << "    h2 { color: #4ec9b0; }\n";
  file << "    table { border-collapse: collapse; width: 100%; margin-bottom: 20px; }\n";
  file << "    th, td { border: 1px solid #3c3c3c; padding: 8px; text-align: left; }\n";
  file << "    th { background-color: #252526; }\n";
  file << "    tr:nth-child(even) { background-color: #2d2d30; }\n";
  file << "    .warning { color: #ce9178; }\n";
  file << "    .error { color: #f14c4c; }\n";
  file << "    .size { text-align: right; }\n";
  file << "  </style>\n";
  file << "</head>\n";
  file << "<body>\n";

  file << "  <h1>Build Size Analysis Report</h1>\n";

  // Summary
  file << "  <h2>Summary</h2>\n";
  file << "  <table>\n";
  file << "    <tr><th>Metric</th><th>Value</th></tr>\n";
  file << "    <tr><td>Total Size</td><td class='size'>"
       << SizeVisualization::formatBytes(m_analysis.totalOriginalSize) << "</td></tr>\n";
  file << "    <tr><td>File Count</td><td class='size'>"
       << m_analysis.totalFileCount << "</td></tr>\n";
  file << "    <tr><td>Compression Ratio</td><td class='size'>"
       << std::fixed << std::setprecision(2)
       << (m_analysis.overallCompressionRatio * 100) << "%</td></tr>\n";
  file << "    <tr><td>Wasted Space (Duplicates)</td><td class='size'>"
       << SizeVisualization::formatBytes(m_analysis.totalWastedSpace) << "</td></tr>\n";
  file << "    <tr><td>Unused Space</td><td class='size'>"
       << SizeVisualization::formatBytes(m_analysis.unusedSpace) << "</td></tr>\n";
  file << "    <tr><td>Potential Savings</td><td class='size'>"
       << SizeVisualization::formatBytes(m_analysis.potentialSavings) << "</td></tr>\n";
  file << "  </table>\n";

  // Categories
  file << "  <h2>Size by Category</h2>\n";
  file << "  <table>\n";
  file << "    <tr><th>Category</th><th>Files</th><th>Size</th><th>% of Total</th></tr>\n";
  for (const auto &cat : m_analysis.categorySummaries) {
    std::string catName;
    switch (cat.category) {
    case AssetCategory::Images:
      catName = "Images";
      break;
    case AssetCategory::Audio:
      catName = "Audio";
      break;
    case AssetCategory::Scripts:
      catName = "Scripts";
      break;
    case AssetCategory::Fonts:
      catName = "Fonts";
      break;
    case AssetCategory::Video:
      catName = "Video";
      break;
    case AssetCategory::Data:
      catName = "Data";
      break;
    default:
      catName = "Other";
      break;
    }
    file << "    <tr><td>" << catName << "</td><td class='size'>"
         << cat.fileCount << "</td><td class='size'>"
         << SizeVisualization::formatBytes(cat.totalOriginalSize)
         << "</td><td class='size'>" << std::fixed << std::setprecision(1)
         << cat.percentageOfTotal << "%</td></tr>\n";
  }
  file << "  </table>\n";

  // Suggestions
  if (!m_analysis.suggestions.empty()) {
    file << "  <h2>Optimization Suggestions</h2>\n";
    file << "  <table>\n";
    file << "    <tr><th>Priority</th><th>Type</th><th>Asset</th><th>Description</th><th>Est. Savings</th></tr>\n";
    for (const auto &sug : m_analysis.suggestions) {
      std::string priority;
      std::string priorityClass;
      switch (sug.priority) {
      case OptimizationSuggestion::Priority::Critical:
        priority = "Critical";
        priorityClass = "error";
        break;
      case OptimizationSuggestion::Priority::High:
        priority = "High";
        priorityClass = "warning";
        break;
      case OptimizationSuggestion::Priority::Medium:
        priority = "Medium";
        break;
      default:
        priority = "Low";
        break;
      }

      file << "    <tr><td class='" << priorityClass << "'>" << priority
           << "</td><td>" << static_cast<int>(sug.type) << "</td><td>"
           << sug.assetPath << "</td><td>" << sug.description
           << "</td><td class='size'>"
           << SizeVisualization::formatBytes(sug.estimatedSavings)
           << "</td></tr>\n";
    }
    file << "  </table>\n";
  }

  file << "</body>\n";
  file << "</html>\n";

  file.close();
  return Result<void>::ok();
}

Result<void> BuildSizeAnalyzer::exportAsCsv(const std::string &outputPath) const {
  std::ofstream file(outputPath);
  if (!file.is_open()) {
    return Result<void>::error("Cannot create CSV file: " + outputPath);
  }

  // Header
  file << "Path,Name,Category,Original Size,Compressed Size,Compression Ratio,Is Duplicate,Is Unused\n";

  // Assets
  for (const auto &asset : m_analysis.assets) {
    std::string catName;
    switch (asset.category) {
    case AssetCategory::Images:
      catName = "Images";
      break;
    case AssetCategory::Audio:
      catName = "Audio";
      break;
    case AssetCategory::Scripts:
      catName = "Scripts";
      break;
    case AssetCategory::Fonts:
      catName = "Fonts";
      break;
    case AssetCategory::Video:
      catName = "Video";
      break;
    case AssetCategory::Data:
      catName = "Data";
      break;
    default:
      catName = "Other";
      break;
    }

    file << "\"" << asset.path << "\","
         << "\"" << asset.name << "\","
         << catName << ","
         << asset.originalSize << ","
         << asset.compressedSize << ","
         << asset.compressionRatio << ","
         << (asset.isDuplicate ? "Yes" : "No") << ","
         << (asset.isUnused ? "Yes" : "No") << "\n";
  }

  file.close();
  return Result<void>::ok();
}

// ============================================================================
// Private Methods
// ============================================================================

void BuildSizeAnalyzer::scanAssets() {
  if (m_projectPath.empty()) {
    return;
  }

  fs::path projectDir(m_projectPath);
  if (!fs::exists(projectDir)) {
    return;
  }

  // Scan assets directory
  fs::path assetsDir = projectDir / "assets";
  if (fs::exists(assetsDir)) {
    for (const auto &entry : fs::recursive_directory_iterator(assetsDir)) {
      if (!entry.is_regular_file()) {
        continue;
      }

      AssetCategory category = categorizeAsset(entry.path().string());

      // Check if category should be analyzed
      bool shouldAnalyze = false;
      switch (category) {
      case AssetCategory::Images:
        shouldAnalyze = m_config.analyzeImages;
        break;
      case AssetCategory::Audio:
        shouldAnalyze = m_config.analyzeAudio;
        break;
      case AssetCategory::Scripts:
        shouldAnalyze = m_config.analyzeScripts;
        break;
      case AssetCategory::Fonts:
        shouldAnalyze = m_config.analyzeFonts;
        break;
      case AssetCategory::Video:
        shouldAnalyze = m_config.analyzeVideo;
        break;
      default:
        shouldAnalyze = m_config.analyzeOther;
        break;
      }

      if (!shouldAnalyze) {
        continue;
      }

      // Check exclude patterns
      bool excluded = false;
      for (const auto &pattern : m_config.excludePatterns) {
        if (entry.path().string().find(pattern) != std::string::npos) {
          excluded = true;
          break;
        }
      }

      if (excluded) {
        continue;
      }

      AssetSizeInfo info;
      info.path = entry.path().string();
      info.name = entry.path().filename().string();
      info.category = category;
      info.originalSize = static_cast<u64>(entry.file_size());
      info.compressedSize = info.originalSize; // Will be updated during analysis

      m_analysis.assets.push_back(info);
      m_analysis.totalFileCount++;
    }
  }

  // Scan scripts directory
  fs::path scriptsDir = projectDir / "scripts";
  if (fs::exists(scriptsDir) && m_config.analyzeScripts) {
    for (const auto &entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nmscript") {
          AssetSizeInfo info;
          info.path = entry.path().string();
          info.name = entry.path().filename().string();
          info.category = AssetCategory::Scripts;
          info.originalSize = static_cast<u64>(entry.file_size());
          info.compressedSize = info.originalSize;

          m_analysis.assets.push_back(info);
          m_analysis.totalFileCount++;
        }
      }
    }
  }
}

void BuildSizeAnalyzer::analyzeAsset(AssetSizeInfo &info) {
  // Compute file hash for duplicate detection
  std::string hash = computeFileHash(info.path);

  // Track for duplicate detection
  m_hashToFiles[hash].push_back(info.path);

  // Detect compression type
  info.compression = detectCompression(info.path);

  // Calculate compression ratio (placeholder - would need actual compression)
  info.compressionRatio = 1.0f;

  // Image-specific analysis
  if (info.category == AssetCategory::Images) {
    // Placeholder - would use image library to get dimensions
    // info.imageWidth = ...;
    // info.imageHeight = ...;

    // Check for oversized images
    if (info.originalSize > m_config.largeImageThreshold) {
      info.isOversized = true;
      info.optimizationSuggestions.push_back(
          "Consider reducing image size or using better compression");
    }
  }

  // Audio-specific analysis
  if (info.category == AssetCategory::Audio) {
    // Placeholder - would use audio library
    // info.audioDuration = ...;
    // info.audioSampleRate = ...;

    if (info.originalSize > m_config.largeAudioThreshold) {
      info.isOversized = true;
      info.optimizationSuggestions.push_back(
          "Consider using OGG Vorbis compression or reducing quality");
    }
  }

  // Update totals
  m_analysis.totalOriginalSize += info.originalSize;
  m_analysis.totalCompressedSize += info.compressedSize;
}

void BuildSizeAnalyzer::detectDuplicates() {
  for (const auto &[hash, files] : m_hashToFiles) {
    if (files.size() > 1) {
      DuplicateGroup group;
      group.hash = hash;
      group.paths = files;

      // Find size of first file
      for (const auto &asset : m_analysis.assets) {
        if (asset.path == files[0]) {
          group.singleFileSize = asset.originalSize;
          break;
        }
      }

      group.wastedSpace = group.singleFileSize * (files.size() - 1);
      m_analysis.totalWastedSpace += group.wastedSpace;

      // Mark duplicates in assets
      bool first = true;
      for (const auto &path : files) {
        if (first) {
          first = false;
          continue;
        }

        for (auto &asset : m_analysis.assets) {
          if (asset.path == path) {
            asset.isDuplicate = true;
            asset.duplicateOf = files[0];
            break;
          }
        }
      }

      m_analysis.duplicates.push_back(group);
    }
  }
}

void BuildSizeAnalyzer::detectUnused() {
  // Placeholder - would need to parse scripts and scenes to find references
  // For now, mark nothing as unused

  // In a real implementation:
  // 1. Parse all scripts for asset references
  // 2. Parse all scene files for asset references
  // 3. Mark any assets not in the reference set as unused
}

void BuildSizeAnalyzer::generateSuggestions() {
  // Suggest removing duplicates
  for (const auto &dup : m_analysis.duplicates) {
    OptimizationSuggestion suggestion;
    suggestion.priority = OptimizationSuggestion::Priority::High;
    suggestion.type = OptimizationSuggestion::Type::RemoveDuplicate;
    suggestion.assetPath = dup.paths[1]; // First duplicate
    suggestion.description = "Remove duplicate file (same content as " +
                             dup.paths[0] + ")";
    suggestion.estimatedSavings = dup.singleFileSize;
    suggestion.canAutoFix = true;

    m_analysis.suggestions.push_back(suggestion);
    m_analysis.potentialSavings += suggestion.estimatedSavings;
  }

  // Suggest optimizing large images
  for (const auto &asset : m_analysis.assets) {
    if (asset.category == AssetCategory::Images && asset.isOversized) {
      OptimizationSuggestion suggestion;
      suggestion.priority = OptimizationSuggestion::Priority::Medium;
      suggestion.type = OptimizationSuggestion::Type::CompressImage;
      suggestion.assetPath = asset.path;
      suggestion.description =
          "Large image detected (" +
          SizeVisualization::formatBytes(asset.originalSize) +
          "). Consider resizing or compressing.";
      suggestion.estimatedSavings = asset.originalSize / 2; // Rough estimate
      suggestion.canAutoFix = false;

      m_analysis.suggestions.push_back(suggestion);
      m_analysis.potentialSavings += suggestion.estimatedSavings;
    }

    if (asset.category == AssetCategory::Audio && asset.isOversized) {
      OptimizationSuggestion suggestion;
      suggestion.priority = OptimizationSuggestion::Priority::Medium;
      suggestion.type = OptimizationSuggestion::Type::CompressAudio;
      suggestion.assetPath = asset.path;
      suggestion.description =
          "Large audio file detected (" +
          SizeVisualization::formatBytes(asset.originalSize) +
          "). Consider using OGG Vorbis.";
      suggestion.estimatedSavings = asset.originalSize / 3; // Rough estimate
      suggestion.canAutoFix = false;

      m_analysis.suggestions.push_back(suggestion);
      m_analysis.potentialSavings += suggestion.estimatedSavings;
    }
  }

  // Suggest removing unused assets
  for (const auto &unusedPath : m_analysis.unusedAssets) {
    for (const auto &asset : m_analysis.assets) {
      if (asset.path == unusedPath) {
        OptimizationSuggestion suggestion;
        suggestion.priority = OptimizationSuggestion::Priority::High;
        suggestion.type = OptimizationSuggestion::Type::RemoveUnused;
        suggestion.assetPath = asset.path;
        suggestion.description = "Asset appears to be unused";
        suggestion.estimatedSavings = asset.originalSize;
        suggestion.canAutoFix = true;

        m_analysis.suggestions.push_back(suggestion);
        m_analysis.potentialSavings += suggestion.estimatedSavings;
        break;
      }
    }
  }

  // Sort suggestions by estimated savings (descending)
  std::sort(m_analysis.suggestions.begin(), m_analysis.suggestions.end(),
            [](const OptimizationSuggestion &a, const OptimizationSuggestion &b) {
              return a.estimatedSavings > b.estimatedSavings;
            });
}

void BuildSizeAnalyzer::calculateSummaries() {
  // Group by category
  std::unordered_map<AssetCategory, CategorySummary> categoryMap;

  for (const auto &asset : m_analysis.assets) {
    auto &summary = categoryMap[asset.category];
    summary.category = asset.category;
    summary.fileCount++;
    summary.totalOriginalSize += asset.originalSize;
    summary.totalCompressedSize += asset.compressedSize;
  }

  // Calculate percentages and averages
  for (auto &[category, summary] : categoryMap) {
    if (m_analysis.totalOriginalSize > 0) {
      summary.percentageOfTotal =
          static_cast<f32>(summary.totalOriginalSize) /
          static_cast<f32>(m_analysis.totalOriginalSize) * 100.0f;
    }

    if (summary.totalOriginalSize > 0) {
      summary.averageCompressionRatio =
          static_cast<f32>(summary.totalCompressedSize) /
          static_cast<f32>(summary.totalOriginalSize);
    }

    m_analysis.categorySummaries.push_back(summary);
  }

  // Sort by size (descending)
  std::sort(m_analysis.categorySummaries.begin(),
            m_analysis.categorySummaries.end(),
            [](const CategorySummary &a, const CategorySummary &b) {
              return a.totalOriginalSize > b.totalOriginalSize;
            });

  // Calculate overall compression ratio
  if (m_analysis.totalOriginalSize > 0) {
    m_analysis.overallCompressionRatio =
        static_cast<f32>(m_analysis.totalCompressedSize) /
        static_cast<f32>(m_analysis.totalOriginalSize);
  }
}

void BuildSizeAnalyzer::reportProgress(const std::string &task, f32 progress) {
  for (auto *listener : m_listeners) {
    listener->onAnalysisProgress(task, progress);
  }
}

std::string BuildSizeAnalyzer::computeFileHash(const std::string &path) {
  // Simple hash based on file size and first/last bytes
  // In production, would use SHA-256 or similar
  try {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      return "";
    }

    auto size = file.tellg();
    file.seekg(0);

    u64 hash = static_cast<u64>(size);

    // Read first 1KB
    char buffer[1024];
    file.read(buffer, std::min(static_cast<std::streamsize>(1024),
                               static_cast<std::streamsize>(size)));

    for (i32 i = 0; i < file.gcount(); i++) {
      hash = hash * 31 + static_cast<u8>(buffer[i]);
    }

    // Read last 1KB if file is larger
    if (size > 2048) {
      file.seekg(-1024, std::ios::end);
      file.read(buffer, 1024);

      for (i32 i = 0; i < file.gcount(); i++) {
        hash = hash * 31 + static_cast<u8>(buffer[i]);
      }
    }

    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();

  } catch (const std::exception &) {
    return "";
  }
}

CompressionType BuildSizeAnalyzer::detectCompression(const std::string &path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".png") {
    return CompressionType::Png;
  }
  if (ext == ".jpg" || ext == ".jpeg") {
    return CompressionType::Jpeg;
  }
  if (ext == ".ogg") {
    return CompressionType::Ogg;
  }

  return CompressionType::None;
}

AssetCategory BuildSizeAnalyzer::categorizeAsset(const std::string &path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Images
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
      ext == ".gif" || ext == ".webp" || ext == ".tga") {
    return AssetCategory::Images;
  }

  // Audio
  if (ext == ".ogg" || ext == ".wav" || ext == ".mp3" || ext == ".flac" ||
      ext == ".aac" || ext == ".m4a") {
    return AssetCategory::Audio;
  }

  // Scripts
  if (ext == ".nms" || ext == ".nmscript" || ext == ".lua" || ext == ".json") {
    return AssetCategory::Scripts;
  }

  // Fonts
  if (ext == ".ttf" || ext == ".otf" || ext == ".woff" || ext == ".woff2") {
    return AssetCategory::Fonts;
  }

  // Video
  if (ext == ".mp4" || ext == ".webm" || ext == ".avi" || ext == ".mkv") {
    return AssetCategory::Video;
  }

  // Data
  if (ext == ".xml" || ext == ".yaml" || ext == ".yml" || ext == ".csv" ||
      ext == ".dat" || ext == ".bin") {
    return AssetCategory::Data;
  }

  return AssetCategory::Other;
}

// ============================================================================
// BuildSizeAnalyzerPanel Implementation
// ============================================================================

BuildSizeAnalyzerPanel::BuildSizeAnalyzerPanel() = default;
BuildSizeAnalyzerPanel::~BuildSizeAnalyzerPanel() = default;

void BuildSizeAnalyzerPanel::update([[maybe_unused]] f64 deltaTime) {
  // Update panel state if needed
}

void BuildSizeAnalyzerPanel::render() {
  // Placeholder - would render using ImGui or similar
}

void BuildSizeAnalyzerPanel::onResize([[maybe_unused]] i32 width,
                                      [[maybe_unused]] i32 height) {
  // Handle resize
}

void BuildSizeAnalyzerPanel::setAnalyzer(BuildSizeAnalyzer *analyzer) {
  m_analyzer = analyzer;
}

void BuildSizeAnalyzerPanel::refreshAnalysis() {
  if (m_analyzer) {
    m_analyzer->analyze();
  }
}

void BuildSizeAnalyzerPanel::exportReport(const std::string &path) {
  if (m_analyzer) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".json") {
      auto result = m_analyzer->exportAsJson();
      if (result.isOk()) {
        std::ofstream file(path);
        file << result.value();
        file.close();
      }
    } else if (ext == ".html") {
      m_analyzer->exportAsHtml(path);
    } else if (ext == ".csv") {
      m_analyzer->exportAsCsv(path);
    }
  }
}

void BuildSizeAnalyzerPanel::setFilter(const std::string &filter) {
  m_filter = filter;
}

void BuildSizeAnalyzerPanel::setCategoryFilter(AssetCategory category) {
  m_categoryFilter = category;
}

void BuildSizeAnalyzerPanel::setOnAssetSelected(
    std::function<void(const std::string &)> callback) {
  m_onAssetSelected = std::move(callback);
}

void BuildSizeAnalyzerPanel::setOnOptimizationApplied(
    std::function<void()> callback) {
  m_onOptimizationApplied = std::move(callback);
}

void BuildSizeAnalyzerPanel::renderOverview() {
  // Placeholder
}

void BuildSizeAnalyzerPanel::renderCategoryBreakdown() {
  // Placeholder
}

void BuildSizeAnalyzerPanel::renderSizeList() {
  // Placeholder
}

void BuildSizeAnalyzerPanel::renderDuplicates() {
  // Placeholder
}

void BuildSizeAnalyzerPanel::renderUnused() {
  // Placeholder
}

void BuildSizeAnalyzerPanel::renderSuggestions() {
  // Placeholder
}

void BuildSizeAnalyzerPanel::renderToolbar() {
  // Placeholder
}

void BuildSizeAnalyzerPanel::renderPieChart([[maybe_unused]] f32 x,
                                            [[maybe_unused]] f32 y,
                                            [[maybe_unused]] f32 radius) {
  // Placeholder
}

void BuildSizeAnalyzerPanel::renderSizeBar([[maybe_unused]] f32 x,
                                           [[maybe_unused]] f32 y,
                                           [[maybe_unused]] f32 width,
                                           [[maybe_unused]] f32 height,
                                           [[maybe_unused]] u64 size,
                                           [[maybe_unused]] u64 total) {
  // Placeholder
}

std::string BuildSizeAnalyzerPanel::formatSize(u64 bytes) const {
  return SizeVisualization::formatBytes(bytes);
}

} // namespace NovelMind::editor
