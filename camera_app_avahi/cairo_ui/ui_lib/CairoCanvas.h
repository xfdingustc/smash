
#ifndef __IN_CairoCanvas_h__
#define __IN_CairoCanvas_h__

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_CACHE_IMAGE_H
#include FT_CACHE_SMALL_BITMAPS_H

#include <cairo.h>
#include "CBmp.h"
#include "ClinuxThread.h"
#include "GobalMacro.h"

namespace ui {

typedef UINT16 COLOR;

#define ANGLE(ang)  ((ang) / 180.0 * M_PI)

enum H_ALIGN
{
    LEFT = 0,
    CENTER,
    RIGHT,
};

enum V_ALIGN
{
    TOP = 0,
    MIDDLE,
    BOTTOM,
};

struct Point
{
    int x;
    int y;
    Point() : x(0), y(0) {}
    Point(int xx, int yy) { x = xx; y = yy; }
    Point(const Point& point) { x = point.x; y = point.y; }
    Point& operator = (const Point& point)
    {
        x = point.x;
        y = point.y;
        return *this;
    }
    void Set( int xx, int yy) { x = xx; y = yy; }
    void Mov(int xx, int yy) { x += xx; y+= yy; }
};

struct Size
{
    UINT16 width;
    UINT16 height;
    Size() : width(0), height(0) {}
    Size(UINT16 w, UINT16 h) { width = w; height = h; }
    Size(const Size& size) { width = size.width; height = size.height; }
    Size& operator = (const Size& size)
    {
        width = size.width;
        height = size.height;
        return *this;
    }
    void Set( UINT16 w, UINT16 h) { width = w;  height = h; }
};

typedef enum FONT {
    FONT_ROBOTOMONO_Medium = 0,
    FONT_ROBOTOMONO_Bold,
    FONT_ROBOTOMONO_Regular,
    FONT_ROBOTOMONO_Light,
    FONT_ROBOTO_Regular,
    FONT_NUM,
} FONT;

class CairoCanvas
{
public:
    CairoCanvas(int width, int height, int bitdepth);
    ~CairoCanvas();

    void LockCanvas();
    void UnlockCanvas();

    void FillAll(COLOR c);

    Size& GetSize() { return _size; }
    COLOR* GetBuffer() { return _pBuffer; }

    UINT8 GetByteDepth() { return _colorDepth; }

    void DrawPoint(const Point& p, COLOR c);
    void DrawPoint(UINT16 x, UINT16 y, COLOR c);
    void DrawRow(const Point& p, UINT16 endX, COLOR c);
    void DrawRow(UINT16 x, UINT16 y, UINT16 endX, COLOR c);

    void DrawLine(const Point& p1, const Point& p2, int width, COLOR color);
    void DrawDashLine(const Point& p1, const Point& p2, int width, COLOR color);
    void DrawRect(
        const Point& lefttop,
        const Point& rightbottom,
        COLOR c,
        UINT16 radius = 5,
        float alpha = 1.0);
    void DrawRect(
        const Point& lefttop,
        const Size& size,
        COLOR c,
        UINT16 radius = 5,
        float alpha = 1.0);
    void DrawRectStroke(
        const Point& lefttop,
        const Size& size,
        COLOR c,
        UINT16 linewidth = 1,
        UINT16 radius = 5,
        float alpha = 1.0);
    void DrawRectGradient(
        const Point& lefttop,
        const Size& size,
        COLOR c1,
        COLOR c2);
    void DrawRectGradient(
        const Point& lefttop,
        const Size& size,
        COLOR c1, float alpha1,
        COLOR c2, float alpha2);

    void DrawRound(const Point& center, UINT16 radius, COLOR color);
    void DrawRound(const Point& center, UINT16 radius, COLOR color, float alpha);
    void DrawCircle(
        const Point& center,
        UINT16 radius,
        UINT16 borderWidth,
        COLOR borderColor);
    void DrawArc(
        const Point& center,
        UINT16 radius,
        UINT16 borderWidth,
        COLOR borderColor,
        float angle_start,
        float angle_end);

    void DrawRing(
        const Point& center,
        UINT16 radius_outer,
        UINT16 radius_inner,
        COLOR color);

    void DrawFan(
        const Point& center,
        UINT16 radius,
        float angle_A,
        float angle_B,
        COLOR color);
    void DrawFan(
        const Point& center,
        UINT16 radius,
        float angle_A,
        float angle_B,
        COLOR color,
        COLOR borderColor);

    void DrawFanRing(
        const Point& center,
        UINT16 radius_outer,
        UINT16 radius_inner,
        float angle_A,
        float angle_B,
        COLOR color);

    void DrawCap(
        const Point& center,
        UINT16 radius,
        float angle_A,
        float angle_B,
        COLOR color);

    void DrawText(
        const char *pTxt, FONT fonttype, int txtSize, COLOR color,
        const Point& leftTop, const Size& areaSize,
        int h_align, int v_align, float alpha = 1.0, int line_height = 0);

    void DrawPng(const char *path, const Point& leftTop, const Size& areaSize);
    void DrawBmp(
        const Point& lefttop,
        const Size& size,
        CBmp* bmp);

private:
    int Wmemset(UINT16* pBufer, UINT16 col, int length);

    inline void colorConvert(COLOR c, float &r, float &g, float &b);

    static bool createFontface();

    Size            _size;
    COLOR           *_pBuffer;
    UINT8           _colorDepth;

    cairo_surface_t *_surface;
    cairo_t *_cr;

    CMutex  *_pLock;

    static bool                   _fontLoaded;
    static FT_Library             _ft_library[FONT_NUM];
    static FT_Face                _ft_face[FONT_NUM];
    static cairo_user_data_key_t  _cr_key[FONT_NUM];
    static cairo_font_face_t      *_font_face[FONT_NUM];
};

};

#endif
