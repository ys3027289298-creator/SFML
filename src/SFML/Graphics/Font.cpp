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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#ifdef SFML_SYSTEM_ANDROID
#include <SFML/System/Android/ResourceStream.hpp>
#endif
#include <SFML/System/Err.hpp>
#include <SFML/System/Exception.hpp>
#include <SFML/System/FileInputStream.hpp>
#include <SFML/System/MemoryInputStream.hpp>
#include <SFML/System/Utils.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_STROKER_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H

#include <algorithm>
#include <atomic>
#include <functional>
#include <ostream>
#include <utility>

#include <cmath>
#include <cstring>


namespace
{
// FreeType callbacks that operate on a sf::InputStream
unsigned long read(FT_Stream rec, unsigned long offset, unsigned char* buffer, unsigned long count)
{
    auto* stream = static_cast<sf::InputStream*>(rec->descriptor.pointer);
    if (stream->seek(offset) == offset)
    {
        if (count > 0)
            return static_cast<unsigned long>(stream->read(reinterpret_cast<char*>(buffer), count).value());

        return 0;
    }

    return count > 0 ? 0 : 1; // error code is 0 if we're reading, or nonzero if we're seeking
}
void close(FT_Stream)
{
}

// Helper to interpret memory as a specific type
template <typename T, typename U>
inline T reinterpret(const U& input)
{
    T output;
    std::memcpy(&output, &input, sizeof(U));
    return output;
}

// Combine outline thickness, boldness and font glyph index into a single 64-bit key
std::uint64_t combine(float outlineThickness, bool bold, std::uint32_t index)
{
    return (std::uint64_t{reinterpret<std::uint32_t>(outlineThickness)} << 32) | (std::uint64_t{bold} << 31) | index;
}

// Thread-safe unique identifier generator
std::uint64_t getUniqueId() noexcept
{
    static std::atomic<std::uint64_t> id(1); // start at 1, zero is "no font"

    return id.fetch_add(1);
}

// Convert a four-character axis label into its packed 32-bit tag.
// Returns false if the label does not contain exactly four ASCII characters.
bool tagFromString(std::string_view tag, std::uint32_t& result)
{
    if (tag.size() != 4)
        return false;

    result = 0;
    for (char character : tag)
    {
        const auto unsignedCharacter = static_cast<unsigned char>(character);
        if (unsignedCharacter < 0x20 || unsignedCharacter > 0x7E)
            return false;
        result = (result << 8) | unsignedCharacter;
    }

    return true;
}

// Convert a packed 32-bit axis tag back into a four-character label
std::string tagToString(std::uint32_t tag)
{
    return {static_cast<char>((tag >> 24) & 0xFF),
            static_cast<char>((tag >> 16) & 0xFF),
            static_cast<char>((tag >> 8) & 0xFF),
            static_cast<char>(tag & 0xFF)};
}

// Resolve a 'name' table entry of the face into a UTF-8 string.
// Prefers the Unicode platforms and falls back to the first available
// record with a matching name ID. Returns an empty string on failure.
std::string getNameString(FT_Face face, FT_UInt nameId)
{
    const FT_UInt count = FT_Get_Sfnt_Name_Count(face);

    FT_SfntName best{};
    bool        found = false;
    for (FT_UInt i = 0; i < count; ++i)
    {
        FT_SfntName record;
        if (FT_Get_Sfnt_Name(face, i, &record) != FT_Err_Ok || record.name_id != nameId)
            continue;

        // Windows Unicode (3,1) and Unicode (0,*) records are directly usable
        const bool isWindowsUnicode = record.platform_id == 3 && record.encoding_id == 1;
        const bool isUnicode        = record.platform_id == 0;
        if (isWindowsUnicode || isUnicode)
        {
            best  = record;
            found = true;
            if (isWindowsUnicode)
                break;
        }
        else if (!found)
        {
            best  = record;
            found = true;
        }
    }

    if (!found || best.string_len == 0)
        return {};

    // UTF-16BE records (Windows Unicode / platform 0) need decoding
    const bool isUtf16 = (best.platform_id == 3 && best.encoding_id == 1) || best.platform_id == 0;
    if (!isUtf16)
        return {reinterpret_cast<const char*>(best.string), best.string_len};

    std::string result;
    result.reserve(best.string_len / 2);
    for (FT_UInt i = 0; i + 1 < best.string_len; i += 2)
    {
        const char32_t codePoint = static_cast<char32_t>((best.string[i] << 8) | best.string[i + 1]);

        if (codePoint < 0x80)
        {
            result.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint < 0x800)
        {
            result.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else
        {
            result.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }
    return result;
}
} // namespace


namespace sf
{
////////////////////////////////////////////////////////////
struct Font::FontHandles
{
    FontHandles() = default;

    ~FontHandles()
    {
        // All the function below are safe to call with null pointer arguments.
        // The documentation of FreeType isn't clear on the matter, but the
        // implementation does explicitly check for null.

        FT_Stroker_Done(stroker);
        FT_Done_Face(face);
        // `streamRec` doesn't need to be explicitly freed.
        FT_Done_FreeType(library);
    }

    // clang-format off
    FontHandles(const FontHandles&)            = delete;
    FontHandles& operator=(const FontHandles&) = delete;

    FontHandles(FontHandles&&)            = delete;
    FontHandles& operator=(FontHandles&&) = delete;
    // clang-format on

    FT_Library   library{};   //< Pointer to the internal library interface
    FT_StreamRec streamRec{}; //< Stream rec object describing an input stream
    FT_Face      face{};      //< Pointer to the internal font face
    FT_Stroker   stroker{};   //< Pointer to the stroker
};


////////////////////////////////////////////////////////////
Font::Font(const std::filesystem::path& filename)
{
    if (!openFromFile(filename))
        throw Exception("Failed to open font from file");
}


////////////////////////////////////////////////////////////
Font::Font(const void* data, std::size_t sizeInBytes)
{
    if (!openFromMemory(data, sizeInBytes))
        throw Exception("Failed to open font from memory");
}


////////////////////////////////////////////////////////////
Font::Font(InputStream& stream)
{
    if (!openFromStream(stream))
        throw Exception("Failed to open font from stream");
}


////////////////////////////////////////////////////////////
bool Font::openFromFile(const std::filesystem::path& filename)
{
    using namespace std::string_view_literals;

    // Cleanup the previous resources
    cleanup();

    // Create the input stream and open the file
#ifndef SFML_SYSTEM_ANDROID
    const auto stream = std::make_shared<FileInputStream>();
    const auto type   = "file"sv;
#else
    const auto stream = std::make_shared<priv::ResourceStream>();
    const auto type   = "Android resource stream"sv;
#endif

    if (!stream->open(filename))
    {
        err() << "Failed to load font (failed to open file): " << std::strerror(errno) << '\n'
              << formatDebugPathInfo(filename) << std::endl;
        return false;
    }

    // Open the font, and if succesful save the stream to keep it alive
    if (openFromStreamImpl(*stream, type))
    {
        m_stream = stream;
        return true;
    }

    // If loading failed, print filename (after the error message already printed in openFromStreamImpl)
    err() << formatDebugPathInfo(filename) << std::endl;
    return false;
}


////////////////////////////////////////////////////////////
bool Font::openFromMemory(const void* data, std::size_t sizeInBytes)
{
    // Cleanup the previous resources
    cleanup();

    if (!data)
    {
        err() << "Failed to load font from memory (provided data pointer is null)" << std::endl;
        return false;
    }

    // Create memory stream - the memory is owned by the user
    const auto memoryStream = std::make_shared<MemoryInputStream>(data, sizeInBytes);

    // Open the font, and if succesful save the stream to keep it alive
    if (openFromStreamImpl(*memoryStream, "memory"))
    {
        m_stream = memoryStream;
        return true;
    }

    return false;
}


////////////////////////////////////////////////////////////
bool Font::openFromStream(InputStream& stream)
{
    // Make sure that the stream's reading position is at the beginning
    if (!stream.seek(0).has_value())
    {
        err() << "Failed to seek font stream" << std::endl;
        return false;
    }

    // Open the font, do not save the stream in m_stream, its owned by the caller
    return openFromStreamImpl(stream, "stream");
}


////////////////////////////////////////////////////////////
const Font::Info& Font::getInfo() const
{
    return m_info;
}


////////////////////////////////////////////////////////////
bool Font::isVariableFont() const
{
    return !m_variableAxes.empty();
}


////////////////////////////////////////////////////////////
const std::vector<Font::VariableAxis>& Font::getVariableAxes() const
{
    return m_variableAxes;
}


////////////////////////////////////////////////////////////
const std::vector<Font::NamedInstance>& Font::getNamedInstances() const
{
    return m_namedInstances;
}


////////////////////////////////////////////////////////////
bool Font::setVariableAxisValue(std::string_view tag, float value)
{
    std::uint32_t packedTag = 0;
    if (!tagFromString(tag, packedTag))
    {
        err() << "Failed to set variation axis value (axis tag \"" << tag
              << "\" is invalid, expected a four-character label such as \"wght\")" << std::endl;
        return false;
    }

    return setVariableAxisValue(packedTag, value);
}


////////////////////////////////////////////////////////////
bool Font::setVariableAxisValue(std::uint32_t tag, float value)
{
    if (!m_fontHandles || m_variableAxes.empty())
    {
        err() << "Failed to set variation axis value (font is not loaded or is not a variable font)" << std::endl;
        return false;
    }

    const auto axisIt = std::find_if(m_variableAxes.begin(),
                                     m_variableAxes.end(),
                                     [tag](const VariableAxis& axis) { return axis.tag == tag; });
    if (axisIt == m_variableAxes.end())
    {
        err() << "Failed to set variation axis value (axis \"" << tagToString(tag) << "\" does not exist in font \""
              << m_info.family << "\")" << std::endl;
        return false;
    }

    // Clamp the requested value to the axis range
    const float clampedValue = std::clamp(value, axisIt->minimum, axisIt->maximum);
    if (clampedValue != value)
    {
        err() << "Variation axis value " << value << " for axis \"" << tagToString(tag) << "\" is out of range ["
              << axisIt->minimum << ", " << axisIt->maximum << "], clamping to " << clampedValue << std::endl;
    }

    const auto valueIt = std::find_if(m_axisValues.begin(),
                                      m_axisValues.end(),
                                      [tag](const std::pair<std::uint32_t, float>& entry) { return entry.first == tag; });

    // Assigning the default value removes the override entirely so the
    // font converges back to the single empty-axis cache key
    if (clampedValue == axisIt->defaultValue)
    {
        if (valueIt != m_axisValues.end())
            m_axisValues.erase(valueIt);
    }
    else if (valueIt == m_axisValues.end())
        m_axisValues.emplace_back(tag, clampedValue);
    else
        valueIt->second = clampedValue;

    // Keep the entries sorted by tag so cache keys stay comparable
    std::sort(m_axisValues.begin(), m_axisValues.end());

    m_axisValuesApplied = false;
    return true;
}


////////////////////////////////////////////////////////////
std::optional<float> Font::getVariableAxisValue(std::string_view tag) const
{
    std::uint32_t packedTag = 0;
    if (!tagFromString(tag, packedTag))
        return std::nullopt;

    const auto axisIt = std::find_if(m_variableAxes.begin(),
                                     m_variableAxes.end(),
                                     [packedTag](const VariableAxis& axis) { return axis.tag == packedTag; });
    if (axisIt == m_variableAxes.end())
        return std::nullopt;

    const auto valueIt = std::find_if(m_axisValues.begin(),
                                      m_axisValues.end(),
                                      [packedTag](const std::pair<std::uint32_t, float>& entry)
                                      { return entry.first == packedTag; });
    if (valueIt != m_axisValues.end())
        return valueIt->second;

    return axisIt->defaultValue;
}


////////////////////////////////////////////////////////////
bool Font::selectNamedInstance(std::string_view name)
{
    const auto instanceIt = std::find_if(m_namedInstances.begin(),
                                         m_namedInstances.end(),
                                         [name](const NamedInstance& instance) { return instance.name == name; });
    if (instanceIt == m_namedInstances.end())
    {
        err() << "Failed to select named instance \"" << name << "\" (no instance with that name in font \""
              << m_info.family << "\")" << std::endl;
        return false;
    }

    // Rebuild the coordinate set from scratch so axes which are not
    // covered by the instance fall back to their default values. Values
    // that match an axis default are dropped on purpose: m_axisValues only
    // stores non-default coordinates so the reset state keeps a stable,
    // empty cache key.
    std::vector<std::pair<std::uint32_t, float>> newValues;
    for (const auto& [tag, coordinate] : instanceIt->coordinates)
    {
        const auto axisIt = std::find_if(m_variableAxes.begin(),
                                         m_variableAxes.end(),
                                         [tag](const VariableAxis& axis) { return axis.tag == tag; });
        if (axisIt != m_variableAxes.end() && coordinate != axisIt->defaultValue)
            newValues.emplace_back(tag, coordinate);
    }
    std::sort(newValues.begin(), newValues.end());

    m_axisValues        = std::move(newValues);
    m_axisValuesApplied = false;
    return true;
}


////////////////////////////////////////////////////////////
void Font::resetVariableAxes()
{
    if (m_axisValues.empty())
        return;

    m_axisValues.clear();
    m_axisValuesApplied = false;
}


////////////////////////////////////////////////////////////
std::vector<std::pair<std::uint32_t, float>> Font::getVariableAxisValues() const
{
    return m_axisValues;
}


////////////////////////////////////////////////////////////
const Glyph& Font::getGlyphById(std::uint32_t id, unsigned int characterSize, bool bold, float outlineThickness) const
{
    // Make sure the face currently matches the requested axis values
    if (!applyAxisValues())
    {
        static const Glyph emptyGlyph;
        return emptyGlyph;
    }

    // Get the page corresponding to the character size
    GlyphTable& glyphs = loadPage(characterSize).glyphs;

    // Build the key by combining the glyph index (based on code point), bold flag, and outline thickness
    const std::uint64_t key = combine(outlineThickness, bold, id);

    // Search the glyph into the cache
    if (const auto it = glyphs.find(key); it != glyphs.end())
    {
        // Found: just return it
        return it->second;
    }

    // Not found: we have to load it
    const Glyph glyph = loadGlyph(id, characterSize, bold, outlineThickness);
    return glyphs.try_emplace(key, glyph).first->second;
}


////////////////////////////////////////////////////////////
const Glyph& Font::getGlyph(char32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness) const
{
    return getGlyphById(FT_Get_Char_Index(m_fontHandles ? m_fontHandles->face : nullptr, codePoint),
                        characterSize,
                        bold,
                        outlineThickness);
}


////////////////////////////////////////////////////////////
bool Font::hasGlyph(char32_t codePoint) const
{
    return FT_Get_Char_Index(m_fontHandles ? m_fontHandles->face : nullptr, codePoint) != 0;
}


////////////////////////////////////////////////////////////
float Font::getKerning(std::uint32_t first, std::uint32_t second, unsigned int characterSize, bool bold) const
{
    return getKerning(char32_t{first}, char32_t{second}, characterSize, bold);
}


////////////////////////////////////////////////////////////
float Font::getKerning(char32_t first, char32_t second, unsigned int characterSize, bool bold) const
{
    // Special case where first or second is null character
    if (first == U'\0' || second == U'\0')
        return 0.f;

    FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;

    if (face && applyAxisValues() && setCurrentSize(characterSize))
    {
        // Convert the characters to indices
        const FT_UInt index1 = FT_Get_Char_Index(face, first);
        const FT_UInt index2 = FT_Get_Char_Index(face, second);

        // Retrieve position compensation deltas generated by auto-hinting if it is used
        const auto firstRsbDelta  = static_cast<float>(getGlyph(first, characterSize, bold).rsbDelta);
        const auto secondLsbDelta = static_cast<float>(getGlyph(second, characterSize, bold).lsbDelta);

        // Get the kerning vector if present
        FT_Vector kerning{0, 0};
        if (FT_HAS_KERNING(face))
            FT_Get_Kerning(face, index1, index2, FT_KERNING_UNFITTED, &kerning);

        // X advance is already in pixels for bitmap fonts
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(kerning.x);

        // Combine kerning with compensation deltas and return the X advance
        // Flooring is required as we use FT_KERNING_UNFITTED flag which is not quantized in 64 based grid
        return std::floor((secondLsbDelta - firstRsbDelta + static_cast<float>(kerning.x) + 32) / float{1 << 6});
    }

    // Invalid font
    return 0.f;
}


////////////////////////////////////////////////////////////
float Font::getAscent(unsigned int characterSize) const
{
    FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;

    if (face && applyAxisValues() && setCurrentSize(characterSize))
    {
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(face->size->metrics.ascender) / float{1 << 6};

        if (m_axisValues.empty() && m_defaultMetrics.valid && !m_variableAxes.empty())
            return static_cast<float>(FT_MulFix(m_defaultMetrics.ascender, face->size->metrics.y_scale)) / float{1 << 6};

        return static_cast<float>(FT_MulFix(face->ascender, face->size->metrics.y_scale)) / float{1 << 6};
    }

    return 0.f;
}


////////////////////////////////////////////////////////////
float Font::getDescent(unsigned int characterSize) const
{
    FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;

    if (face && applyAxisValues() && setCurrentSize(characterSize))
    {
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(face->size->metrics.descender) / float{1 << 6};

        if (m_axisValues.empty() && m_defaultMetrics.valid && !m_variableAxes.empty())
            return static_cast<float>(FT_MulFix(m_defaultMetrics.descender, face->size->metrics.y_scale)) / float{1 << 6};

        return static_cast<float>(FT_MulFix(face->descender, face->size->metrics.y_scale)) / float{1 << 6};
    }

    return 0.f;
}


////////////////////////////////////////////////////////////
float Font::getLineSpacing(unsigned int characterSize) const
{
    FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;

    if (face && applyAxisValues() && setCurrentSize(characterSize))
    {
        if (m_axisValues.empty() && m_defaultMetrics.valid && !m_variableAxes.empty())
            return static_cast<float>(FT_MulFix(m_defaultMetrics.height, face->size->metrics.y_scale)) / float{1 << 6};

        return static_cast<float>(face->size->metrics.height) / float{1 << 6};
    }

    return 0.f;
}


////////////////////////////////////////////////////////////
float Font::getUnderlinePosition(unsigned int characterSize) const
{
    FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;

    if (face && applyAxisValues() && setCurrentSize(characterSize))
    {
        // Return a fixed position if font is a bitmap font
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(characterSize) / 10.f;

        if (m_axisValues.empty() && m_defaultMetrics.valid && !m_variableAxes.empty())
            return -static_cast<float>(FT_MulFix(m_defaultMetrics.underlinePosition, face->size->metrics.y_scale)) /
                   float{1 << 6};

        return -static_cast<float>(FT_MulFix(face->underline_position, face->size->metrics.y_scale)) / float{1 << 6};
    }

    return 0.f;
}


////////////////////////////////////////////////////////////
float Font::getUnderlineThickness(unsigned int characterSize) const
{
    FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;

    if (face && applyAxisValues() && setCurrentSize(characterSize))
    {
        // Return a fixed thickness if font is a bitmap font
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(characterSize) / 14.f;

        if (m_axisValues.empty() && m_defaultMetrics.valid && !m_variableAxes.empty())
            return static_cast<float>(FT_MulFix(m_defaultMetrics.underlineThickness, face->size->metrics.y_scale)) /
                   float{1 << 6};

        return static_cast<float>(FT_MulFix(face->underline_thickness, face->size->metrics.y_scale)) / float{1 << 6};
    }

    return 0.f;
}


////////////////////////////////////////////////////////////
const Texture& Font::getTexture(unsigned int characterSize) const
{
    static_cast<void>(applyAxisValues());
    return loadPage(characterSize).texture;
}

////////////////////////////////////////////////////////////
void Font::setSmooth(bool smooth)
{
    if (smooth != m_isSmooth)
    {
        m_isSmooth = smooth;

        for (auto& [key, page] : m_pages)
        {
            page.texture.setSmooth(m_isSmooth);
        }
    }
}

////////////////////////////////////////////////////////////
bool Font::isSmooth() const
{
    return m_isSmooth;
}


////////////////////////////////////////////////////////////
void Font::cleanup()
{
    // Drop ownership of shared FreeType pointers
    m_fontHandles.reset();

    // Reset members
    m_pages.clear();
    std::vector<std::uint8_t>().swap(m_pixelBuffer);

    // Drop the file stream if we held one due to openFromFile or openFromMemory
    m_stream.reset();

    // Reset variation state
    m_variableAxes.clear();
    m_namedInstances.clear();
    m_axisValues.clear();
    m_axisValuesApplied = true;
    m_sizeMetricsDirty  = false;
}


////////////////////////////////////////////////////////////
bool Font::openFromStreamImpl(InputStream& stream, std::string_view type)
{
    // Cleanup the previous resources
    cleanup();

    auto fontHandles = std::make_shared<FontHandles>();

    // Initialize FreeType
    // Note: we initialize FreeType for every font instance in order to avoid having a single
    // global manager that would create a lot of issues regarding creation and destruction order.
    if (FT_Init_FreeType(&fontHandles->library) != 0)
    {
        err() << "Failed to load font from " << type << " (failed to initialize FreeType)" << std::endl;
        return false;
    }

    // Prepare a wrapper for our stream, that we'll pass to FreeType callbacks
    fontHandles->streamRec.base               = nullptr;
    fontHandles->streamRec.size               = static_cast<unsigned long>(stream.getSize().value());
    fontHandles->streamRec.pos                = 0;
    fontHandles->streamRec.descriptor.pointer = &stream;
    fontHandles->streamRec.read               = &read;
    fontHandles->streamRec.close              = &close;

    // Setup the FreeType callbacks that will read our stream
    FT_Open_Args args;
    args.flags  = FT_OPEN_STREAM;
    args.stream = &fontHandles->streamRec;
    args.driver = nullptr;

    // Load the new font face from the specified stream
    FT_Face face = nullptr;
    if (FT_Open_Face(fontHandles->library, &args, 0, &face) != 0)
    {
        err() << "Failed to load font from " << type << " (failed to create the font face)" << std::endl;
        return false;
    }
    fontHandles->face = face;

    // Load the stroker that will be used to outline the font
    if (FT_Stroker_New(fontHandles->library, &fontHandles->stroker) != 0)
    {
        err() << "Failed to load font from " << type << " (failed to create the stroker)" << std::endl;
        return false;
    }

    // Select the Unicode character map
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        err() << "Failed to load font from " << type << " (failed to set the Unicode character set)" << std::endl;
        return false;
    }

    // Store the loaded font handles
    m_fontHandles = std::move(fontHandles);

    // Store the font information
    m_info.id                 = getUniqueId();
    m_info.family             = face->family_name ? face->family_name : std::string();
    m_info.hasKerning         = FT_HAS_KERNING(face);
    m_info.hasVerticalMetrics = FT_HAS_VERTICAL(face);

    // Snapshot the metrics of the untouched default instance. FreeType does
    // not reliably restore MVAR-derived hhea metrics after variation
    // coordinates were set and cleared again, so we reuse this snapshot
    // whenever the font is back on its default axis values.
    m_defaultMetrics.ascender           = face->ascender;
    m_defaultMetrics.descender          = face->descender;
    m_defaultMetrics.height             = face->height;
    m_defaultMetrics.underlinePosition  = face->underline_position;
    m_defaultMetrics.underlineThickness = face->underline_thickness;
    m_defaultMetrics.valid              = true;

    // Enumerate variation axes and named instances (no-op for static fonts)
    loadVariableFontInfo();

    return true;
}


////////////////////////////////////////////////////////////
Font::Page& Font::loadPage(unsigned int characterSize) const
{
    return m_pages.try_emplace(makePageKey(characterSize), m_isSmooth).first->second;
}


////////////////////////////////////////////////////////////
void Font::loadVariableFontInfo()
{
    m_variableAxes.clear();
    m_namedInstances.clear();
    m_axisValues.clear();
    m_axisValuesApplied = true;
    m_sizeMetricsDirty  = false;

    FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;
    if (!face || !FT_HAS_MULTIPLE_MASTERS(face))
        return;

    FT_MM_Var* mmVar = nullptr;
    if (FT_Get_MM_Var(face, &mmVar) != 0 || !mmVar)
    {
        err() << "Failed to query variation axes of font \"" << m_info.family << "\"" << std::endl;
        return;
    }

    // FreeType guarantees that FT_Get_MM_Var succeeds for any face that
    // reports the multiple masters flag, but keep the guard for safety.
    struct MmVarDeleter
    {
        FT_Library library;
        void       operator()(FT_MM_Var* pointer) const
        {
            if (pointer)
                FT_Done_MM_Var(library, pointer);
        }
    };
    const std::unique_ptr<FT_MM_Var, MmVarDeleter> holder(mmVar, MmVarDeleter{m_fontHandles->library});

    m_variableAxes.reserve(mmVar->num_axis);
    for (FT_UInt i = 0; i < mmVar->num_axis; ++i)
    {
        const FT_Var_Axis& ftAxis = mmVar->axis[i];

        VariableAxis axis;
        axis.tag          = static_cast<std::uint32_t>(ftAxis.tag);
        axis.name         = ftAxis.name ? ftAxis.name : tagToString(static_cast<std::uint32_t>(ftAxis.tag));
        axis.minimum      = static_cast<float>(ftAxis.minimum) / float{1 << 16};
        axis.maximum      = static_cast<float>(ftAxis.maximum) / float{1 << 16};
        axis.defaultValue = static_cast<float>(ftAxis.def) / float{1 << 16};
        m_variableAxes.push_back(std::move(axis));
    }

    m_namedInstances.reserve(mmVar->num_namedstyles);
    for (FT_UInt i = 0; i < mmVar->num_namedstyles; ++i)
    {
        const FT_Var_Named_Style& ftStyle = mmVar->namedstyle[i];

        NamedInstance instance;
        instance.name = getNameString(face, ftStyle.strid);
        if (instance.name.empty())
            instance.name = "Instance " + std::to_string(i);

        instance.coordinates.reserve(mmVar->num_axis);
        for (FT_UInt axisIndex = 0; axisIndex < mmVar->num_axis; ++axisIndex)
        {
            const float coordinate = static_cast<float>(ftStyle.coords[axisIndex]) / float{1 << 16};
            instance.coordinates.emplace_back(mmVar->axis[axisIndex].tag, coordinate);
        }
        std::sort(instance.coordinates.begin(), instance.coordinates.end());
        m_namedInstances.push_back(std::move(instance));
    }
}


////////////////////////////////////////////////////////////
bool Font::applyAxisValues() const
{
    if (m_axisValuesApplied)
        return true;

    if (!m_fontHandles || !m_fontHandles->face)
        return false;

    FT_Face face = m_fontHandles->face;

    // Nothing applied yet: reset the face to its default instance so that
    // a previously selected variation cannot leak into the new state.
    if (m_axisValues.empty())
    {
        if (FT_Set_Named_Instance(face, 0) != FT_Err_Ok)
        {
            err() << "Failed to reset variation axes of font \"" << m_info.family << "\"" << std::endl;
            return false;
        }

        m_axisValuesApplied = true;
        m_sizeMetricsDirty  = true;
        return true;
    }

    std::vector<FT_Fixed> coordinates;
    coordinates.reserve(m_variableAxes.size());

    // Start from the default coordinate of every axis, then override the
    // axes the user explicitly assigned a value to.
    for (const VariableAxis& axis : m_variableAxes)
    {
        const auto  valueIt = std::find_if(m_axisValues.begin(),
                                          m_axisValues.end(),
                                          [&axis](const std::pair<std::uint32_t, float>& entry)
                                          { return entry.first == axis.tag; });
        const float value   = (valueIt != m_axisValues.end()) ? valueIt->second : axis.defaultValue;
        coordinates.push_back(static_cast<FT_Fixed>(std::llround(value * float{1 << 16})));
    }

    const FT_Error error = FT_Set_Var_Design_Coordinates(face, static_cast<FT_UInt>(coordinates.size()), coordinates.data());
    if (error != FT_Err_Ok)
    {
        err() << "Failed to set variation coordinates of font \"" << m_info.family << "\"" << std::endl;
        return false;
    }

    m_axisValuesApplied = true;
    m_sizeMetricsDirty  = true;
    return true;
}


////////////////////////////////////////////////////////////
Font::PageKey Font::makePageKey(unsigned int characterSize) const
{
    return PageKey{characterSize, m_axisValues};
}


////////////////////////////////////////////////////////////
std::vector<std::pair<std::uint32_t, float>> Font::getAxisVariationsForShaping() const
{
    // Make sure a newly created HarfBuzz font observes the same face state
    static_cast<void>(applyAxisValues());
    return m_axisValues;
}


////////////////////////////////////////////////////////////
bool Font::PageKey::operator==(const PageKey& rhs) const
{
    return characterSize == rhs.characterSize && axisValues == rhs.axisValues;
}


////////////////////////////////////////////////////////////
std::size_t Font::PageKeyHash::operator()(const PageKey& key) const
{
    std::size_t seed = std::hash<unsigned int>{}(key.characterSize);
    for (const auto& [tag, value] : key.axisValues)
    {
        // Reinterpret the float bits so identical values hash identically
        std::uint32_t valueBits = 0;
        std::memcpy(&valueBits, &value, sizeof(valueBits));
        seed ^= std::hash<std::uint32_t>{}(tag) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<std::uint32_t>{}(valueBits) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}


////////////////////////////////////////////////////////////
Glyph Font::loadGlyph(std::uint32_t id, unsigned int characterSize, bool bold, float outlineThickness) const
{
    // The glyph to return
    Glyph glyph;

    // Stop if no font is loaded
    if (!m_fontHandles)
        return glyph;

    // Get our FT_Face
    FT_Face face = m_fontHandles->face;
    if (!face)
        return glyph;

    // Set the character size
    if (!setCurrentSize(characterSize))
        return glyph;

    // Load the glyph corresponding to the code point
    FT_Int32 flags = FT_LOAD_TARGET_NORMAL;
    if (outlineThickness != 0)
        flags |= FT_LOAD_NO_BITMAP;
    if (FT_Load_Glyph(face, id, flags) != 0)
        return glyph;

    // Retrieve the glyph
    FT_Glyph glyphDesc = nullptr;
    if (FT_Get_Glyph(face->glyph, &glyphDesc) != 0)
        return glyph;

    // Apply bold and outline (there is no fallback for outline) if necessary -- first technique using outline (highest quality)
    const FT_Pos weight  = 1 << 6;
    const bool   outline = (glyphDesc->format == FT_GLYPH_FORMAT_OUTLINE);
    if (outline)
    {
        if (bold)
        {
            auto* outlineGlyph = reinterpret_cast<FT_OutlineGlyph>(glyphDesc);
            FT_Outline_Embolden(&outlineGlyph->outline, weight);
        }

        if (outlineThickness != 0)
        {
            FT_Stroker stroker = m_fontHandles->stroker;

            FT_Stroker_Set(stroker,
                           static_cast<FT_Fixed>(outlineThickness * float{1 << 6}),
                           FT_STROKER_LINECAP_ROUND,
                           FT_STROKER_LINEJOIN_ROUND,
                           0);
            FT_Glyph_Stroke(&glyphDesc, stroker, true);
        }
    }

    // Convert the glyph to a bitmap (i.e. rasterize it)
    // Warning! After this line, do not read any data from glyphDesc directly, use
    // bitmapGlyph.root to access the FT_Glyph data.
    FT_Glyph_To_Bitmap(&glyphDesc, FT_RENDER_MODE_NORMAL, nullptr, 1);
    auto*      bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyphDesc);
    FT_Bitmap& bitmap      = bitmapGlyph->bitmap;

    // Apply bold if necessary -- fallback technique using bitmap (lower quality)
    if (!outline)
    {
        if (bold)
            FT_Bitmap_Embolden(m_fontHandles->library, &bitmap, weight, weight);

        if (outlineThickness != 0)
            err() << "Failed to outline glyph (no fallback available)" << std::endl;
    }

    // Compute the glyph's advance offset
    glyph.advance = static_cast<float>(bitmapGlyph->root.advance.x >> 16);
    if (bold)
        glyph.advance += static_cast<float>(weight) / float{1 << 6};

    glyph.lsbDelta = static_cast<int>(face->glyph->lsb_delta);
    glyph.rsbDelta = static_cast<int>(face->glyph->rsb_delta);

    Vector2u size(bitmap.width, bitmap.rows);

    if ((size.x > 0) && (size.y > 0))
    {
        // Leave a small padding around characters, so that filtering doesn't
        // pollute them with pixels from neighbors
        const unsigned int padding = 2;

        size += 2u * Vector2u(padding, padding);

        // Get the glyphs page corresponding to the character size
        Page& page = loadPage(characterSize);

        // Find a good position for the new glyph into the texture
        glyph.textureRect = findGlyphRect(page, size);

        // Make sure the texture data is positioned in the center
        // of the allocated texture rectangle
        glyph.textureRect.position += Vector2i(padding, padding);
        glyph.textureRect.size -= 2 * Vector2i(padding, padding);

        // Compute the glyph's bounding box
        glyph.bounds.position = Vector2f(Vector2i(bitmapGlyph->left, -bitmapGlyph->top));
        glyph.bounds.size     = Vector2f(Vector2u(bitmap.width, bitmap.rows));

        // Resize the pixel buffer to the new size and fill it with transparent white pixels
        m_pixelBuffer.resize(std::size_t{size.x} * std::size_t{size.y} * 4);

        std::uint8_t* current = m_pixelBuffer.data();
        std::uint8_t* end     = current + size.x * size.y * 4;

        while (current != end)
        {
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 0;
        }

        // Extract the glyph's pixels from the bitmap
        const std::uint8_t* pixels = bitmap.buffer;
        if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
        {
            // Pixels are 1 bit monochrome values
            for (unsigned int y = padding; y < size.y - padding; ++y)
            {
                for (unsigned int x = padding; x < size.x - padding; ++x)
                {
                    // The color channels remain white, just fill the alpha channel
                    const std::size_t index = x + y * size.x;
                    m_pixelBuffer[index * 4 + 3] = ((pixels[(x - padding) / 8]) & (1 << (7 - ((x - padding) % 8)))) ? 255 : 0;
                }
                pixels += bitmap.pitch;
            }
        }
        else
        {
            // Pixels are 8 bit gray levels
            for (unsigned int y = padding; y < size.y - padding; ++y)
            {
                for (unsigned int x = padding; x < size.x - padding; ++x)
                {
                    // The color channels remain white, just fill the alpha channel
                    const std::size_t index      = x + y * size.x;
                    m_pixelBuffer[index * 4 + 3] = pixels[x - padding];
                }
                pixels += bitmap.pitch;
            }
        }

        // Write the pixels to the texture
        const auto dest       = Vector2u(glyph.textureRect.position) - Vector2u(padding, padding);
        const auto updateSize = Vector2u(glyph.textureRect.size) + 2u * Vector2u(padding, padding);
        page.texture.update(m_pixelBuffer.data(), updateSize, dest);
    }

    // Delete the FT glyph
    FT_Done_Glyph(glyphDesc);

    // Done :)
    return glyph;
}


////////////////////////////////////////////////////////////
IntRect Font::findGlyphRect(Page& page, Vector2u size) const
{
    // Find the line that fits well the glyph
    Row*  row       = nullptr;
    float bestRatio = 0;
    for (auto it = page.rows.begin(); it != page.rows.end() && !row; ++it)
    {
        const float ratio = static_cast<float>(size.y) / static_cast<float>(it->height);

        // Ignore rows that are either too small or too high
        if ((ratio < 0.7f) || (ratio > 1.f))
            continue;

        // Check if there's enough horizontal space left in the row
        if (size.x > page.texture.getSize().x - it->width)
            continue;

        // Make sure that this new row is the best found so far
        if (ratio < bestRatio)
            continue;

        // The current row passed all the tests: we can select it
        row       = &*it;
        bestRatio = ratio;
    }

    // If we didn't find a matching row, create a new one (10% taller than the glyph)
    if (!row)
    {
        const unsigned int rowHeight = size.y + size.y / 10;
        while ((page.nextRow + rowHeight >= page.texture.getSize().y) || (size.x >= page.texture.getSize().x))
        {
            // Not enough space: resize the texture if possible
            const Vector2u textureSize = page.texture.getSize();
            if ((textureSize.x * 2 <= Texture::getMaximumSize()) && (textureSize.y * 2 <= Texture::getMaximumSize()))
            {
                // Make the texture 2 times bigger
                Texture newTexture;
                if (!newTexture.resize(textureSize * 2u))
                {
                    err() << "Failed to create new page texture" << std::endl;
                    return {{0, 0}, {2, 2}};
                }

                newTexture.setSmooth(m_isSmooth);
                newTexture.update(page.texture);
                page.texture.swap(newTexture);
            }
            else
            {
                // Oops, we've reached the maximum texture size...
                err() << "Failed to add a new character to the font: the maximum texture size has been reached"
                      << std::endl;
                return {{0, 0}, {2, 2}};
            }
        }

        // We can now create the new row
        page.rows.emplace_back(page.nextRow, rowHeight);
        page.nextRow += rowHeight;
        row = &page.rows.back();
    }

    // Find the glyph's rectangle on the selected row
    IntRect rect(Rect<unsigned int>({row->width, row->top}, size));

    // Update the row information
    row->width += size.x;

    return rect;
}


////////////////////////////////////////////////////////////
bool Font::setCurrentSize(unsigned int characterSize) const
{
    // FT_Set_Pixel_Sizes is an expensive function, so we must call it
    // only when necessary to avoid killing performances

    // m_fontHandles and m_fontHandles->face are checked to be non-null before calling this method
    FT_Face         face        = m_fontHandles->face;
    const FT_UShort currentSize = face->size->metrics.x_ppem;

    // FreeType only recomputes MVAR-derived size metrics when
    // FT_Set_Pixel_Sizes is called. After variation coordinates changed we
    // therefore force a refresh even if the requested ppem is already
    // active, so line spacing and friends observe the new axis values.
    if (currentSize != characterSize || m_sizeMetricsDirty)
    {
        const FT_Error result = FT_Set_Pixel_Sizes(face, 0, characterSize);
        if (result == FT_Err_Ok)
            m_sizeMetricsDirty = false;

        if (result == FT_Err_Invalid_Pixel_Size)
        {
            // In the case of bitmap fonts, resizing can
            // fail if the requested size is not available
            if (!FT_IS_SCALABLE(face))
            {
                err() << "Failed to set bitmap font size to " << characterSize << '\n' << "Available sizes are: ";
                for (int i = 0; i < face->num_fixed_sizes; ++i)
                {
                    const long size = (face->available_sizes[i].y_ppem + 32) >> 6;
                    err() << size << " ";
                }
                err() << std::endl;
            }
            else
            {
                err() << "Failed to set font size to " << characterSize << std::endl;
            }
        }

        return result == FT_Err_Ok;
    }

    return true;
}


////////////////////////////////////////////////////////////
Font::Page::Page(bool smooth)
{
    // Make sure that the texture is initialized by default
    Image image({128, 128}, Color::Transparent);

    // Reserve a 2x2 white square for texturing underlines
    for (unsigned int x = 0; x < 2; ++x)
        for (unsigned int y = 0; y < 2; ++y)
            image.setPixel({x, y}, Color::White);

    // Create the texture
    if (!texture.loadFromImage(image))
    {
        err() << "Failed to load font page texture" << std::endl;
    }

    texture.setSmooth(smooth);
}


////////////////////////////////////////////////////////////
Font::FontHandle Font::getFontHandle() const
{
    if (!m_fontHandles)
        return nullptr;

    return m_fontHandles->face;
}

} // namespace sf
