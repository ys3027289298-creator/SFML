#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Glyph.hpp>
#include <SFML/Graphics/Text.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <GraphicsUtil.hpp>
#include <WindowUtil.hpp>
#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include <cstdint>

namespace
{
// Pack a four-character axis label the same way sf::Font stores tags
std::uint32_t makeTag(std::string_view label)
{
    std::uint32_t tag = 0;
    for (char character : label)
        tag = (tag << 8) | static_cast<unsigned char>(character);
    return tag;
}

const std::uint32_t weightTag = makeTag("wght");
const std::uint32_t widthTag  = makeTag("wdth");
const std::uint32_t slantTag  = makeTag("csln");
} // namespace


TEST_CASE("[Graphics] sf::Font variable fonts", runDisplayTests())
{
    SECTION("Axis enumeration")
    {
        const sf::Font font("sfml-variable.ttf");

        REQUIRE(font.isVariableFont());

        const auto& axes = font.getVariableAxes();
        REQUIRE(axes.size() == 3);

        const auto findAxis = [&](std::uint32_t tag) -> const sf::Font::VariableAxis&
        {
            const auto it = std::find_if(axes.begin(),
                                         axes.end(),
                                         [tag](const sf::Font::VariableAxis& axis) { return axis.tag == tag; });
            REQUIRE(it != axes.end());
            return *it;
        };

        const auto& weight = findAxis(weightTag);
        CHECK(weight.name == "Weight");
        CHECK(weight.minimum == Approx(100.f));
        CHECK(weight.defaultValue == Approx(400.f));
        CHECK(weight.maximum == Approx(900.f));

        const auto& width = findAxis(widthTag);
        CHECK(width.name == "Width");
        CHECK(width.minimum == Approx(75.f));
        CHECK(width.defaultValue == Approx(100.f));
        CHECK(width.maximum == Approx(125.f));

        const auto& slant = findAxis(slantTag);
        CHECK(slant.name == "csln");
        CHECK(slant.minimum == Approx(0.f));
        CHECK(slant.defaultValue == Approx(0.f));
        CHECK(slant.maximum == Approx(12.f));

        SECTION("Named instances")
        {
            const auto& instances = font.getNamedInstances();
            REQUIRE(instances.size() == 3);

            const auto findInstance = [&](std::string_view name) -> const sf::Font::NamedInstance&
            {
                const auto it = std::find_if(instances.begin(),
                                             instances.end(),
                                             [name](const sf::Font::NamedInstance& instance)
                                             { return instance.name == name; });
                REQUIRE(it != instances.end());
                return *it;
            };

            const auto& regular = findInstance("Regular");
            CHECK(regular.coordinates.size() == 3);

            const auto& bold       = findInstance("Bold");
            const auto  boldWeight = std::find_if(bold.coordinates.begin(),
                                                 bold.coordinates.end(),
                                                 [](const auto& pair) { return pair.first == weightTag; });
            REQUIRE(boldWeight != bold.coordinates.end());
            CHECK(boldWeight->second == Approx(800.f));

            const auto& italic      = findInstance("Condensed Italic");
            const auto  italicWidth = std::find_if(italic.coordinates.begin(),
                                                  italic.coordinates.end(),
                                                  [](const auto& pair) { return pair.first == widthTag; });
            REQUIRE(italicWidth != italic.coordinates.end());
            CHECK(italicWidth->second == Approx(75.f));
        }
    }

    SECTION("Static font fallback")
    {
        sf::Font font("sfml-static.ttf");

        CHECK(!font.isVariableFont());
        CHECK(font.getVariableAxes().empty());
        CHECK(font.getNamedInstances().empty());

        // Every mutating call must fail without changing the font output
        CHECK(!font.setVariableAxisValue("wght", 700.f));
        CHECK(!font.setVariableAxisValue(weightTag, 700.f));
        CHECK(!font.selectNamedInstance("Bold"));
        CHECK(!font.getVariableAxisValue("wght").has_value());

        font.resetVariableAxes(); // must be a safe no-op
        const auto& glyph = font.getGlyph(U'A', 24, false);
        CHECK(glyph.advance > 0);
    }

    SECTION("Invalid axis labels and out of range values")
    {
        sf::Font font("sfml-variable.ttf");

        CHECK(!font.setVariableAxisValue("wg", 500.f));           // too short
        CHECK(!font.setVariableAxisValue("weight", 500.f));       // too long
        CHECK(!font.setVariableAxisValue("wg\x01t", 500.f));      // non-printable
        CHECK(!font.setVariableAxisValue(makeTag("opsz"), 14.f)); // unknown axis
        CHECK(!font.getVariableAxisValue("nope").has_value());

        // Out-of-range values are clamped instead of rejected
        CHECK(font.setVariableAxisValue("wght", 9999.f));
        const auto heavy = font.getVariableAxisValue("wght");
        REQUIRE(heavy.has_value());
        CHECK(*heavy == Approx(900.f));

        CHECK(font.setVariableAxisValue(weightTag, -50.f));
        const auto light = font.getVariableAxisValue("wght");
        REQUIRE(light.has_value());
        CHECK(*light == Approx(100.f));
    }

    SECTION("Axis values affect glyphs and metrics")
    {
        sf::Font font("sfml-variable.ttf");

        const auto defaultGlyph       = font.getGlyph(U'A', 32, false);
        const auto defaultLineSpacing = font.getLineSpacing(32);

        SECTION("Design coordinates")
        {
            REQUIRE(font.setVariableAxisValue("wght", 900.f));
            REQUIRE(font.setVariableAxisValue("wdth", 125.f));

            const auto heavy = font.getVariableAxisValue("wght");
            REQUIRE(heavy.has_value());
            CHECK(*heavy == Approx(900.f));

            const auto heavyGlyph = font.getGlyph(U'A', 32, false);
            CHECK(heavyGlyph.advance != defaultGlyph.advance);
            CHECK(heavyGlyph.textureRect != defaultGlyph.textureRect);

            // The MVAR table makes vertical metrics vary as well
            CHECK(font.getLineSpacing(32) != defaultLineSpacing);
            CHECK(font.getAscent(32) > 0.f);
            CHECK(font.getDescent(32) < 0.f);

            // Kerning remains available and returns a finite value
            CHECK(font.getKerning(U'A', U'V', 32) == font.getKerning(U'A', U'V', 32));
        }

        SECTION("Named instance selection")
        {
            REQUIRE(font.selectNamedInstance("Bold"));
            const auto boldWeight = font.getVariableAxisValue("wght");
            REQUIRE(boldWeight.has_value());
            CHECK(*boldWeight == Approx(800.f));

            const auto boldGlyph = font.getGlyph(U'A', 32, false);
            CHECK(boldGlyph.advance != defaultGlyph.advance);

            // Unknown names are rejected without disturbing the current state
            CHECK(!font.selectNamedInstance("Does Not Exist"));
            CHECK(font.getVariableAxisValue("wght") == boldWeight);

            // The Condensed Italic instance changes both width and slant
            REQUIRE(font.selectNamedInstance("Condensed Italic"));
            const auto italicWidth = font.getVariableAxisValue("wdth");
            REQUIRE(italicWidth.has_value());
            CHECK(*italicWidth == Approx(75.f));
            const auto italicSlant = font.getVariableAxisValue("csln");
            REQUIRE(italicSlant.has_value());
            CHECK(*italicSlant == Approx(10.f));
        }

        SECTION("Reset restores the default instance")
        {
            REQUIRE(font.setVariableAxisValue("wght", 900.f));
            REQUIRE(font.setVariableAxisValue("wdth", 125.f));
            const auto changedGlyph = font.getGlyph(U'A', 32, false);
            REQUIRE(changedGlyph.advance != defaultGlyph.advance);

            font.resetVariableAxes();
            CHECK(font.getVariableAxisValues().empty());

            const auto restoredGlyph = font.getGlyph(U'A', 32, false);
            CHECK(restoredGlyph.advance == defaultGlyph.advance);
            CHECK(restoredGlyph.textureRect == defaultGlyph.textureRect);
            CHECK(font.getLineSpacing(32) == defaultLineSpacing);
        }
    }

    SECTION("Cache isolation across axis values")
    {
        sf::Font font("sfml-variable.ttf");

        // Populate the default-instance page
        const auto& defaultGlyph = font.getGlyph(U'A', 32, false);
        const auto  defaultRect  = defaultGlyph.textureRect;

        // Switching axes must produce a distinct page without rebuilding
        // the glyphs that were already cached for the default instance
        REQUIRE(font.setVariableAxisValue("wght", 900.f));
        const auto& heavyGlyph = font.getGlyph(U'A', 32, false);
        CHECK(heavyGlyph.textureRect != defaultRect);

        // Going back reuses the original page and its texture coordinates
        font.resetVariableAxes();
        const auto& restoredGlyph = font.getGlyph(U'A', 32, false);
        CHECK(restoredGlyph.textureRect == defaultRect);

        // The cache key also covers the character size, bold and outline
        const auto& boldGlyph = font.getGlyph(U'A', 32, true);
        CHECK(boldGlyph.textureRect != defaultRect);
        const auto& outlineGlyph = font.getGlyph(U'A', 32, false, 1.5f);
        CHECK(outlineGlyph.textureRect != defaultRect);
        const auto& otherSizeGlyph = font.getGlyph(U'A', 40, false);
        CHECK(otherSizeGlyph.textureRect != defaultRect);

        // Re-requesting an already cached combination returns the same glyph
        const auto& cachedAgain = font.getGlyph(U'A', 32, false);
        CHECK(cachedAgain.textureRect == defaultRect);
    }

    SECTION("HarfBuzz shaping responds to axes")
    {
        sf::Font font("sfml-variable.ttf");

        sf::Text regularText(font, "AVAH");
        regularText.setCharacterSize(32);
        const sf::FloatRect regularBounds = regularText.getLocalBounds();

        REQUIRE(font.setVariableAxisValue("wght", 900.f));
        REQUIRE(font.setVariableAxisValue("wdth", 125.f));

        sf::Text heavyText(font, "AVAH");
        heavyText.setCharacterSize(32);
        const sf::FloatRect heavyBounds = heavyText.getLocalBounds();

        // Wider axes must widen the shaped string
        CHECK(heavyBounds.size.x > regularBounds.size.x);

        // After a reset the shaped output matches the default instance again
        font.resetVariableAxes();
        sf::Text resetText(font, "AVAH");
        resetText.setCharacterSize(32);
        CHECK(resetText.getLocalBounds().size.x == Approx(regularBounds.size.x));
    }

    SECTION("Unloaded font behaves safely")
    {
        sf::Font font;
        CHECK(!font.isVariableFont());
        CHECK(!font.setVariableAxisValue("wght", 400.f));
        CHECK(!font.selectNamedInstance("Regular"));
        CHECK(!font.getVariableAxisValue("wght").has_value());
        font.resetVariableAxes();
        CHECK(font.getVariableAxisValues().empty());
    }
}
