#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace repl::components
{
inline ftxui::Element RenderHistory(const std::vector<std::string>& history)
{
  ftxui::Elements elements;
  for (size_t i = 0; i < history.size(); ++i) {
    elements.push_back(ftxui::hbox({
        ftxui::text(" " + std::to_string(i + 1) + " │ ") | ftxui::color(ftxui::Color::GrayDark),
        ftxui::text(history[i]),
    }));
  }
  return ftxui::vbox(std::move(elements));
}
} // namespace repl::components
