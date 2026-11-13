////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2026 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Export.hpp>

#include <SFML/Graphics/Glyph.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <SFML/System/Vector2.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cstddef>
#include <cstdint>


namespace sf
{
class InputStream;

////////////////////////////////////////////////////////////
/// \brief Class for loading and manipulating character fonts
///
////////////////////////////////////////////////////////////
class SFML_GRAPHICS_API Font
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Holds information about a variation axis of a variable font
    ///
    /// A variation axis is identified by a four-character tag such as
    /// `wght` (weight), `wdth` (width) or `slnt` (slant). The minimum,
    /// default and maximum values are design coordinates expressed as
    /// plain floats, matching the values passed to
    /// `setVariableAxisValue`.
    ///
    ////////////////////////////////////////////////////////////
    struct VariableAxis
    {
        std::uint32_t tag{};          //!< Four-character axis tag (packed as a 32-bit integer)
        std::string   name;           //!< Human readable name of the axis
        float         minimum{};      //!< Minimum design coordinate of the axis
        float         maximum{};      //!< Maximum design coordinate of the axis
        float         defaultValue{}; //!< Default design coordinate of the axis
    };

    ////////////////////////////////////////////////////////////
    /// \brief Holds information about a named instance of a variable font
    ///
    ////////////////////////////////////////////////////////////
    struct NamedInstance
    {
        std::string                                  name;        //!< Name of the instance (e.g. "Bold Italic")
        std::vector<std::pair<std::uint32_t, float>> coordinates; //!< Axis tag to design coordinate mapping
    };

    ////////////////////////////////////////////////////////////
    /// \brief Holds various information about a font
    ///
    ////////////////////////////////////////////////////////////
    struct Info
    {
        std::uint64_t id{};                 //!< A unique ID that identifies the font
        std::string   family;               //!< The font family
        bool          hasKerning{};         //!< Has kerning information
        bool          hasVerticalMetrics{}; //!< Has native vertical metrics
    };

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Construct an empty font that does not contain any glyphs.
    ///
    ////////////////////////////////////////////////////////////
    Font() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Construct the font from a file
    ///
    /// The supported font formats are: TrueType, Type 1, CFF,
    /// OpenType, SFNT, X11 PCF, Windows FNT, BDF, PFR and Type 42.
    /// Note that this function knows nothing about the standard
    /// fonts installed on the user's system, thus you can't
    /// load them directly.
    ///
    /// \warning SFML cannot preload all the font data in this
    /// function, so the file has to remain accessible until
    /// the `sf::Font` object opens a new font or is destroyed.
    ///
    /// \param filename Path of the font file to open
    ///
    /// \throws sf::Exception if opening was unsuccessful
    ///
    /// \see `openFromFile`, `openFromMemory`, `openFromStream`
    ///
    ////////////////////////////////////////////////////////////
    explicit Font(const std::filesystem::path& filename);

    ////////////////////////////////////////////////////////////
    /// \brief Construct the font from a file in memory
    ///
    /// The supported font formats are: TrueType, Type 1, CFF,
    /// OpenType, SFNT, X11 PCF, Windows FNT, BDF, PFR and Type 42.
    ///
    /// \warning SFML cannot preload all the font data in this
    /// function, so the buffer pointed by `data` has to remain
    /// valid until the `sf::Font` object opens a new font or
    /// is destroyed.
    ///
    /// \param data        Pointer to the file data in memory
    /// \param sizeInBytes Size of the data to load, in bytes
    ///
    /// \throws sf::Exception if loading was unsuccessful
    ///
    /// \see `openFromFile`, `openFromMemory`, `openFromStream`
    ///
    ////////////////////////////////////////////////////////////
    Font(const void* data, std::size_t sizeInBytes);

    ////////////////////////////////////////////////////////////
    /// \brief Construct the font from a custom stream
    ///
    /// The supported font formats are: TrueType, Type 1, CFF,
    /// OpenType, SFNT, X11 PCF, Windows FNT, BDF, PFR and Type 42.
    /// Warning: SFML cannot preload all the font data in this
    /// function, so the contents of `stream` have to remain
    /// valid as long as the font is used.
    ///
    /// \warning SFML cannot preload all the font data in this
    /// function, so the stream has to remain accessible until
    /// the `sf::Font` object opens a new font or is destroyed.
    ///
    /// \param stream Source stream to read from
    ///
    /// \throws sf::Exception if loading was unsuccessful
    ///
    /// \see `openFromFile`, `openFromMemory`, `openFromStream`
    ///
    ////////////////////////////////////////////////////////////
    explicit Font(InputStream& stream);

    ////////////////////////////////////////////////////////////
    /// \brief Open the font from a file
    ///
    /// The supported font formats are: TrueType, Type 1, CFF,
    /// OpenType, SFNT, X11 PCF, Windows FNT, BDF, PFR and Type 42.
    /// Note that this function knows nothing about the standard
    /// fonts installed on the user's system, thus you can't
    /// load them directly.
    ///
    /// \warning SFML cannot preload all the font data in this
    /// function, so the file has to remain accessible until
    /// the `sf::Font` object opens a new font or is destroyed.
    ///
    /// \param filename Path of the font file to load
    ///
    /// \return `true` if opening succeeded, `false` if it failed
    ///
    /// \see `openFromMemory`, `openFromStream`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool openFromFile(const std::filesystem::path& filename);

    ////////////////////////////////////////////////////////////
    /// \brief Open the font from a file in memory
    ///
    /// The supported font formats are: TrueType, Type 1, CFF,
    /// OpenType, SFNT, X11 PCF, Windows FNT, BDF, PFR and Type 42.
    ///
    /// \warning SFML cannot preload all the font data in this
    /// function, so the buffer pointed by `data` has to remain
    /// valid until the `sf::Font` object opens a new font or
    /// is destroyed.
    ///
    /// \param data        Pointer to the file data in memory
    /// \param sizeInBytes Size of the data to load, in bytes
    ///
    /// \return `true` if opening succeeded, `false` if it failed
    ///
    /// \see `openFromFile`, `openFromStream`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool openFromMemory(const void* data, std::size_t sizeInBytes);

    ////////////////////////////////////////////////////////////
    /// \brief Open the font from a custom stream
    ///
    /// The supported font formats are: TrueType, Type 1, CFF,
    /// OpenType, SFNT, X11 PCF, Windows FNT, BDF, PFR and Type 42.
    ///
    /// \warning SFML cannot preload all the font data in this
    /// function, so the stream has to remain accessible until
    /// the `sf::Font` object opens a new font or is destroyed.
    ///
    /// \param stream Source stream to read from
    ///
    /// \return `true` if opening succeeded, `false` if it failed
    ///
    /// \see `openFromFile`, `openFromMemory`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool openFromStream(InputStream& stream);

    ////////////////////////////////////////////////////////////
    /// \brief Get the font information
    ///
    /// \return A structure that holds the font information
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const Info& getInfo() const;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether or not the font is a variable font
    ///
    /// Variable fonts (TrueType GX or OpenType variation fonts) expose
    /// one or more variation axes that can be adjusted through
    /// `setVariableAxisValue`. Static fonts and bitmap fonts always
    /// return `false`.
    ///
    /// \return `true` if the font has at least one variation axis
    ///
    /// \see `getVariableAxes`, `setVariableAxisValue`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isVariableFont() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the variation axes exposed by this font
    ///
    /// The returned vector is empty for fonts that are not variable.
    /// Each axis reports its tag, name and the range of valid design
    /// coordinates.
    ///
    /// \return The variation axes of the font
    ///
    /// \see `isVariableFont`, `getNamedInstances`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const std::vector<VariableAxis>& getVariableAxes() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the named instances stored in this font
    ///
    /// A named instance associates a name (such as "Condensed Bold")
    /// with a full set of axis coordinates. The returned vector is
    /// empty for fonts that are not variable or that define no named
    /// instances.
    ///
    /// \return The named instances of the font
    ///
    /// \see `selectNamedInstance`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const std::vector<NamedInstance>& getNamedInstances() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the value of a variation axis using a tag string
    ///
    /// The axis label must be a four-character OpenType axis tag such
    /// as "wght", "wdth" or "slnt". The value is a design coordinate
    /// and gets clamped to the range reported by `getVariableAxes`.
    /// Setting an axis only affects glyphs, metrics and textures that
    /// are requested afterwards; glyphs cached for other axis values
    /// are kept intact.
    ///
    /// \param tag   Four-character axis tag
    /// \param value Design coordinate to apply
    ///
    /// \return `true` if the axis exists and the value was applied,
    ///         `false` if the font is not loaded, is not variable,
    ///         the tag is invalid or the axis does not exist
    ///
    /// \see `setVariableAxisValue(std::uint32_t, float)`,
    ///      `resetVariableAxes`, `selectNamedInstance`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool setVariableAxisValue(std::string_view tag, float value);

    ////////////////////////////////////////////////////////////
    /// \brief Set the value of a variation axis using a packed tag
    ///
    /// \param tag   Four-character axis tag packed as a 32-bit integer
    /// \param value Design coordinate to apply (clamped to the axis range)
    ///
    /// \return `true` if the axis exists and the value was applied,
    ///         `false` if the font is not loaded, is not variable
    ///         or the axis does not exist
    ///
    /// \see `setVariableAxisValue(std::string_view, float)`,
    ///      `resetVariableAxes`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool setVariableAxisValue(std::uint32_t tag, float value);

    ////////////////////////////////////////////////////////////
    /// \brief Get the value currently assigned to a variation axis
    ///
    /// \param tag Four-character axis tag
    ///
    /// \return The current design coordinate, or `std::nullopt` if
    ///         the tag is invalid or the axis does not exist
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::optional<float> getVariableAxisValue(std::string_view tag) const;

    ////////////////////////////////////////////////////////////
    /// \brief Select a named instance by its name
    ///
    /// All axis coordinates stored in the named instance are applied
    /// together; coordinates of axes not covered by the instance are
    /// reset to their default values.
    ///
    /// \param name Name of the instance as reported by
    ///             `getNamedInstances`
    ///
    /// \return `true` if the instance exists and was applied,
    ///         `false` otherwise (the previous axis values remain
    ///         unchanged on failure)
    ///
    /// \see `getNamedInstances`, `setVariableAxisValue`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool selectNamedInstance(std::string_view name);

    ////////////////////////////////////////////////////////////
    /// \brief Reset every variation axis to its default value
    ///
    /// After calling this function the font behaves exactly like a
    /// static instance again. On fonts that are not variable this
    /// function has no effect.
    ///
    /// \see `setVariableAxisValue`
    ///
    ////////////////////////////////////////////////////////////
    void resetVariableAxes();

    ////////////////////////////////////////////////////////////
    /// \brief Get the variation coordinates currently applied
    ///
    /// The entries are ordered by axis tag and only contain axes that
    /// differ from their default value.
    ///
    /// \return A copy of the active axis tag/value pairs
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::vector<std::pair<std::uint32_t, float>> getVariableAxisValues() const;

    ////////////////////////////////////////////////////////////
    /// \brief Retrieve a glyph of the font by glyph ID
    ///
    /// If the font is a bitmap font, not all character sizes
    /// might be available. If the glyph is not available at the
    /// requested size, an empty glyph is returned.
    ///
    /// This function is only useful for getting the glyphs
    /// returned in the data from calling `shape`.
    ///
    /// Be aware that using a negative value for the outline
    /// thickness will cause distorted rendering.
    ///
    /// \param id               ID of the glyph to get
    /// \param characterSize    Reference character size
    /// \param bold             Retrieve the bold version or the regular one?
    /// \param outlineThickness Thickness of outline (when != 0 the glyph will not be filled)
    ///
    /// \return The glyph corresponding to `id` and `characterSize`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const Glyph& getGlyphById(std::uint32_t id,
                                            unsigned int  characterSize,
                                            bool          bold,
                                            float         outlineThickness = 0) const;

    ////////////////////////////////////////////////////////////
    /// \brief Retrieve a glyph of the font
    ///
    /// If the font is a bitmap font, not all character sizes
    /// might be available. If the glyph is not available at the
    /// requested size, an empty glyph is returned.
    ///
    /// You may want to use `hasGlyph` to determine if the
    /// glyph exists before requesting it. If the glyph does not
    /// exist, a font specific default is returned.
    ///
    /// Be aware that using a negative value for the outline
    /// thickness will cause distorted rendering.
    ///
    /// \param codePoint        Unicode code point of the character to get
    /// \param characterSize    Reference character size
    /// \param bold             Retrieve the bold version or the regular one?
    /// \param outlineThickness Thickness of outline (when != 0 the glyph will not be filled)
    ///
    /// \return The glyph corresponding to `codePoint` and `characterSize`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const Glyph& getGlyph(char32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness = 0) const;

    ////////////////////////////////////////////////////////////
    /// \brief Determine if this font has a glyph representing the requested code point
    ///
    /// Most fonts only include a very limited selection of glyphs from
    /// specific Unicode subsets, like Latin, Cyrillic, or Asian characters.
    ///
    /// While code points without representation will return a font specific
    /// default character, it might be useful to verify whether specific
    /// code points are included to determine whether a font is suited
    /// to display text in a specific language.
    ///
    /// \param codePoint Unicode code point to check
    ///
    /// \return `true` if the codepoint has a glyph representation, `false` otherwise
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool hasGlyph(char32_t codePoint) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the kerning offset of two glyphs
    ///
    /// \deprecated Use the `getKerning(char32_t, char32_t, unsigned int, bool)` overload instead.
    ///
    /// The kerning is an extra offset (negative) to apply between two
    /// glyphs when rendering them, to make the pair look more "natural".
    /// For example, the pair "AV" have a special kerning to make them
    /// closer than other characters. Most of the glyphs pairs have a
    /// kerning offset of zero, though.
    ///
    /// \param first         Unicode code point of the first character
    /// \param second        Unicode code point of the second character
    /// \param characterSize Reference character size
    /// \param bold          Retrieve the bold version or the regular one?
    ///
    /// \return Kerning value for `first` and `second`, in pixels
    ///
    ////////////////////////////////////////////////////////////
    [[deprecated("Use the getKerning(char32_t, char32_t, unsigned int, bool) overload")]] [[nodiscard]] float getKerning(
        std::uint32_t first,
        std::uint32_t second,
        unsigned int  characterSize,
        bool          bold = false) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the kerning offset of two glyphs
    ///
    /// The kerning is an extra offset (negative) to apply between two
    /// glyphs when rendering them, to make the pair look more "natural".
    /// For example, the pair "AV" have a special kerning to make them
    /// closer than other characters. Most of the glyphs pairs have a
    /// kerning offset of zero, though.
    ///
    /// \param first         Unicode code point of the first character
    /// \param second        Unicode code point of the second character
    /// \param characterSize Reference character size
    /// \param bold          Retrieve the bold version or the regular one?
    ///
    /// \return Kerning value for `first` and `second`, in pixels
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] float getKerning(char32_t first, char32_t second, unsigned int characterSize, bool bold = false) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the ascent
    ///
    /// The ascent is the largest distance between the baseline and
    /// the top of all glyphs in the font.
    ///
    /// Be aware that there is no uniform definition of how the
    /// ascent is calculated. It can vary from font to font.
    ///
    /// \param characterSize Reference character size
    ///
    /// \return Ascent, in pixels
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] float getAscent(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the descent
    ///
    /// The descent is the largest distance between the baseline and
    /// the bottom of all glyphs in the font.
    ///
    /// Be aware that there is no uniform definition of how the
    /// descent is calculated. It can vary from font to font.
    ///
    /// The descent shares the same coordinate system as the
    /// ascent. This means that it will be negative for distances
    /// below the baseline.
    ///
    /// \param characterSize Reference character size
    ///
    /// \return Descent, in pixels
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] float getDescent(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the line spacing
    ///
    /// Line spacing is the vertical offset to apply between two
    /// consecutive lines of text.
    ///
    /// \param characterSize Reference character size
    ///
    /// \return Line spacing, in pixels
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] float getLineSpacing(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the position of the underline
    ///
    /// Underline position is the vertical offset to apply between the
    /// baseline and the underline.
    ///
    /// \param characterSize Reference character size
    ///
    /// \return Underline position, in pixels
    ///
    /// \see `getUnderlineThickness`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] float getUnderlinePosition(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the thickness of the underline
    ///
    /// Underline thickness is the vertical size of the underline.
    ///
    /// \param characterSize Reference character size
    ///
    /// \return Underline thickness, in pixels
    ///
    /// \see `getUnderlinePosition`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] float getUnderlineThickness(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Retrieve the texture containing the loaded glyphs of a certain size
    ///
    /// The contents of the returned texture changes as more glyphs
    /// are requested, thus it is not very relevant. It is mainly
    /// used internally by `sf::Text`.
    ///
    /// \param characterSize Reference character size
    ///
    /// \return Texture containing the glyphs of the requested size
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const Texture& getTexture(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable the smooth filter
    ///
    /// When the filter is activated, the font appears smoother
    /// so that pixels are less noticeable. However if you want
    /// the font to look exactly the same as its source file,
    /// you should disable it.
    /// The smooth filter is enabled by default.
    ///
    /// \param smooth `true` to enable smoothing, `false` to disable it
    ///
    /// \see `isSmooth`
    ///
    ////////////////////////////////////////////////////////////
    void setSmooth(bool smooth);

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the smooth filter is enabled or not
    ///
    /// \return `true` if smoothing is enabled, `false` if it is disabled
    ///
    /// \see `setSmooth`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isSmooth() const;

private:
    friend class Text;

    ////////////////////////////////////////////////////////////
    /// \brief Structure defining a row of glyphs
    ///
    ////////////////////////////////////////////////////////////
    struct Row
    {
        Row(unsigned int rowTop, unsigned int rowHeight) : top(rowTop), height(rowHeight)
        {
        }

        unsigned int width{}; //!< Current width of the row
        unsigned int top;     //!< Y position of the row into the texture
        unsigned int height;  //!< Height of the row
    };

    ////////////////////////////////////////////////////////////
    // Types
    ////////////////////////////////////////////////////////////
    using GlyphTable = std::unordered_map<std::uint64_t, Glyph>; //!< Table mapping a codepoint to its glyph

    ////////////////////////////////////////////////////////////
    /// \brief Key identifying a set of glyph pages
    ///
    /// Glyphs cached at different character sizes or with different
    /// variation axis values must never collide, so both are part
    /// of the page key.
    ///
    ////////////////////////////////////////////////////////////
    struct PageKey
    {
        unsigned int                                 characterSize{}; //!< Reference character size
        std::vector<std::pair<std::uint32_t, float>> axisValues;      //!< Active variation axis values

        [[nodiscard]] bool operator==(const PageKey& rhs) const;
    };

    ////////////////////////////////////////////////////////////
    /// \brief Hash functor for PageKey
    ///
    ////////////////////////////////////////////////////////////
    struct PageKeyHash
    {
        [[nodiscard]] std::size_t operator()(const PageKey& key) const;
    };

    ////////////////////////////////////////////////////////////
    /// \brief Structure defining a page of glyphs
    ///
    ////////////////////////////////////////////////////////////
    struct Page
    {
        explicit Page(bool smooth);

        GlyphTable       glyphs;     //!< Table mapping code points to their corresponding glyph
        Texture          texture;    //!< Texture containing the pixels of the glyphs
        unsigned int     nextRow{3}; //!< Y position of the next new row in the texture
        std::vector<Row> rows;       //!< List containing the position of all the existing rows
    };

    ////////////////////////////////////////////////////////////
    /// \brief Free all the internal resources
    ///
    ////////////////////////////////////////////////////////////
    void cleanup();

    ////////////////////////////////////////////////////////////
    /// \brief Open from stream and print errors with custom message
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool openFromStreamImpl(InputStream& stream, std::string_view type);

    ////////////////////////////////////////////////////////////
    /// \brief Find or create the glyphs page corresponding to the given character size
    ///
    /// \param characterSize Reference character size
    ///
    /// \return The glyphs page corresponding to \a characterSize
    ///
    ////////////////////////////////////////////////////////////
    Page& loadPage(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Load a new glyph and store it in the cache
    ///
    /// \param id               Glyph ID of the character to load
    /// \param characterSize    Reference character size
    /// \param bold             Retrieve the bold version or the regular one?
    /// \param outlineThickness Thickness of outline (when != 0 the glyph will not be filled)
    ///
    /// \return The glyph corresponding to `id` and `characterSize`
    ///
    ////////////////////////////////////////////////////////////
    Glyph loadGlyph(std::uint32_t id, unsigned int characterSize, bool bold, float outlineThickness) const;

    ////////////////////////////////////////////////////////////
    /// \brief Find a suitable rectangle within the texture for a glyph
    ///
    /// \param page Page of glyphs to search in
    /// \param size Width and height of the rectangle
    ///
    /// \return Found rectangle within the texture
    ///
    ////////////////////////////////////////////////////////////
    IntRect findGlyphRect(Page& page, Vector2u size) const;

    ////////////////////////////////////////////////////////////
    /// \brief Make sure that the given size is the current one
    ///
    /// \param characterSize Reference character size
    ///
    /// \return `true` on success, `false` if any error happened
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool setCurrentSize(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Read the variation axes and named instances from the face
    ///
    ////////////////////////////////////////////////////////////
    void loadVariableFontInfo();

    ////////////////////////////////////////////////////////////
    /// \brief Make sure the FreeType face uses the current axis values
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool applyAxisValues() const;

    ////////////////////////////////////////////////////////////
    /// \brief Build the page key for the current axis values
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] PageKey makePageKey(unsigned int characterSize) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the active axis values as tag/value pairs for shaping
    ///
    /// This is used internally by Text so that HarfBuzz shaping takes
    /// the same variation coordinates into account as glyph rasterization.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::vector<std::pair<std::uint32_t, float>> getAxisVariationsForShaping() const;

    ////////////////////////////////////////////////////////////
    /// Handle
    ////////////////////////////////////////////////////////////
    using FontHandle = void*; //!< Font handle used by the shaper

    ////////////////////////////////////////////////////////////
    /// \brief Get the current font handle
    ///
    /// This is used internally by Text to shape unicode text.
    ///
    /// \warning Using this handle without care may result in unwanted
    /// side effects, as it could interfere with SFMLs internal usage!
    ///
    /// \return The currently active font handle or nullptr if there is none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] FontHandle getFontHandle() const;

    ////////////////////////////////////////////////////////////
    // Types
    ////////////////////////////////////////////////////////////
    struct FontHandles;
    using PageTable = std::unordered_map<PageKey, Page, PageKeyHash>; //!< Table mapping a size/axis combination to its page (texture)

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    std::shared_ptr<FontHandles> m_fontHandles;    //!< Shared information about the internal font instance
    bool                         m_isSmooth{true}; //!< Status of the smooth filter
    Info                         m_info;           //!< Information about the font
    mutable PageTable            m_pages;          //!< Table containing the glyphs pages by size and axis values
    mutable std::vector<std::uint8_t> m_pixelBuffer; //!< Pixel buffer holding a glyph's pixels before being written to the texture
    std::shared_ptr<InputStream>                 m_stream;         //!< Stream for openFromFile and openFromMemory
    std::vector<VariableAxis>                    m_variableAxes;   //!< Variation axes exposed by the font
    std::vector<NamedInstance>                   m_namedInstances; //!< Named instances exposed by the font
    std::vector<std::pair<std::uint32_t, float>> m_axisValues;     //!< Currently applied axis values (non-default only)
    mutable bool m_axisValuesApplied{true};                        //!< Whether the face already uses m_axisValues
    mutable bool m_sizeMetricsDirty{false}; //!< Whether the active size predates the last axis change

    ////////////////////////////////////////////////////////////
    /// \brief Default global metrics captured before any axis is set
    ///
    /// FreeType does not always restore MVAR-derived hhea metrics when
    /// the default coordinates are selected again, so the values are
    /// snapshotted at load time and reused whenever no axis deviates
    /// from its default. This keeps output bit-identical for fonts
    /// whose axes were never (or no longer) customized.
    ///
    ////////////////////////////////////////////////////////////
    struct DefaultMetrics
    {
        long  ascender{};           //!< Default hhea ascender, in font units
        long  descender{};          //!< Default hhea descender, in font units
        long  height{};             //!< Default hhea line height, in font units
        short underlinePosition{};  //!< Default underline position, in font units
        short underlineThickness{}; //!< Default underline thickness, in font units
        bool  valid{};              //!< Whether the snapshot was taken
    };
    DefaultMetrics m_defaultMetrics; //!< Metrics of the unmodified default instance
};

} // namespace sf


////////////////////////////////////////////////////////////
/// \class sf::Font
/// \ingroup graphics
///
/// Fonts can be opened from a file, from memory or from a custom
/// stream, and supports the most common types of fonts. See
/// the openFromFile function for the complete list of supported formats.
///
/// Once it is opened, a `sf::Font` instance provides three
/// types of information about the font:
/// \li Global metrics, such as the line spacing
/// \li Per-glyph metrics, such as bounding box or kerning
/// \li Pixel representation of glyphs
///
/// Fonts alone are not very useful: they hold the font data
/// but cannot make anything useful of it. To do so you need to
/// use the `sf::Text` class, which is able to properly output text
/// with several options such as character size, style, color,
/// position, rotation, etc.
/// This separation allows more flexibility and better performances:
/// indeed a `sf::Font` is a heavy resource, and any operation on it
/// is slow (often too slow for real-time applications). On the other
/// side, a `sf::Text` is a lightweight object which can combine the
/// glyphs data and metrics of a `sf::Font` to display any text on a
/// render target.
/// Note that it is also possible to bind several `sf::Text` instances
/// to the same `sf::Font`.
///
/// It is important to note that the `sf::Text` instance doesn't
/// copy the font that it uses, it only keeps a reference to it.
/// Thus, a `sf::Font` must not be destructed while it is
/// used by a `sf::Text` (i.e. never write a function that
/// uses a local `sf::Font` instance for creating a text).
///
/// Usage example:
/// \code
/// // Open a new font
/// const sf::Font font("arial.ttf");
///
/// // Create a text which uses our font
/// sf::Text text1(font);
/// text1.setCharacterSize(30);
/// text1.setStyle(sf::Text::Regular);
///
/// // Create another text using the same font, but with different parameters
/// sf::Text text2(font);
/// text2.setCharacterSize(50);
/// text2.setStyle(sf::Text::Italic);
/// \endcode
///
/// Apart from opening font files, and passing them to instances
/// of `sf::Text`, you should normally not have to deal directly
/// with this class. However, it may be useful to access the
/// font metrics or rasterized glyphs for advanced usage.
///
/// Note that if the font is a bitmap font, it is not scalable,
/// thus not all requested sizes will be available to use. This
/// needs to be taken into consideration when using `sf::Text`.
/// If you need to display text of a certain size, make sure the
/// corresponding bitmap font that supports that size is used.
///
/// \see `sf::Text`
///
////////////////////////////////////////////////////////////
