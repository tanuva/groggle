#define CATCH_CONFIG_MAIN
#include "catch2/catch_amalgamated.hpp"

#include "color.h"

TEST_CASE("Color black", "[color]")
{
    groggle::Color color;
    REQUIRE(color.h() == 0);
    REQUIRE(color.s() == 0);
    REQUIRE(color.v() == 0);
    REQUIRE(color.r() == 0);
    REQUIRE(color.g() == 0);
    REQUIRE(color.b() == 0);
}

TEST_CASE( "Color red", "[color]")
{
    groggle::Color color(0, 1, 1);
    REQUIRE(color.h() == 0);
    REQUIRE(color.s() == 1);
    REQUIRE(color.v() == 1);
    REQUIRE(color.r() == 1);
    REQUIRE(color.g() == 0);
    REQUIRE(color.b() == 0);
}

TEST_CASE( "Color green", "[color]")
{
    groggle::Color color(120, 1, 1);
    REQUIRE(color.h() == 120);
    REQUIRE(color.s() == 1);
    REQUIRE(color.v() == 1);
    REQUIRE(color.r() == 0);
    REQUIRE(color.g() == 1);
    REQUIRE(color.b() == 0);
}

TEST_CASE( "Color blue", "[color]")
{
    groggle::Color color(240, 1, 1);
    REQUIRE(color.h() == 240);
    REQUIRE(color.s() == 1);
    REQUIRE(color.v() == 1);
    REQUIRE(color.r() == 0);
    REQUIRE(color.g() == 0);
    REQUIRE(color.b() == 1);
}
