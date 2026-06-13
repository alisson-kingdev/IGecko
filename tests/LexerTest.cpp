#include <catch2/catch_test_macros.hpp>
#include "../src/lexer/Lexer.h"

TEST_CASE("Lexer can tokenize simple expressions", "[lexer]") {
    std::string source = "var x = 10;";
    Lexer lexer(source, "test.gk");
    auto tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 6); // var, identifier(x), assign, number(10), semicolon, EOF
    REQUIRE(tokens[0].type == TokenType::VAR);
    REQUIRE(tokens[1].type == TokenType::IDENTIFIER);
    REQUIRE(tokens[1].lexeme == "x");
    REQUIRE(tokens[2].type == TokenType::ASSIGN);
    REQUIRE(tokens[3].type == TokenType::NUMBER);
    REQUIRE(tokens[4].type == TokenType::SEMICOLON);
    REQUIRE(tokens[5].type == TokenType::EOF_TOKEN);
}
