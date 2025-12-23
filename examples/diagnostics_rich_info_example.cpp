/**
 * @file diagnostics_rich_info_example.cpp
 * @brief Example demonstrating diagnostics with related info and suggestions
 *
 * This example shows how to create diagnostics with:
 * - Related information (withRelated)
 * - Quick-fix suggestions (withSuggestion)
 *
 * Usage in code:
 *   #include "NovelMind/editor/error_reporter.hpp"
 *
 *   using namespace NovelMind::editor;
 *
 *   // Example 1: Diagnostic with related info
 *   Diagnostic diag;
 *   diag.severity = DiagnosticSeverity::Error;
 *   diag.category = DiagnosticCategory::Script;
 *   diag.message = "Undefined variable 'player'";
 *   diag.location = {.file = "main.script", .line = 42, .column = 10};
 *
 *   // Add related info showing where the variable might be defined
 *   DiagnosticRelated related;
 *   related.location = {.file = "globals.script", .line = 5, .column = 1};
 *   related.message = "Similar variable 'Player' defined here";
 *   diag.relatedInfo.push_back(related);
 *
 *   ErrorReporter::instance().report(diag);
 *
 *   // Example 2: Diagnostic with quick-fix suggestion
 *   Diagnostic diag2;
 *   diag2.severity = DiagnosticSeverity::Warning;
 *   diag2.category = DiagnosticCategory::Script;
 *   diag2.message = "Deprecated function 'showText' used";
 *   diag2.location = {.file = "scene1.script", .line = 15, .column = 5};
 *
 *   // Add a suggestion to replace with new function
 *   DiagnosticFix fix;
 *   fix.title = "Replace with 'displayText()'";
 *   fix.description = "The new function provides better formatting options";
 *   fix.replacementText = "displayText(message, {style: 'default'})";
 *   fix.range = {.file = "scene1.script", .line = 15, .column = 5,
 *                .endLine = 15, .endColumn = 30};
 *   diag2.fixes.push_back(fix);
 *
 *   ErrorReporter::instance().report(diag2);
 *
 *   // Example 3: Complex diagnostic with multiple related and suggestions
 *   Diagnostic diag3;
 *   diag3.severity = DiagnosticSeverity::Error;
 *   diag3.category = DiagnosticCategory::Graph;
 *   diag3.message = "Cyclic dependency detected in story graph";
 *   diag3.location = {.file = "chapter1.graph", .line = 25, .column = 1};
 *
 *   // Show all nodes involved in the cycle
 *   DiagnosticRelated rel1;
 *   rel1.location = {.file = "chapter1.graph", .line = 30, .column = 1};
 *   rel1.message = "Node 'scene_A' points here";
 *   diag3.relatedInfo.push_back(rel1);
 *
 *   DiagnosticRelated rel2;
 *   rel2.location = {.file = "chapter1.graph", .line = 35, .column = 1};
 *   rel2.message = "Node 'scene_B' points here";
 *   diag3.relatedInfo.push_back(rel2);
 *
 *   // Suggest breaking the cycle
 *   DiagnosticFix fix1;
 *   fix1.title = "Remove edge from scene_B to scene_A";
 *   fix1.description = "This will break the cycle";
 *   diag3.fixes.push_back(fix1);
 *
 *   DiagnosticFix fix2;
 *   fix2.title = "Add intermediate node";
 *   fix2.description = "Insert a decision node to break the cycle";
 *   diag3.fixes.push_back(fix2);
 *
 *   ErrorReporter::instance().report(diag3);
 */

#include "NovelMind/editor/error_reporter.hpp"
#include <iostream>

using namespace NovelMind::editor;

void createExampleDiagnostics() {
  // Example 1: Undefined variable with related info
  {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.category = DiagnosticCategory::Script;
    diag.code = "E001";
    diag.message = "Undefined variable 'player'";
    diag.location = SourceLocation{.file = "main.script",
                                   .line = 42,
                                   .column = 10,
                                   .endLine = 42,
                                   .endColumn = 16};

    DiagnosticRelated related;
    related.location = SourceLocation{.file = "globals.script",
                                      .line = 5,
                                      .column = 1,
                                      .endLine = 5,
                                      .endColumn = 20};
    related.message =
        "Similar variable 'Player' defined here (note capitalization)";
    diag.relatedInfo.push_back(related);

    ErrorReporter::instance().report(diag);
    std::cout << "Created diagnostic: Undefined variable with related info\n";
  }

  // Example 2: Deprecated function with suggestion
  {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Warning;
    diag.category = DiagnosticCategory::Script;
    diag.code = "W042";
    diag.message = "Deprecated function 'showText' used";
    diag.location = SourceLocation{.file = "scene1.script",
                                   .line = 15,
                                   .column = 5,
                                   .endLine = 15,
                                   .endColumn = 30};

    DiagnosticFix fix;
    fix.title = "Replace with 'displayText()'";
    fix.description = "The new function provides better formatting options";
    fix.replacementText = "displayText(message, {style: 'default'})";
    fix.range = SourceLocation{.file = "scene1.script",
                               .line = 15,
                               .column = 5,
                               .endLine = 15,
                               .endColumn = 30};
    diag.fixes.push_back(fix);

    ErrorReporter::instance().report(diag);
    std::cout << "Created diagnostic: Deprecated function with suggestion\n";
  }

  // Example 3: Cyclic dependency with multiple related and suggestions
  {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.category = DiagnosticCategory::Graph;
    diag.code = "E103";
    diag.message = "Cyclic dependency detected in story graph";
    diag.location = SourceLocation{.file = "chapter1.graph",
                                   .line = 25,
                                   .column = 1,
                                   .endLine = 25,
                                   .endColumn = 10};

    // Show all nodes involved in the cycle
    DiagnosticRelated rel1;
    rel1.location = SourceLocation{.file = "chapter1.graph",
                                   .line = 30,
                                   .column = 1,
                                   .endLine = 30,
                                   .endColumn = 10};
    rel1.message = "Node 'scene_A' points here";
    diag.relatedInfo.push_back(rel1);

    DiagnosticRelated rel2;
    rel2.location = SourceLocation{.file = "chapter1.graph",
                                   .line = 35,
                                   .column = 1,
                                   .endLine = 35,
                                   .endColumn = 10};
    rel2.message = "Node 'scene_B' points here, creating a cycle";
    diag.relatedInfo.push_back(rel2);

    // Suggest ways to break the cycle
    DiagnosticFix fix1;
    fix1.title = "Remove edge from scene_B to scene_A";
    fix1.description = "This will break the cycle by removing the back-edge";
    diag.fixes.push_back(fix1);

    DiagnosticFix fix2;
    fix2.title = "Add intermediate decision node";
    fix2.description =
        "Insert a decision node to break the cycle and add conditional logic";
    diag.fixes.push_back(fix2);

    ErrorReporter::instance().report(diag);
    std::cout
        << "Created diagnostic: Cyclic dependency with multiple related and "
           "suggestions\n";
  }

  // Example 4: Missing asset with related info
  {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.category = DiagnosticCategory::Asset;
    diag.code = "E201";
    diag.message = "Missing texture 'bg_forest.png'";
    diag.location = SourceLocation{.file = "scene_forest.scene",
                                   .line = 10,
                                   .column = 15,
                                   .endLine = 10,
                                   .endColumn = 35};

    DiagnosticRelated rel;
    rel.location = SourceLocation{.file = "scene_city.scene",
                                  .line = 8,
                                  .column = 15,
                                  .endLine = 8,
                                  .endColumn = 35};
    rel.message = "Asset also referenced here";
    diag.relatedInfo.push_back(rel);

    DiagnosticFix fix;
    fix.title = "Use placeholder texture";
    fix.description =
        "Replace with 'bg_placeholder.png' until the asset is available";
    fix.replacementText = "bg_placeholder.png";
    fix.range = SourceLocation{.file = "scene_forest.scene",
                               .line = 10,
                               .column = 15,
                               .endLine = 10,
                               .endColumn = 35};
    diag.fixes.push_back(fix);

    ErrorReporter::instance().report(diag);
    std::cout << "Created diagnostic: Missing asset with suggestion\n";
  }

  std::cout << "\nAll example diagnostics created successfully!\n";
  std::cout << "Check the Diagnostics panel to see:\n";
  std::cout << "- Expandable tree items with related info and suggestions\n";
  std::cout << "- Color-coded severity levels\n";
  std::cout << "- Double-click to navigate to locations\n";
  std::cout << "- Right-click suggestions to copy replacement text\n";
}

// This function can be called from the editor to populate test diagnostics
// int main() {
//   createExampleDiagnostics();
//   return 0;
// }
