///
/// highlight attribute
///
#pragma once
#include <stdint.h>
#include <vector>

constexpr int MAX_HIGHLIGHT_ATTRIBS = 0xFFFF;

enum HighlightAttributeFlags : uint16_t {
  HL_ATTRIB_REVERSE = 1 << 0,
  HL_ATTRIB_ITALIC = 1 << 1,
  HL_ATTRIB_BOLD = 1 << 2,
  HL_ATTRIB_STRIKETHROUGH = 1 << 3,
  HL_ATTRIB_UNDERLINE = 1 << 4,
  HL_ATTRIB_UNDERCURL = 1 << 5
};

constexpr uint32_t DEFAULT_COLOR = 0x46464646;

struct HighlightAttribute {
  uint32_t foreground;
  uint32_t background;
  uint32_t special;
  uint16_t flags;
  HighlightAttribute *_default;

  uint32_t CreateForegroundColor() const {
    if (this->flags & HL_ATTRIB_REVERSE) {
      return this->background == DEFAULT_COLOR ? this->_default->background
                                               : this->background;
    } else {
      return this->foreground == DEFAULT_COLOR ? this->_default->foreground
                                               : this->foreground;
    }
  }

  uint32_t CreateBackgroundColor() const {
    if (this->flags & HL_ATTRIB_REVERSE) {
      return this->foreground == DEFAULT_COLOR ? this->_default->foreground
                                               : this->foreground;
    } else {
      return this->background == DEFAULT_COLOR ? this->_default->background
                                               : this->background;
    }
  }

  uint32_t CreateSpecialColor() const {
    return this->special == DEFAULT_COLOR ? this->_default->special
                                          : this->special;
  }
};

using HighlightAttributes = std::vector<HighlightAttribute>;
