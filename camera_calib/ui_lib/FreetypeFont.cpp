

#include "FreetypeFont.h"

namespace ui {

FreetypeFont* FreetypeFont::_pInstance = NULL;

void FreetypeFont::Create(const char *font_path) {
    if (_pInstance == NULL) {
        _pInstance = new FreetypeFont(font_path);
    }
}

void FreetypeFont::Destroy() {
    if (_pInstance != NULL) {
        printf("delete ui FreetypeFont\n");
        delete _pInstance;
    }
}

FreetypeFont::FreetypeFont(const char *font_path) {
    int error = FT_Init_FreeType(&_ft_library);
    if (error) {
        printf("FT_Init_FreeType() failed with error = %d\n", error);
    }
    error = FT_New_Face(
        _ft_library, font_path, 0, &_ft_face);
    if (error) {
        printf("FT_New_Face() failed with error = %d\n", error);
    }
}

FreetypeFont::~FreetypeFont() {
    // TODO:
}


};
