#ifndef _AC_BITMAP_FONT_H_
#define _AC_BITMAP_FONT_H_

#include "acu.h"
#include "acFont.h"


class acFreeTypeFont : public acFont {
public:
  acFreeTypeFont(const char *filename);

  float getDescent();
  float getHeight();
  float charWidth(char c);
  float charHeight(char c);
  boolean charExists(char c);
  void drawChar(char c, float x, float y);

  unsigned char* getCharData(char c, float *x, float *y, 
			     float *w, float *h);
protected:
  int numChars;
  int numBits;
  int mboxX;
  int mboxY;
  int baseHt;

  // per character
  int *value;
  int *height;
  int *width;
  int *setWidth;
  int *topExtent;
  int *leftExtent;

  unsigned char **images;
  GLuint *texNames;

  //float charFreeTypeWidth(char c);
  //float charFreeTypeHeight(char c);
  //float charTop(char c);
  //float charTopExtent(char c);
  //float charLeftExtent(char c);
};

#endif

