#pragma once
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <string>

namespace repl::style
{

struct Theme
{
  std::string name;
  ftxui::Color bg;
  ftxui::Color fg;
  ftxui::Color gutter_bg;
  ftxui::Color gutter_fg;
  ftxui::Color keyword;
  ftxui::Color string;
  ftxui::Color number;
  ftxui::Color function;
  ftxui::Color type;
  ftxui::Color comment;
  ftxui::Color status_bg;
  ftxui::Color status_fg;
  ftxui::Color accent;
};

inline Theme Dracula()
{
  return {.name = "Dracula",
          .bg = ftxui::Color::RGB(40, 42, 54),
          .fg = ftxui::Color::RGB(248, 248, 242),
          .gutter_bg = ftxui::Color::RGB(40, 42, 54),
          .gutter_fg = ftxui::Color::RGB(98, 114, 164),
          .keyword = ftxui::Color::RGB(255, 121, 198),
          .string = ftxui::Color::RGB(241, 250, 140),
          .number = ftxui::Color::RGB(189, 147, 249),
          .function = ftxui::Color::RGB(80, 250, 123),
          .type = ftxui::Color::RGB(139, 233, 253),
          .comment = ftxui::Color::RGB(98, 114, 164),
          .status_bg = ftxui::Color::RGB(68, 71, 90),
          .status_fg = ftxui::Color::RGB(139, 233, 253),
          .accent = ftxui::Color::RGB(80, 250, 123)};
}

inline Theme GitHub()
{
  return {.name = "GitHub",
          .bg = ftxui::Color::RGB(255, 255, 255),
          .fg = ftxui::Color::RGB(36, 41, 46),
          .gutter_bg = ftxui::Color::RGB(246, 248, 250),
          .gutter_fg = ftxui::Color::RGB(175, 184, 193),
          .keyword = ftxui::Color::RGB(215, 58, 73),
          .string = ftxui::Color::RGB(3, 47, 98),
          .number = ftxui::Color::RGB(0, 92, 197),
          .function = ftxui::Color::RGB(111, 66, 193),
          .type = ftxui::Color::RGB(36, 41, 46),
          .comment = ftxui::Color::RGB(106, 115, 125),
          .status_bg = ftxui::Color::RGB(36, 41, 46),
          .status_fg = ftxui::Color::RGB(255, 255, 255),
          .accent = ftxui::Color::RGB(40, 167, 69)};
}

} // namespace repl::style
