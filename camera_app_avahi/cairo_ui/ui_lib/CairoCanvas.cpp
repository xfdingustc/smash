
#include <stdlib.h>
#include <math.h>
#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>

#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"

#include "CairoCanvas.h"

#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_CACHE_IMAGE_H
#include FT_CACHE_SMALL_BITMAPS_H

namespace ui {

bool CairoCanvas::_fontLoaded = false;
FT_Library             CairoCanvas::_ft_library[FONT_NUM];
FT_Face                CairoCanvas::_ft_face[FONT_NUM];
cairo_user_data_key_t  CairoCanvas::_cr_key[FONT_NUM];
cairo_font_face_t*     CairoCanvas::_font_face[FONT_NUM];

CairoCanvas::CairoCanvas(int width, int height, int bitdepth)
    : _size(width, height)
{
    _colorDepth = bitdepth / 8;

    cairo_format_t format = CAIRO_FORMAT_RGB16_565;
    int stride = cairo_format_stride_for_width(format, width);
    //CAMERALOG("create Canvas: width = %d, height = %d, stride = %d",
    //    width, height, stride);

    _pBuffer = new COLOR[width * height];

    _surface = cairo_image_surface_create_for_data(
        (unsigned char *)_pBuffer, format, width, height, stride);
    _cr = cairo_create(_surface);

    if (!_fontLoaded) {
        createFontface();
    }
    if (_fontLoaded) {
        cairo_set_font_face(_cr, _font_face[FONT_ROBOTOMONO_Medium]);
    }

    _pLock = new CMutex("canvas mutex");
}

CairoCanvas::~CairoCanvas()
{
    // TODO:
    cairo_destroy(_cr);
    cairo_surface_destroy(_surface);
    delete _pLock;
    delete[] _pBuffer;
}

void CairoCanvas::LockCanvas()
{
    _pLock->Take();
}

void CairoCanvas::UnlockCanvas()
{
    _pLock->Give();
}

int CairoCanvas::Wmemset(UINT16* pBufer, UINT16 col, int length)
{
    int r = length;
    UINT32 color = col;
    color = (color << 16)|col;
    UINT32* p;
    while (r > 0) {
        if ((UINT32)pBufer % 4 == 0) {
            if (r >= 8) {
                p = (UINT32*)pBufer;
                p[0] = color;
                p[1] = color;
                p[2] = color;
                p[3] = color;
                pBufer += 8;
                r -= 8;
            } else {
                pBufer[0] = col;
                pBufer ++;
                r --;
            }
        } else {
            pBufer[0] = col;
            pBufer ++;
            r --;
        }
    }
    return 0;
}

void CairoCanvas::colorConvert(COLOR c, float &r, float &g, float &b)
{
    r = ((c & 0xF800) >> 11) / 31.0;
    g = ((c & 0x07E0) >> 5) / 63.0;
    b = (c & 0x001F) / 31.0;
}

bool CairoCanvas::createFontface()
{
    const char *fonts[] = {
        "/usr/share/fonts/truetype/RobotoMono-Medium.ttf",
        "/usr/share/fonts/truetype/RobotoMono-Bold.ttf",
        "/usr/share/fonts/truetype/RobotoMono-Regular.ttf",
        "/usr/share/fonts/truetype/RobotoMono-Light.ttf",
        "/usr/share/fonts/truetype/Roboto-Regular.ttf"
    };

    int error = 0;
    cairo_status_t status;
    for (int i = 0; i < FONT_NUM; i++) {
        error = FT_Init_FreeType(&(_ft_library[i]));
        if (error) {
            CAMERALOG("FT_Init_FreeType() failed with error = %d", error);
        }
        error = FT_New_Face(_ft_library[i],
            fonts[i],
            0,
            &(_ft_face[i]));
        if (error) {
            CAMERALOG("FT_New_Face() failed with error = %d", error);
        }

        _font_face[i] = cairo_ft_font_face_create_for_ft_face(
            _ft_face[i], 0);
        status = cairo_font_face_set_user_data(_font_face[i],
            &(_cr_key[i]), _ft_face[i],
            (cairo_destroy_func_t)FT_Done_Face);
        if (status) {
           cairo_font_face_destroy(_font_face[i]);
           FT_Done_Face(_ft_face[i]);
           return false;
        }
    }

    _fontLoaded = true;

    return true;
}

void CairoCanvas::FillAll(COLOR c)
{
    if (c == 0x00) {
        memset(_pBuffer, 0x00, _size.height * _size.width * _colorDepth);
    } else {
        Wmemset(_pBuffer, c, _size.height * _size.width);
    }
}

void CairoCanvas::DrawPoint(const Point& p, COLOR c)
{
    if (p.y *_size.width + p.x < _size.height*_size.width)
    {
        *(_pBuffer + p.y *_size.width + p.x) = c;
    }
}

void CairoCanvas::DrawPoint(UINT16 x, UINT16 y, COLOR c)
{
    if (y *_size.width + x < _size.height*_size.width)
    {
        *(_pBuffer + y *_size.width + x) = c;
    }
}

void CairoCanvas::DrawRow(const Point& p, UINT16 endX, COLOR c)
{
    Wmemset(_pBuffer + p.y  * _size.width + p.x, c, endX - p.x);
}

void CairoCanvas::DrawRow(UINT16 x, UINT16 y, UINT16 length, COLOR c)
{
    Wmemset(_pBuffer + y  * _size.width + x, c, length);
}

void CairoCanvas::DrawLine(const Point& p1, const Point& p2, int width, COLOR color)
{
    cairo_set_dash(_cr, NULL, 0, 0.0);
    cairo_set_line_width(_cr, width);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);
    cairo_move_to(_cr, p1.x, p1.y);
    cairo_line_to(_cr, p2.x, p2.y);
    cairo_stroke(_cr);
}

void CairoCanvas::DrawDashLine(const Point& p1, const Point& p2, int width, COLOR color)
{
    // TODO: more styles
    double dashes[] = {
            4.0,  /* ink */
            16.0,  /* skip */
            4.0,  /* ink */
            16.0   /* skip*/
        };
    int ndash = sizeof (dashes)/sizeof(dashes[0]);
    double offset = 0.0;
    cairo_set_dash(_cr, dashes, ndash, offset);

    cairo_set_line_width(_cr, width);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);
    cairo_move_to(_cr, p1.x, p1.y);
    cairo_line_to(_cr, p2.x, p2.y);
    cairo_stroke(_cr);
}

void CairoCanvas::DrawRect(
    const Point& lefttop, const Point& rightbottom,
    COLOR color, UINT16 radius, float alpha)
{
    cairo_set_line_width(_cr, 1.0);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgba(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31, alpha);

    cairo_rectangle(_cr, lefttop.x, lefttop.y,
        rightbottom.x - lefttop.x, rightbottom.y - lefttop.y);
    cairo_set_line_join (_cr, CAIRO_LINE_JOIN_ROUND);
    cairo_fill(_cr);
}

void CairoCanvas::DrawRect(
    const Point& lefttop, const Size& size,
    COLOR color, UINT16 radius, float alpha)
{
    cairo_set_line_width(_cr, 1.0);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgba(_cr, ((float)b) / 31, ((float)g) / 63, ((float)r) / 31, alpha);

    if (radius < 5.0) {
        cairo_rectangle(_cr, lefttop.x, lefttop.y, size.width, size.height);
        cairo_set_line_join (_cr, CAIRO_LINE_JOIN_ROUND);
        cairo_fill(_cr);
    } else {
        cairo_move_to(_cr, lefttop.x + radius, lefttop.y);
        cairo_line_to(_cr, lefttop.x + size.width - radius, lefttop.y);

        cairo_move_to(_cr, lefttop.x + size.width, lefttop.y + radius);
        cairo_line_to(_cr, lefttop.x + size.width, lefttop.y + size.height - radius);

        cairo_move_to(_cr, lefttop.x + size.width - radius, lefttop.y + size.height);
        cairo_line_to(_cr, lefttop.x + radius, lefttop.y + size.height);

        cairo_move_to(_cr, lefttop.x, lefttop.y + size.height - radius);
        cairo_line_to(_cr, lefttop.x, lefttop.y + radius);

        cairo_arc(_cr, lefttop.x + radius,
            lefttop.y + radius, radius, M_PI, 3 * M_PI / 2.0);
        cairo_arc(_cr, lefttop.x + size.width - radius,
            lefttop.y + radius, radius, 3 * M_PI / 2, 2 * M_PI);
        cairo_arc(_cr, lefttop.x + size.width - radius,
            lefttop.y + size.height - radius, radius, 0, M_PI / 2);
        cairo_arc(_cr, lefttop.x + radius,
            lefttop.y + size.height - radius, radius, M_PI / 2, M_PI);

        cairo_fill(_cr);
    }
}

void CairoCanvas::DrawRectStroke(
    const Point& lefttop, const Size& size,
    COLOR color, UINT16 linewidth, UINT16 radius, float alpha)
{
    cairo_set_line_width(_cr, linewidth);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgba(_cr, ((float)b) / 31, ((float)g) / 63, ((float)r) / 31, alpha);

    if (radius < 5.0) {
        cairo_rectangle(_cr, lefttop.x, lefttop.y, size.width, size.height);
        cairo_set_line_join (_cr, CAIRO_LINE_JOIN_ROUND);
        cairo_stroke(_cr);
    } else {
        cairo_move_to(_cr, lefttop.x + radius, lefttop.y);
        cairo_line_to(_cr, lefttop.x + size.width - radius, lefttop.y);

        cairo_move_to(_cr, lefttop.x + size.width, lefttop.y + radius);
        cairo_line_to(_cr, lefttop.x + size.width, lefttop.y + size.height - radius);

        cairo_move_to(_cr, lefttop.x + size.width - radius, lefttop.y + size.height);
        cairo_line_to(_cr, lefttop.x + radius, lefttop.y + size.height);

        cairo_move_to(_cr, lefttop.x, lefttop.y + size.height - radius);
        cairo_line_to(_cr, lefttop.x, lefttop.y + radius);

        cairo_arc(_cr, lefttop.x + radius,
            lefttop.y + radius, radius, M_PI, 3 * M_PI / 2.0);
        cairo_arc(_cr, lefttop.x + size.width - radius,
            lefttop.y + radius, radius, 3 * M_PI / 2, 2 * M_PI);
        cairo_arc(_cr, lefttop.x + size.width - radius,
            lefttop.y + size.height - radius, radius, 0, M_PI / 2);
        cairo_arc(_cr, lefttop.x + radius,
            lefttop.y + size.height - radius, radius, M_PI / 2, M_PI);

        cairo_stroke(_cr);
    }
}

void CairoCanvas::DrawRectGradient(
    const Point& lefttop,
    const Size& size,
    COLOR c1,
    COLOR c2)
{
    float r1, g1, b1;
    float r2, g2, b2;
    colorConvert(c1, r1, g1, b1);
    colorConvert(c2, r2, g2, b2);

    cairo_pattern_t *pat;
    pat = cairo_pattern_create_linear(lefttop.x, lefttop.y,
        lefttop.x, lefttop.y + size.height);
    if (!pat) {
        CAMERALOG("create linear pattern failed");
        return;
    }
    cairo_pattern_add_color_stop_rgb(pat, 0.0, r1, g1, b1);
    cairo_pattern_add_color_stop_rgb(pat, 0.3, r2, g2, b2);
    cairo_pattern_add_color_stop_rgb(pat, 0.7, r2, g2, b2);
    cairo_pattern_add_color_stop_rgb(pat, 1.0, r1, g1, b1);

    cairo_rectangle(_cr, lefttop.x, lefttop.y,
        size.width, size.height);
    cairo_set_source(_cr, pat);
    cairo_fill(_cr);
    cairo_pattern_destroy(pat);
}

void CairoCanvas::DrawRectGradient(
    const Point& lefttop,
    const Size& size,
    COLOR c1, float alpha1,
    COLOR c2, float alpha2)
{
    float r1, g1, b1;
    float r2, g2, b2;
    colorConvert(c1, r1, g1, b1);
    colorConvert(c2, r2, g2, b2);

    cairo_pattern_t *pat;
    pat = cairo_pattern_create_linear(lefttop.x, lefttop.y,
        lefttop.x, lefttop.y + size.height);
    if (!pat) {
        CAMERALOG("create linear pattern failed");
        return;
    }
    cairo_pattern_add_color_stop_rgba(pat, 0.0, r1, g1, b1, alpha1);
    cairo_pattern_add_color_stop_rgba(pat, 1.0, r2, g2, b2, alpha2);

    cairo_rectangle(_cr, lefttop.x, lefttop.y,
        size.width, size.height);
    cairo_set_source(_cr, pat);
    cairo_fill(_cr);
    cairo_pattern_destroy(pat);
}

void CairoCanvas::DrawRound(const Point& center, UINT16 radius, COLOR color)
{
    cairo_set_line_width(_cr, 1.0);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);
    cairo_arc(_cr, center.x, center.y, radius, ANGLE(0), ANGLE(360));
    cairo_fill(_cr);
}

void CairoCanvas::DrawRound(const Point& center, UINT16 radius, COLOR color, float alpha)
{
    cairo_set_line_width(_cr, 1.0);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgba(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31, alpha);
    cairo_arc(_cr, center.x, center.y, radius, ANGLE(0), ANGLE(360));
    cairo_fill(_cr);
}

void CairoCanvas::DrawCircle(
    const Point& center,
    UINT16 radius,
    UINT16 borderWidth,
    COLOR borderColor)
{
    cairo_set_line_width(_cr, borderWidth);
    int r = (borderColor & 0xF800) >> 11;
    int g = (borderColor & 0x07E0) >> 5;
    int b = borderColor & 0x001F;
    cairo_move_to(_cr,
        center.x + radius,
        center.y);
    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);
    cairo_arc(_cr, center.x, center.y, radius, ANGLE(0), ANGLE(360));
    cairo_stroke(_cr);
}

void CairoCanvas::DrawArc(
    const Point& center,
    UINT16 radius,
    UINT16 borderWidth,
    COLOR borderColor,
    float angle_start,
    float angle_end)
{
    cairo_set_line_width(_cr, borderWidth);
    cairo_set_line_cap (_cr, CAIRO_LINE_CAP_ROUND);
    int r = (borderColor & 0xF800) >> 11;
    int g = (borderColor & 0x07E0) >> 5;
    int b = borderColor & 0x001F;

    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);
    if (angle_start == angle_end) {
        cairo_move_to(_cr,
            center.x + radius,
            center.y);
        cairo_arc(_cr, center.x, center.y, radius, ANGLE(0), ANGLE(360));
    } else {
        cairo_move_to(_cr,
            center.x + radius * cos(ANGLE(angle_start)),
            center.y + radius * sin(ANGLE(angle_start)));
        cairo_arc(_cr, center.x, center.y, radius, ANGLE(angle_start), ANGLE(angle_end));
    }
    cairo_stroke(_cr);
}

void CairoCanvas::DrawRing(
    const Point& center, UINT16 radius_outer, UINT16 radius_inner, COLOR color)
{
    cairo_set_line_width(_cr, radius_outer - radius_inner);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);

    cairo_move_to(_cr,
        center.x + radius_inner,
        center.y);
    cairo_line_to(_cr,
        center.x + radius_outer,
        center.y);
    cairo_arc_negative(_cr, center.x, center.y, radius_outer, ANGLE(0), ANGLE(180));
    cairo_line_to(_cr,
        center.x - radius_inner,
        center.y);
    cairo_arc(_cr, center.x, center.y, radius_inner, ANGLE(180), ANGLE(0));
    cairo_fill(_cr);

    cairo_move_to(_cr,
        center.x - radius_inner,
        center.y);
    cairo_line_to(_cr,
        center.x - radius_outer,
        center.y);
    cairo_arc_negative(_cr, center.x, center.y, radius_outer, ANGLE(180), ANGLE(360));
    cairo_line_to(_cr,
        center.x + radius_inner,
        center.y);
    cairo_arc(_cr, center.x, center.y, radius_inner, ANGLE(360), ANGLE(180));
    cairo_fill(_cr);
}

void CairoCanvas::DrawFan(
    const Point& center, UINT16 radius, float angle_A, float angle_B, COLOR color)
{
    if (angle_A < 0.0 || angle_B < 0.0 || angle_A > 360.0 || angle_B > 360.0) {
        return;
    }

    if ((angle_A == angle_B) || (abs(angle_A - angle_B) == 360.0)) {
        DrawRound(center, radius, color);
        return;
    }

    cairo_set_line_width(_cr, 1.0);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);
    cairo_move_to(_cr, center.x, center.y);
    cairo_line_to(_cr,
        center.x + radius * cos(ANGLE(angle_A)),
        center.y + radius * sin(ANGLE(angle_A)));
    cairo_arc_negative(_cr, center.x, center.y, radius, ANGLE(angle_A), ANGLE(angle_B));
    cairo_line_to(_cr,
        center.x + radius * cos(ANGLE(angle_B)),
        center.y + radius * sin(ANGLE(angle_B)));
    cairo_fill(_cr);
}

void CairoCanvas::DrawFanRing(
    const Point& center,
    UINT16 radius_outer, UINT16 radius_inner,
    float angle_A, float angle_B,
    COLOR color)
{
    if (angle_A < 0.0 || angle_B < 0.0 || angle_A > 360.0 || angle_B > 360.0) {
        return;
    }

    if (radius_outer <= radius_inner) {
        return;
    }

    if ((angle_A == angle_B) || (abs(angle_A - angle_B) == 360.0)) {
        DrawRing(center, radius_outer, radius_inner, color);
        return;
    }

    cairo_set_line_width(_cr, 1.0);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);
    cairo_move_to(_cr,
        center.x + radius_inner * cos(ANGLE(angle_A)),
        center.y + radius_inner * sin(ANGLE(angle_A)));
    cairo_line_to(_cr,
        center.x + radius_outer * cos(ANGLE(angle_A)),
        center.y + radius_outer * sin(ANGLE(angle_A)));
    cairo_arc_negative(_cr, center.x, center.y, radius_outer, ANGLE(angle_A), ANGLE(angle_B));
    cairo_line_to(_cr,
        center.x + radius_inner * cos(ANGLE(angle_B)),
        center.y + radius_inner * sin(ANGLE(angle_B)));
    cairo_arc(_cr, center.x, center.y, radius_inner, ANGLE(angle_B), ANGLE(angle_A));
    cairo_fill(_cr);
}

void CairoCanvas::DrawCap(
    const Point& center,
    UINT16 radius,
    float angle_A,
    float angle_B,
    COLOR color)
{
    //CAMERALOG("radius = %d, angle_A = %f, angle_B = %f",
    //    radius, angle_A, angle_B);

    cairo_set_line_width(_cr, 1.0);
    int r = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int b = color & 0x001F;
    cairo_set_source_rgb(_cr, ((float)r) / 31, ((float)g) / 63, ((float)b) / 31);

    cairo_move_to(_cr,
        center.x + radius * cos(angle_A),
        center.y + radius * sin(angle_A));
    cairo_arc_negative(_cr, center.x, center.y, radius, angle_A, angle_B);
    cairo_line_to(_cr,
        center.x + radius * cos(angle_A),
        center.y + radius * sin(angle_A));

    cairo_fill(_cr);
}

void CairoCanvas::DrawText(
    const char *pTxt, FONT fonttype, int txtSize, COLOR color,
    const Point& leftTop, const Size& areaSize,
    int h_align, int v_align, float alpha, int line_height)
{
    if (!pTxt || (txtSize == 0) || (strlen(pTxt) <= 0)) {
        return;
    }

    if (!_fontLoaded || (fonttype >= FONT_NUM)) {
        return;
    }

    cairo_set_font_face(_cr, _font_face[fonttype]);

    int b = (color & 0xF800) >> 11;
    int g = (color & 0x07E0) >> 5;
    int r = color & 0x001F;
    cairo_set_source_rgba(_cr, ((float)b) / 31, ((float)g) / 63, ((float)r) / 31, alpha);
    cairo_set_font_size(_cr, txtSize);

    cairo_font_extents_t fe;
    cairo_font_extents(_cr, &fe);

    cairo_text_extents_t te;
    cairo_text_extents(_cr, pTxt, &te);

#if 0
    CAMERALOG("fe.ascent = %f, fe.descent = %f, "
        "fe.height = %f, "
        "fe.max_x_advance = %f, fe.max_y_advance = %f",
        fe.ascent, fe.descent,
        fe.height,
        fe.max_x_advance, fe.max_y_advance);

    CAMERALOG("pTxt = %s, txtSize = %d, "
        "te.x_bearing = %f, te.y_bearing = %f, "
        "te.width = %f, te.height = %f, "
        "te.x_advance = %f, te.y_advance = %f\n",
        pTxt, txtSize,
        te.x_bearing, te.y_bearing,
        te.width, te.height,
        te.x_advance, te.y_advance);
#endif

    UINT16 height_l = fe.height;
    if (height_l < line_height) {
        height_l = line_height;
    }
    if (height_l == 0) {
        return;
    }

    // Firstly, get how many lines needed to show all the text.
    int total_lines = 0;
    char *str = (char *)pTxt;
    char *str1 = NULL;
    char txt[256];
    do {
        if ((str1 = strstr(str, "\n")) != NULL) {
            strncpy(txt, str, str1 - str);
            txt[str1 - str] = 0x00;
            str = str1 + 1;
        } else {
            strcpy(txt, str);
        }

        if (strlen(txt) == 0) {
            total_lines++;
        } else {
            cairo_text_extents(_cr, txt, &te);
            UINT16 txt_width = te.x_advance;
            int l = 0;
            if (areaSize.width > 0) {
                l = txt_width / areaSize.width +
                    ((txt_width % areaSize.width != 0) ? 1 : 0);
            }
            total_lines += l;
        }
    } while (str1 != NULL);
    //CAMERALOG("pTxt = %s, total_lines = %d", pTxt, total_lines);

    // Secondly, get how many lines can be drawn in the text area.
    int lines = areaSize.height / height_l;
    if (lines > total_lines) {
        lines = total_lines;
    }

    // Thirdly, draw text.
    float pos_x = leftTop.x, pos_y = leftTop.y;
    switch (v_align) {
        case TOP:
            pos_y = leftTop.y + te.height;
            break;
        case MIDDLE:
            pos_y = leftTop.y + areaSize.height - (areaSize.height - te.height) / 2;
            if(lines % 2) {
                pos_y = pos_y - lines / 2 * height_l;
            } else {
                pos_y = pos_y - lines / 2 * height_l + te.height / 2;
            }
            break;
        case BOTTOM:
            pos_y = leftTop.y + areaSize.height;
            pos_y = pos_y - lines * height_l;
            break;
        default:
            break;
    }

    str = (char *)pTxt;
    str1 = NULL;
    int lines_drawn = 0;
    do {
        if ((str1 = strstr(str, "\n")) != NULL) {
            strncpy(txt, str, str1 - str);
            txt[str1 - str] = 0x00;
            str = str1 + 1;
        } else {
            strcpy(txt, str);
        }

        if (strlen(txt) == 0) {
            //CAMERALOG("the %dth line: %s", lines_drawn, txt);
            lines_drawn ++;
        } else {
            cairo_text_extents(_cr, txt, &te);
            UINT16 txt_width = te.x_advance;
            int num = areaSize.width / (txt_width / strlen(txt));
            char txt_to_draw[256];
            int l = txt_width / areaSize.width +
                ((txt_width % areaSize.width != 0) ? 1 : 0);
            int txt_len = strlen(txt);
            //CAMERALOG("txt = %s, l = %d, txt_width = %d, areaSize.width = %d, "
            //    "num = %d, txt_len = %d",
            //    txt, l, txt_width, areaSize.width, num, txt_len);
            for (int i = 0; (i < l) && (lines_drawn < lines); i++) {
                int len = (txt_len - i * num > 0) ?
                    ((num > txt_len - i * num) ? (txt_len - i * num) : num)
                    : 0;
                memset(txt_to_draw, 0x00, 256);
                strncpy(txt_to_draw, txt + i * num, len);
                txt_to_draw[len] = 0x00;
                cairo_text_extents(_cr, txt_to_draw, &te);
                switch (h_align) {
                    case LEFT:
                        pos_x = leftTop.x - te.x_bearing;
                        break;
                    case CENTER:
                        pos_x = leftTop.x + (areaSize.width - te.x_advance) / 2 - te.x_bearing;
                        break;
                    case RIGHT:
                        pos_x = leftTop.x + (areaSize.width - te.x_advance) - te.x_bearing;
                        break;
                    default:
                        break;
                }
                int space_num = 0;
                while (txt_to_draw[space_num] == ' ') {
                    space_num ++;
                }
                cairo_move_to(_cr,
                    pos_x + space_num * (txt_width / strlen(txt)),
                    pos_y + lines_drawn * height_l);
                cairo_show_text(_cr, txt_to_draw);
                //CAMERALOG("the %dth line: %s", lines_drawn, txt_to_draw);
                lines_drawn ++;
            }
        }
    } while ((str1 != NULL) && (lines_drawn < lines));
}

void CairoCanvas::DrawPng(const char *path, const Point& leftTop, const Size& areaSize) {
    if (!path) {
        CAMERALOG("Invalid image, path is NULL!");
        return;
    }
    int w, h;
    cairo_surface_t *image;
    cairo_pattern_t *pattern;
    cairo_matrix_t   matrix;

    image = cairo_image_surface_create_from_png(path);
    if (!image) {
        CAMERALOG("create image %s failed", path);
        return;
    }
    w = cairo_image_surface_get_width(image);
    h = cairo_image_surface_get_height(image);

    pattern = cairo_pattern_create_for_surface(image);
    cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);

    cairo_matrix_init_scale(&matrix, 1.0, 1.0);
    cairo_pattern_set_matrix(pattern, &matrix);

    cairo_set_source(_cr, pattern);
    cairo_rectangle(_cr, leftTop.x, leftTop.y, w, h);
    cairo_fill(_cr);

    cairo_surface_destroy(image);
}

void CairoCanvas::DrawBmp(
    const Point& lefttop,
    const Size& size,
    CBmp* bmp)
{
    if (!bmp)
    {
        CAMERALOG("Invalid bmp image!");
        return;
    }

    if ((size.width <= 0) || (size.height <= 0)
        || (size.width > _size.width) || (size.height > _size.height))
    {
        CAMERALOG("Invalid image size(%d, %d)!", size.width, size.height);
        return;
    }

    UINT8* line;
    if (bmp->getColorDepth() == 16)
    {
        for (int i = lefttop.y; i < lefttop.y + size.height; i++)
        {
            line = bmp->GetBuffer()
                + bmp->GetPitch() * (bmp->getHeight() - (i - lefttop.y) % bmp->getHeight() - 1);
            memcpy(_pBuffer + i * _size.width + lefttop.x,
                line,
                size.width * 2);
        }
    }
}

};

