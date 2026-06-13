#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "components/HistoryComponent.h"
#include "components/InputComponent.h"
#include "components/Theme.h"
#include "interpreter/Interpreter.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "utils/Version.h"

using namespace ftxui;

// RAII helper to redirect stdout
struct CoutRedirect
{
  std::streambuf* old_buf;

  CoutRedirect(std::streambuf* new_buf) : old_buf(std::cout.rdbuf())
  {
    std::cout.rdbuf(new_buf);
  }

  ~CoutRedirect()
  {
    std::cout.rdbuf(old_buf);
  }
};

class Repl
{
public:
  Repl()
      : output_buffer(std::make_unique<std::stringstream>())
      , interpreter(std::make_unique<Interpreter>("<repl>", output_buffer.get()))
      , current_theme(repl::style::Dracula())
  {}

  void run()
  {
    auto screen = ScreenInteractive::Fullscreen();
    // Clear terminal (ANSI escape sequence)
    std::cout << "\033[2J\033[1;1H" << std::flush;

    std::string content;
    std::vector<std::string> output_history;
    bool show_help = true;

    auto editor = repl::components::CreateEditor(&content, current_theme);

    auto component = Container::Vertical({
        editor,
    });

    auto editor_comp = std::dynamic_pointer_cast<repl::components::EditorComponent>(editor);

    component = CatchEvent(component, [&](Event event) {
      // Toggle theme
      if (event == Event::F1 || event == Event::Character('\x14')) {
        use_dracula = !use_dracula;
        current_theme = use_dracula ? repl::style::Dracula() : repl::style::GitHub();
        if (editor_comp) {
          editor_comp->SetTheme(current_theme);
        }
        screen.PostEvent(Event::Custom);
        return true;
      }

      // Run current editor content
      if (event == Event::F5 || event == Event::Character('\x12')) {
        if (!content.empty()) {
          show_help = false;
          output_history.push_back(">> Running...");
          screen.PostEvent(Event::Custom);

          try {
            processInput(content, output_history);
          }
          catch (const std::exception& e) {
            output_history.push_back("Error: " + std::string(e.what()));
          }
        }
        return true;
      }

      // Clear editor content
      if (event == Event::Character('\x0c')) {
        if (editor_comp) {
          editor_comp->Clear();
        }
        screen.PostEvent(Event::Custom);
        return true;
      }

      // Exit REPL
      if (event == Event::Special("\x1b") || event == Event::Escape
          || event == Event::Character('\x11')) {
        exit(0);
      }

      return false;
    });

    auto renderer = Renderer(component, [&] {
      // Status bar updated with ^L instruction
      auto status_bar =
          hbox({
              text(" gecko ") | bold | bgcolor(current_theme.accent) | color(Color::Black),
              text(" main ") | bgcolor(Color::RGB(68, 71, 90)) | color(Color::White),
              text(" " + gecko::getVersion() + " ") | bgcolor(Color::RGB(40, 42, 54))
                  | color(Color::White),
              text(" ") | flex | bgcolor(current_theme.status_bg),
              text(" " + current_theme.name + " ") | bgcolor(current_theme.status_bg)
                  | color(current_theme.status_fg),
              text(" ^R:Run ^L:Clear ^Q:Exit ^/:Comment ^T:Theme ") | bgcolor(current_theme.accent)
                  | color(Color::Black) | bold,
          })
          | size(HEIGHT, EQUAL, 1);

      Element main_view;
      if (output_history.empty() && content.empty() && show_help) {
        main_view =
            vbox({
                text("Welcome to " + gecko::getProjectName() + " REPL!") | bold
                    | color(current_theme.accent),
                text("Using " + gecko::getInterpreterName() + " version " + gecko::getVersion()),
                text("Type your code in the editor above."),
                text("Shortcuts: [F5/CTRL+R] Run, [CTRL+L] Clear Editor, [F1/CTRL+T] Theme"),
                separator(),
                text("Example:"),
                text("  print(\"Hello World\");") | color(current_theme.string),
            })
            | borderStyled(ROUNDED) | flex | bgcolor(current_theme.bg) | color(current_theme.fg);
      }
      else {
        Elements history_elements;
        for (const auto& line : output_history) {
          history_elements.push_back(text(line));
        }
        main_view = vbox(std::move(history_elements)) | vscroll_indicator | frame | flex
                    | borderStyled(ROUNDED) | bgcolor(current_theme.bg) | color(current_theme.fg);
      }

      return vbox({editor->Render() | size(HEIGHT, EQUAL, 15), main_view, status_bar})
             | bgcolor(current_theme.bg) | color(current_theme.fg);
    });

    screen.Loop(renderer);
  }

private:
  std::unique_ptr<std::stringstream> output_buffer;
  std::unique_ptr<Interpreter> interpreter;
  repl::style::Theme current_theme;
  bool use_dracula = true;

  void processInput(const std::string& input, std::vector<std::string>& history)
  {
    if (input == ".exit" || input == "quit") {
      exit(0);
    }

    try {
      Lexer lexer(input, "<repl>");
      auto tokens = lexer.tokenize();
      Parser parser(tokens);
      auto stmts = parser.parse();

      output_buffer->str(""); // Clear old buffer

      // --- REDIRECTION TRICK (RAII) ---
      CoutRedirect redirect(output_buffer->rdbuf());

      // Execute the interpreter (now any print goes to output_buffer)
      interpreter->interpret(stmts);

      // Capture the generated result
      std::string output = output_buffer->str();

      if (!output.empty()) {
        // If the interpreter generated multiple lines, we break and add to history
        std::stringstream ss(output);
        std::string line;
        while (std::getline(ss, line)) {
          history.push_back(line);
        }
      }
      else {
        history.push_back(">> Code executed successfully (No output).");
      }
    }
    catch (const std::exception& e) {
      history.push_back(">> Interpret Error: " + std::string(e.what()));
    }
  }
};
