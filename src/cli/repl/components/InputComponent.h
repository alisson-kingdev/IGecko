#pragma once
#include <algorithm>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "Theme.h"

namespace repl::components
{

using namespace ftxui;

// Helper to highlight syntax with cursor and auto-scroll support
Element
HighlightCode(const std::string& code, const style::Theme& theme, int cursor_pos, bool is_focused)
{
  // Regex for comments (block and line), strings, keywords, functions and numbers
  // Group 1: Block, 2: Line, 3: Strings, 4: Keywords, 5: Functions, 6: Numbers
  std::regex token_regex(
      R"((/\*(?:[^*]|(?:\*+[^*/]))*\*+/)|(//.*)|(".*?"|'.*?'|`.*?`)|(\b(?:package|func|var|const|if|else|for|while|return|class|constructor|static|new|get|set|this|try|catch|from|as|export|import|true|false|null|print|break)\b)|(\b[A-Za-z_][A-Za-z0-9_]*\b\s*(?=\())|(\b\d+\b))");

  auto words_begin = std::sregex_iterator(code.begin(), code.end(), token_regex);
  auto words_end = std::sregex_iterator();

  Elements lines;
  Elements current_line;
  int current_pos = 0;

  auto add_char = [&](char c, ftxui::Color col) {
    auto e = text(std::string(1, c)) | color(col);
    if (is_focused && current_pos == cursor_pos) {
      e = e | inverted | color(theme.accent) | focus;
    }
    current_line.push_back(e);
    current_pos++;
  };

  auto add_text = [&](const std::string& t, ftxui::Color col) {
    for (char c : t) {
      if (c == '\n') {
        if (is_focused && current_pos == cursor_pos) {
          current_line.push_back(text(" ") | inverted | color(theme.accent) | focus);
        }
        // Ensure empty lines have height
        if (current_line.empty()) {
          current_line.push_back(text(" "));
        }
        lines.push_back(hbox(std::move(current_line)));
        current_line = Elements();
        current_pos++; // for the \n
      }
      else {
        add_char(c, col);
      }
    }
  };

  size_t last_match_end = 0;
  for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
    const std::smatch& match = *i;

    // Text before match
    std::string before =
        code.substr(last_match_end, static_cast<size_t>(match.position()) - last_match_end);
    add_text(before, theme.fg);

    // Actual match
    ftxui::Color token_color = theme.fg;
    if (match[1].matched || match[2].matched) {
      token_color = theme.comment;
    }
    else if (match[3].matched) {
      token_color = theme.string;
    }
    else if (match[4].matched) {
      token_color = theme.keyword;
    }
    else if (match[5].matched) {
      token_color = theme.function;
    }
    else if (match[6].matched) {
      token_color = theme.number;
    }

    add_text(match.str(), token_color);
    last_match_end = static_cast<size_t>(match.position() + match.length());
  }

  // Remaining text
  add_text(code.substr(last_match_end), theme.fg);

  // Handle cursor at the very end of code
  if (is_focused && current_pos == cursor_pos) {
    current_line.push_back(text(" ") | inverted | color(theme.accent) | focus);
  }

  if (current_line.empty()) {
    current_line.push_back(text(" "));
  }
  lines.push_back(hbox(std::move(current_line)));

  return vbox(std::move(lines));
}

class EditorComponent : public ComponentBase
{
public:
  EditorComponent(std::string* content, const style::Theme& theme)
      : content_(content), theme_(theme)
  {
    InputOption option;
    option.multiline = true;
    option.cursor_position = &cursor_pos_;
    option.transform = [&](InputState state) {
      // Important: Pass the focus state and real cursor position
      return HighlightCode(*content_, theme_, cursor_pos_, state.focused) | bgcolor(theme_.bg);
    };

    input_ = Input(content_, "Write your Gecko code here...", option);
    Add(input_);
  }

  Element Render() override
  {
    Elements gutter;
    int line_count = 1;
    for (char c : *content_) {
      if (c == '\n') {
        line_count++;
      }
    }

    for (int i = 1; i <= line_count; ++i) {
      gutter.push_back(text(" " + std::to_string(i) + " ") | color(theme_.gutter_fg));
    }

    // Important: We don't add 'frame' here because multiline Input already has an internal frame.
    // Adding another frame breaks arrow navigation and scroll.
    return hbox({vbox(std::move(gutter)) | bgcolor(theme_.gutter_bg),
                 separator(),
                 input_->Render() | flex})
           | vscroll_indicator | frame | flex | border | bgcolor(theme_.bg) | color(theme_.fg);
  }

  void Clear()
  {
    content_->clear();
    cursor_pos_ = 0;
    input_->OnEvent(Event::Custom);
  }

  bool OnEvent(Event event) override
  {
    // Custom Home/End for current line
    if (event == Event::Home) {
      int pos = cursor_pos_;
      while (pos > 0 && (*content_)[static_cast<size_t>(pos) - 1] != '\n') {
        pos--;
      }
      cursor_pos_ = pos;
      return true;
    }
    if (event == Event::End) {
      int pos = cursor_pos_;
      while (pos < static_cast<int>(content_->size())
             && (*content_)[static_cast<size_t>(pos)] != '\n') {
        pos++;
      }
      cursor_pos_ = pos;
      return true;
    }

    if (event == Event::PageUp) {
      cursor_pos_ = 0;
      return true;
    }
    if (event == Event::PageDown) {
      cursor_pos_ = static_cast<int>(content_->size());
      return true;
    }

    // Alt+Arrow Up/Down to move lines
    if (event == Event::Special("\x1b[1;3A") || event == Event::Special("\x1b\x1b[A")) {
      MoveLine(true);
      return true;
    }
    if (event == Event::Special("\x1b[1;3B") || event == Event::Special("\x1b\x1b[B")) {
      MoveLine(false);
      return true;
    }

    if (event == Event::Character('\x1f')) {
      ToggleComment();
      return true;
    }

    if (event == Event::Tab) {
      for (int i = 0; i < 2; ++i) {
        input_->OnEvent(Event::Character(' '));
      }
      return true;
    }

    if (event == Event::Return) {
      // Let Input process Enter first
      input_->OnEvent(Event::Return);

      // Calculate indentation from previous line
      int pos = cursor_pos_;
      if (pos > 0) {
        size_t search = static_cast<size_t>(pos) - 1;
        if (search < content_->size() && (*content_)[search] == '\n') {
          // Go back to end of previous line to check content
          size_t prev_line_end = (search > 0) ? search - 1 : 0;
          size_t line_start = prev_line_end;
          while (line_start > 0 && (*content_)[line_start - 1] != '\n') {
            line_start--;
          }

          std::string indent = "";
          for (size_t i = line_start; i <= prev_line_end && (*content_)[i] == ' '; ++i) {
            indent += ' ';
          }

          // Extra indent after {
          if (prev_line_end < content_->size() && (*content_)[prev_line_end] == '{') {
            indent += "  ";
          }

          for (char c : indent) {
            input_->OnEvent(Event::Character(c));
          }
        }
      }
      return true;
    }

    // Crucial: Let Input handle arrows, Home, End, and Backspace
    return input_->OnEvent(event);
  }

  void SetTheme(const style::Theme& theme)
  {
    theme_ = theme;
  }

  style::Theme GetTheme() const
  {
    return theme_;
  }

private:
  std::string* content_;
  Component input_;
  style::Theme theme_;
  int cursor_pos_ = 0;

  void ToggleComment()
  {
    if (content_->empty()) {
      return;
    }

    int pos = cursor_pos_;
    size_t line_start = static_cast<size_t>(pos);
    while (line_start > 0 && (*content_)[line_start - 1] != '\n') {
      line_start--;
    }

    size_t content_start = line_start;
    while (content_start < content_->size() && (*content_)[content_start] == ' ') {
      content_start++;
    }

    if (content_start + 1 < content_->size() && (*content_)[content_start] == '/'
        && (*content_)[content_start + 1] == '/') {
      // Uncomment
      size_t remove_len = 2;
      if (content_start + 2 < content_->size() && (*content_)[content_start + 2] == ' ') {
        remove_len = 3;
      }
      content_->erase(content_start, remove_len);
      cursor_pos_ = std::max(static_cast<int>(line_start), pos - static_cast<int>(remove_len));
    }
    else {
      // Comment
      content_->insert(content_start, "// ");
      cursor_pos_ = pos + 3;
    }
    input_->OnEvent(Event::Custom);
  }

  void MoveLine(bool up)
  {
    if (content_->empty()) {
      return;
    }

    int pos = cursor_pos_;
    int start = pos;
    while (start > 0 && (*content_)[static_cast<size_t>(start) - 1] != '\n') {
      start--;
    }

    int end = pos;
    while (end < static_cast<int>(content_->size())
           && (*content_)[static_cast<size_t>(end)] != '\n') {
      end++;
    }

    std::string current_line =
        content_->substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
    int line_len = static_cast<int>(current_line.length());

    if (up) {
      if (start == 0) {
        return;
      }
      int prev_start = start - 1;
      while (prev_start > 0 && (*content_)[static_cast<size_t>(prev_start) - 1] != '\n') {
        prev_start--;
      }

      content_->erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
      if (start < static_cast<int>(content_->size())
          && (*content_)[static_cast<size_t>(start)] == '\n') {
        content_->erase(static_cast<size_t>(start), 1);
      }
      else if (start > 0) {
        content_->erase(static_cast<size_t>(start) - 1, 1);
        start--;
      }

      content_->insert(static_cast<size_t>(prev_start), current_line + "\n");
      cursor_pos_ = prev_start + (pos - start);
    }
    else {
      if (end >= static_cast<int>(content_->size())) {
        return;
      }
      int next_end = end + 1;
      while (next_end < static_cast<int>(content_->size())
             && (*content_)[static_cast<size_t>(next_end)] != '\n') {
        next_end++;
      }

      content_->erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
      if (start < static_cast<int>(content_->size())
          && (*content_)[static_cast<size_t>(start)] == '\n') {
        content_->erase(static_cast<size_t>(start), 1);
      }

      size_t next_nl = content_->find('\n', static_cast<size_t>(start));
      if (next_nl == std::string::npos) {
        content_->append("\n" + current_line);
        cursor_pos_ = static_cast<int>(content_->size()) - line_len + (pos - start);
      }
      else {
        content_->insert(next_nl + 1, current_line + "\n");
        cursor_pos_ = static_cast<int>(next_nl) + 1 + (pos - start);
      }
    }
  }
};

inline Component CreateEditor(std::string* content, const style::Theme& theme)
{
  return Make<EditorComponent>(content, theme);
}

} // namespace repl::components
