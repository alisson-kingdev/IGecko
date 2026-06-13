#include <catch2/catch_test_macros.hpp>
#include "../src/parser/Parser.h"
#include "../src/lexer/Lexer.h"

TEST_CASE("Parser can parse a simple variable declaration", "[parser]") {
    std::string source = "var x = 10;";
    Lexer lexer(source, "test.gk");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto stmts = parser.parse();

    REQUIRE(stmts.size() == 1);
    // Assuming the first statement is a variable declaration
    // This requires knowing the structure of your Stmt/VarDeclNode class
    // Depending on the implementation, you might need to cast stmts[0]
}
