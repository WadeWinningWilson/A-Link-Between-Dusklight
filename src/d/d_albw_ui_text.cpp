// ============================================
// NEW CODE — ALBW Port
// Shared J2D mesg-font helpers for native shop + dialogue UIs.
// ============================================
#include "d/d_albw_ui_text.h"

#if TARGET_PC_NATIVE_UI

#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPrint.h"
#include "JSystem/JUtility/JUTFont.h"
#include "JSystem/JUtility/TColor.h"
#include "d/d_pane_class.h"
#include "m_Do/m_Do_graphic.h"
#include <cmath>
#include <cstdio>
#include <cstring>

dALBWPaneBounds dALBW_calcPaneScreenBounds(J2DPane* pane) {
    dALBWPaneBounds out;
    if (!pane) {
        return out;
    }

    CPaneMgr scratch;
    Mtx mtx;
    const Vec tl = scratch.getGlobalVtx(pane, &mtx, 0, false, 0);
    const Vec br = scratch.getGlobalVtx(pane, &mtx, 3, false, 0);

    const f32 scX = mDoGph_gInf_c::getWidthF() / FB_WIDTH;
    const f32 scY = mDoGph_gInf_c::getHeightF() / FB_HEIGHT;
    out.x     = (tl.x - mDoGph_gInf_c::getMinXF()) / scX;
    out.y     = tl.y / scY;
    out.w     = (br.x - tl.x) / scX;
    out.h     = (br.y - tl.y) / scY;
    out.valid = (out.w > 1.0f && out.h > 1.0f);
    return out;
}

static f32 mesgCharWidth(JUTFont* font, int ch, f32 fontSizeX) {
    if (!font || fontSizeX <= 0.0f) {
        return 0.0f;
    }
    JUTFont::TWidth width;
    font->getWidthEntry(ch, &width);
    return (f32)width.field_0x1 * (fontSizeX / (f32)font->getCellWidth());
}

static f32 mesgTextWidth(JUTFont* font, const char* text, f32 fontSizeX, f32 charSpace) {
    f32 w     = 0.0f;
    int chars = 0;
    for (const u8* p = (const u8*)text; *p != '\0'; ++p) {
        w += mesgCharWidth(font, *p, fontSizeX);
        ++chars;
    }
    if (chars > 1 && charSpace > 0.0f) {
        w += charSpace * (f32)(chars - 1);
    }
    return w;
}

void dALBW_wrapMesgWords(char* dst, size_t dstCap, const char* src, f32 maxWidth, JUTFont* font,
                         f32 fontSizeX, f32 charSpace) {
    if (!dst || dstCap == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src || src[0] == '\0' || maxWidth <= 1.0f || !font) {
        if (src && dstCap > 1) {
            strncpy(dst, src, dstCap - 1);
            dst[dstCap - 1] = '\0';
        }
        return;
    }

    size_t outLen = 0;
    auto emit = [&](char c) -> bool {
        if (outLen + 1 >= dstCap) {
            return false;
        }
        dst[outLen++] = c;
        dst[outLen]   = '\0';
        return true;
    };

    auto flushLine = [&](char* line) {
        if (!line[0]) {
            return;
        }
        for (char* c = line; *c != '\0'; ++c) {
            if (!emit(*c)) {
                return;
            }
        }
        emit('\n');
        line[0] = '\0';
    };

    const char* p = src;
    while (*p != '\0' && outLen + 1 < dstCap) {
        while (*p == '\n') {
            if (!emit('\n')) {
                return;
            }
            ++p;
        }
        if (*p == '\0') {
            break;
        }

        char line[256];
        line[0] = '\0';

        while (*p != '\0' && *p != '\n' && outLen + 1 < dstCap) {
            while (*p == ' ') {
                ++p;
            }
            if (*p == '\0' || *p == '\n') {
                break;
            }

            const char* wStart = p;
            while (*p != '\0' && *p != ' ' && *p != '\n') {
                ++p;
            }

            char word[128];
            const size_t wLen = (size_t)(p - wStart);
            if (wLen == 0 || wLen >= sizeof(word)) {
                continue;
            }
            memcpy(word, wStart, wLen);
            word[wLen] = '\0';

            char trial[280];
            if (line[0] != '\0') {
                snprintf(trial, sizeof(trial), "%s %s", line, word);
            } else {
                snprintf(trial, sizeof(trial), "%s", word);
            }

            if (line[0] != '\0' && mesgTextWidth(font, trial, fontSizeX, charSpace) > maxWidth) {
                flushLine(line);
                snprintf(trial, sizeof(trial), "%s", word);
            }

            if (line[0] == '\0' && mesgTextWidth(font, word, fontSizeX, charSpace) > maxWidth) {
                for (size_t i = 0; i < wLen && outLen + 1 < dstCap; ++i) {
                    char next[2] = {word[i], '\0'};
                    char probe[256];
                    if (line[0] != '\0') {
                        snprintf(probe, sizeof(probe), "%s%s", line, next);
                    } else {
                        snprintf(probe, sizeof(probe), "%s", next);
                    }
                    if (line[0] != '\0' && mesgTextWidth(font, probe, fontSizeX, charSpace) > maxWidth) {
                        flushLine(line);
                    }
                    const size_t ll = strlen(line);
                    line[ll]     = word[i];
                    line[ll + 1] = '\0';
                }
            } else {
                if (line[0] != '\0') {
                    const size_t ll = strlen(line);
                    line[ll]     = ' ';
                    strncpy(line + ll + 1, word, sizeof(line) - ll - 2);
                    line[sizeof(line) - 1] = '\0';
                } else {
                    strncpy(line, word, sizeof(line) - 1);
                    line[sizeof(line) - 1] = '\0';
                }
            }
        }

        flushLine(line);
        if (*p == '\n') {
            ++p;
        }
    }

    if (outLen > 0 && dst[outLen - 1] == '\n') {
        dst[--outLen] = '\0';
    }
}

f32 dALBW_mesgParseHeight(const char* text, f32 boxWidth, JUTFont* font, f32 fontSizeX,
                          f32 fontSizeY, f32 charSpace, f32 lineSpace) {
    if (!text || text[0] == '\0' || !font || boxWidth <= 1.0f || fontSizeX <= 0.0f) {
        return 0.0f;
    }

    const JUtility::TColor white(255, 255, 255, 255);
    J2DPrint print(font, charSpace, lineSpace, white, white, JUtility::TColor(0, 0, 0, 0), white);
    print.setFontSize(fontSizeX, fontSizeY);

    u16                 widths[260];
    J2DPrint::TSize     size;
    const int           len = (int)strlen(text);
    const f32           h   = print.parse((const u8*)text, len, (s32)(boxWidth + 0.0001f), widths,
                                          size, 255, false);
    const f32 ascent = font->getAscent() * (fontSizeY / (f32)font->getCellHeight());
    return h + ascent;
}

#endif
