#pragma once
#if TARGET_PC_NATIVE_UI

#include "JSystem/J2DGraph/J2DPane.h"

struct dALBWPaneBounds {
    f32  x     = 0.0f;
    f32  y     = 0.0f;
    f32  w     = 0.0f;
    f32  h     = 0.0f;
    bool valid = false;
};

dALBWPaneBounds dALBW_calcPaneScreenBounds(J2DPane* pane);

void dALBW_wrapMesgWords(char* dst, size_t dstCap, const char* src, f32 maxWidth,
                         class JUTFont* font, f32 fontSizeX, f32 charSpace = 0.0f);

/// Pixel height J2DPrint::parse() uses for this text at boxWidth (matches J2DTextBox layout).
f32 dALBW_mesgParseHeight(const char* text, f32 boxWidth, class JUTFont* font, f32 fontSizeX,
                          f32 fontSizeY, f32 charSpace, f32 lineSpace);

#endif
