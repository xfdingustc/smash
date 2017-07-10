

#ifndef __IN_FreetypeFont_h__
#define __IN_FreetypeFont_h__

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_CACHE_IMAGE_H
#include FT_CACHE_SMALL_BITMAPS_H

namespace ui {

class FreetypeFont {
public:
    static void Create(const char *font_path);
    static void Destroy();
    static FreetypeFont* getInstance() { return _pInstance; }

    FT_Library* getFTLib() { return &_ft_library; }
    FT_Face* getFTFace() { return &_ft_face; }

private:
    FreetypeFont(const char *font_path);
    ~FreetypeFont();

    static FreetypeFont *_pInstance;

    FT_Library      _ft_library;
    FT_Face         _ft_face;
};

};

#endif
