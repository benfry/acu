#include "acu.h"


/* internal state variables, used for acuGet/acuSet */

GLint acuWindowWidth;
GLint acuWindowHeight;
GLfloat acuWindowBgColor[3];
GLboolean acuWindowClear;
GLuint acuDisplayMode = GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH;

/* anytime a string is returned from an acuFunction, use this */
char returnString[256];
char acuResourceLocation[256];

float acuScreenFov;
float acuScreenNear;
float acuScreenFar;

int frameDepth;

GLint acuDebugLevel;
char *acuDebugStr;

int acuScreenGrabQuality;
unsigned char *acuScreenGrabBuffer;
boolean acuScreenGrabFlip;
unsigned char *acuScreenGrabFlipper;
int acuScreenGrabFormat;
int acuScreenGrabX, acuScreenGrabY;
int acuScreenGrabWidth, acuScreenGrabHeight;

/* These are all constants about mazo's view area */
static float mazoDist = 100.0;
static float mazoNearDist = 10.0;
static float mazoFarDist = 1000.0;
static float mazoAspect = 4.0/3.0;
static float mazoEyeX = 320;
static float mazoEyeY = 240;

extern void textInit();

/**
 * Opens a full screen window for drawing
 */
void acuOpen() {
  int argc = 1;
  char *argv = "acu";
  // this strangeness is because of the lameness of glutInit on Mac
  char **vptr = &argv;

//  printf("acuOpen called\n");

  acuWindowWidth = 0;
  acuWindowHeight = 0;

  frameDepth = 0;

  /* set up location of resources dependent on platform */
#if defined(ACU_IRIX)
  strcpy(acuResourceLocation, "/acg/data/");
#elif defined(ACU_WIN32)
  strcpy(acuResourceLocation, "C:/acWindow/");
  //strcpy(acuResourceLocation, "//di/acg/data/");
#elif defined(ACU_MAC)
  strcpy(acuResourceLocation, "/whatever/");
#else
  strcpy(acuResourceLocation, "//um/a/little/help/here/");
#endif

  // InitDisplayMode must appear before CreateWindow
  glutInit(&argc, vptr);
  //glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_ALPHA);
  glutInitDisplayMode(acuDisplayMode); //GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);

  acuDebugLevel = ACU_DEBUG_USEFUL;
  acuDebugStr = (char *) malloc(64);

  acuWindowClear = TRUE;
  acuWindowBgColor[0] = 0;
  acuWindowBgColor[1] = 0;
  acuWindowBgColor[2] = 0;

  acuVideoOpened = FALSE;
  acuVideoMirrorImage = FALSE;
  acuVideoProxyCount = -1;
  acuVideoProxyRepeating = FALSE;
  acuVideoProxyRawWidth = 0;
  acuVideoProxyRawHeight = 0;
  acuVideoFPS = 15;

  acuTesselator = NULL;

  acuTextInited = FALSE;
  acuTextInit();

  acuScreenNear = 1.5;
  acuScreenFar = 20.0;
  acuScreenFov = 60.0;

  acuScreenGrabQuality = 100;
  acuScreenGrabBuffer = NULL;
  acuScreenGrabFlip = TRUE;
  acuScreenGrabFlipper = NULL;
  acuScreenGrabFormat = ACU_FILE_FORMAT_TIFF;
  acuScreenGrabX = 0;
  acuScreenGrabY = 0;
  acuScreenGrabWidth = 0; 
  acuScreenGrabHeight = 0; 

  /* seed random number generator */
#if defined(ACU_IRIX)
  srandom(time(NULL));
#elif defined(ACU_WIN32)
  srand(time(NULL));
#elif defined(ACU_MAC)
  srand(time(NULL));
#endif

}

void acuGlobalGLSettings(void) {
  /* you have to call this for text and stuff to work after
     opening the window */
  glEnable(GL_AUTO_NORMAL);
  glEnable(GL_NORMALIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/**
 * Close the window and exit the application
 */
void acuClose() {
  if (acuVideoOpened) {  /* otherwise win32 video gets upset */
    acuCloseVideo();
  }
  exit(0);
}

/* utility functions used by open/close Frames() */

void frameStepUp();
void frameStepUp() {
  char errString[80];

  /* already in an openFrame */
  if(frameDepth > 10) {
    sprintf(errString, "Warning, frameDepth already at %d\n", frameDepth);
    acuDebug(ACU_DEBUG_PROBLEM, errString);
  } 
  if(frameDepth == 0) {
    glViewport(0, 0, acuWindowWidth, acuWindowHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
  }
  else {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
  }
  ++frameDepth;
}

void frameStepDown();
void frameStepDown() {
  if(--frameDepth == 0) {
    glutSwapBuffers();
  }
  else {
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
  }
}

/**
 * Call this at the beginning of your 
 * application's display function.
 */
void acuOpenFrame() {
  float aspect;
  boolean topLevel;

  /* this handles nested openFrame calls */
  if(frameDepth == 0) topLevel = TRUE;
  else topLevel = FALSE;
  frameStepUp();

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  
  glEnable(GL_DEPTH_TEST);  
  glDepthFunc(GL_LEQUAL);
  glShadeModel(GL_SMOOTH);

  if (topLevel && acuWindowClear) {
    glClearColor(acuWindowBgColor[0], acuWindowBgColor[1], 
      acuWindowBgColor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glDisable(GL_TEXTURE_2D); // no texture default

  /* normally for an openFrame, we would change the projection
   * matrix, but this openFrame is dependent on width, height --
   * so I just do nothing 
   */
  if(topLevel) {
    // sets up standard camera projection;
    glMatrixMode(GL_PROJECTION);
    // done already glLoadIdentity();

    aspect = (float)acuWindowWidth / (float)acuWindowHeight;
    printf("setting aspect to %f\n", aspect);
    /* don't set 'far' too high.. it destroys the resolution of 
       the z-buffer. even 100 might be too high. */
    gluPerspective(acuScreenFov, aspect, acuScreenNear, acuScreenFar);
  
    glMatrixMode(GL_MODELVIEW);
    // done already glLoadIdentity();
  }
}


/**
 * Call this at the end of your display function.
 */
void acuCloseFrame() {
  //glutSwapBuffers();
  frameStepDown();
}


/**
 * Call this at the beginning of a display function.
 */
void acuOpenFrame2D() {
  boolean topLevel;

  /* this handles nested openFrame calls */
  if(frameDepth == 0) topLevel = TRUE;
  else topLevel = FALSE;
  frameStepUp();

  // done already glLoadIdentity();
  // done already glViewport(0, 0, acuWindowWidth, acuWindowHeight);

  /* this should not be based on acuWindowSize, but whatever */
  if(topLevel) {
    glOrtho(0, acuWindowWidth, 0, acuWindowHeight, -1, 1);
  }
  glShadeModel(GL_FLAT);

  if(topLevel && acuWindowClear) {
    glClearColor(acuWindowBgColor[0], acuWindowBgColor[1],
		 acuWindowBgColor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  // done already /* this is needed to clear projection matrix after
  // done already    calling acuOpenFrame() */
  // done already glMatrixMode(GL_PROJECTION);
  // done already glLoadIdentity();
  // done already glMatrixMode(GL_MODELVIEW);
}

/**
 * Call this at the end of a 2D display function.
 */
void acuCloseFrame2D() {
  //glutSwapBuffers();
  frameStepDown();
}


/**
 * Tom's latest frame.
 * This frame is built for viewing the unit sphere centered
 * about the origin. What a nice coordinate system.
 */
void acuOpenGeoFrame() {
  boolean topLevel;
  float aspect;
  GLfloat fov;
  //GLint wSize[2];
  GLfloat sNear, sFar;
  //  GLfloat dist = 1.1547; // 2 / sqr(3) = dist away for 60 degree fov
  GLfloat dist = 2.0;
  fov = 60.0f;

  /* this handles nested openFrame calls */
  if(frameDepth == 0) topLevel = TRUE;
  else topLevel = FALSE;
  frameStepUp();

  sNear = dist - 1.0;
  sFar = 1.0 + dist;
	//sFar = 50.0;
	
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  
  glEnable(GL_DEPTH_TEST);  
  glDepthFunc(GL_LEQUAL);
  glShadeModel(GL_SMOOTH);

  if(topLevel && acuWindowClear) {
    glClearColor(acuWindowBgColor[0], acuWindowBgColor[1],
      acuWindowBgColor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  // done already glViewport(0, 0, wSize[0], wSize[1]);

  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glDisable(GL_TEXTURE_2D); // no texture default

  /* normally for an openFrame, we would change the projection
   * matrix, but this openGeoFrame is dependent on width, height --
   * so I just do nothing 
   */
  if(topLevel) {
    // sets up standard camera projection;
    glMatrixMode(GL_PROJECTION);
    // done already glLoadIdentity();

    // NO aspect = (float)wSize[0] / (float)wSize[1];
    aspect = (float)acuWindowWidth / (float)acuWindowHeight;
    /* don't set 'far' too high.. it destroys the resolution of 
       the z-buffer. even 100 might be too high. */
    gluPerspective(fov, aspect, sNear, sFar);
    gluLookAt(0, 0, dist, 0, 0, 0, 0.0, 1.0, 0.0);
  
    glMatrixMode(GL_MODELVIEW);
    // done already glLoadIdentity();
  }
}

/**
 * Call this at the end of a Geo display function.
 */
void acuCloseGeoFrame() {
  //glutSwapBuffers();
  frameStepDown();
}

/* mazo internal function to set viewing constants 
   depeding on field of view */
void updateMazoViewConstants() {
  float halfFov, theTan;
  
  mazoEyeX = acuWindowWidth / 2.0;
  mazoEyeY = acuWindowHeight / 2.0;
  halfFov = PI * acuScreenFov / 360.0;
  theTan = tan(halfFov);
  mazoDist = mazoEyeY / theTan;
  mazoNearDist = mazoDist / 10.0;
  mazoFarDist = mazoDist * 10.0;
  mazoAspect = (float)acuWindowWidth/acuWindowHeight;
} 

/**
 * Call this before display in a mazo app
 */
void acuOpenMazoFrame() {
  static int lastMazoHeight = 0;
  static int lastMazoWidth = 0;
  boolean topLevel;

  /* this handles nested openFrame calls */
  if(frameDepth == 0) topLevel = TRUE;
  else topLevel = FALSE;
  frameStepUp();

  if(topLevel) {
    if ((lastMazoHeight != acuWindowHeight) ||
        (lastMazoWidth != acuWindowWidth)) {
      lastMazoHeight = acuWindowHeight;
      lastMazoWidth = acuWindowWidth;
      
      updateMazoViewConstants();
    }
  }

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glShadeModel(GL_SMOOTH);

  // done already glViewport(0, 0, acuWindowWidth, acuWindowHeight);

  if (topLevel && acuWindowClear) {
    glClearColor(acuWindowBgColor[0], acuWindowBgColor[1],
                 acuWindowBgColor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glDisable(GL_TEXTURE_2D); // no texture default

  /* normally for an openFrame, we would change the projection
   * matrix, but this openGeoFrame is dependent on width, height --
   * so I just do nothing 
   */
  if(topLevel) {
    // not legal in C! tom must be using a non ansi-compliant compiler
    //GLfloat params[4] = { mazoEyeX, mazoEyeY, mazoDist, 0 };
    GLfloat params[4];
    params[0] = mazoEyeX;
    params[1] = mazoEyeY;
    params[2] = mazoDist;
    params[3] = 0;

    glLightfv(GL_LIGHT0, GL_POSITION, params);

    // sets up standard camera projection;
    glMatrixMode(GL_PROJECTION);
    // done already glLoadIdentity();

    gluPerspective(acuScreenFov, mazoAspect, mazoNearDist, mazoFarDist);
    //printf("(%f, %f, %f, %f)\n", acuScreenFov, mazoAspect, 
    //	   mazoNearDist, mazoFarDist);
    gluLookAt(mazoEyeX, mazoEyeY, mazoDist, 
	      mazoEyeX, mazoEyeY, 0.0, 0.0, 1.0, 0.0);
    //printf("(%f, %f, %f), (%f, %f, %f)\n", mazoEyeX, mazoEyeY, 
    //	   mazoDist, mazoEyeX, mazoEyeY, 0.0);
  
    glMatrixMode(GL_MODELVIEW);
    // done already glLoadIdentity();
  }
 
}

void acuCloseMazoFrame() {
  /*
  if(mazoDoBuffer) {
    glRasterPos2f(0.4, 0.4);
    glDrawPixels(acuWindowWidth, acuWindowHeight, 
	       GL_RGBA, GL_UNSIGNED_BYTE, mazoBuffer);
  }
  glutSwapBuffers();
  */
  frameStepDown();
}

/* REMOVED 11/99

GLubyte *acuGetMazoImage() {
  mazoDoBuffer = 1;
  return mazoBuffer;
}
*/


/* REMOVED 11/99
   This is deprecated as it does not jive with 
   my baby memory management system
void acuSetMazoImage(GLubyte *newBuffer) {
  / * if(mazoBuffer != NULL) delete[] mazoBuffer;* /
  mazoBuffer = newBuffer;
}
*/


void acuScreenGrab(char *filename) {
  unsigned char *temp;
  int j, k;
  FILE *fp; /* for ppm */

  if ((acuScreenGrabWidth == 0) ||
      (acuScreenGrabHeight == 0)) {  // not set yet
    acuScreenGrabWidth = acuWindowWidth;
    acuScreenGrabHeight = acuWindowHeight;
  }

  //printf("sg %d %d %d %d  %d %d\n", 
  //    acuScreenGrabX, acuScreenGrabY,
  //    acuScreenGrabWidth, acuScreenGrabHeight,
  //    acuWindowWidth, acuWindowHeight);

  if (acuScreenGrabFormat == ACU_FILE_FORMAT_SCRSAVE) {
    char command[256];
    sprintf(command, "scrsave %s %d %d %d %d", filename,
	    acuScreenGrabX, acuScreenGrabY,
	    acuScreenGrabWidth, acuScreenGrabHeight);
	    //0, acuWindowWidth, 0, acuWindowHeight);
    printf("%s\n", command);
    system(command);
    return;
  }

  if (acuScreenGrabBuffer == NULL) {
    acuScreenGrabBuffer = (unsigned char *) 
      malloc(acuWindowWidth * acuWindowHeight * 3);
  }

  glReadPixels(acuScreenGrabX, acuScreenGrabY, 
	       acuScreenGrabWidth, acuScreenGrabHeight,
	       //0, 0, acuWindowWidth, acuWindowHeight,
	       GL_RGB, GL_UNSIGNED_BYTE, acuScreenGrabBuffer);

  /* so it's really lazy of me to set up the algorithm this
   * way, but acuScreenGrab is so slow and memory intensive, 
   * anyways, so who cares, there are better things to work on.
   * of course if you're reading this, maybe *you* should fix it?
   */
  if (acuScreenGrabFlip) {
    if (acuScreenGrabFlipper == NULL) {
      acuScreenGrabFlipper = (unsigned char *)
        malloc(acuWindowWidth * acuWindowHeight * 3);
    }
    /*
    for (j = 0; j < acuWindowHeight; j++) {
      k = acuWindowHeight - j - 1;
      memcpy(&acuScreenGrabFlipper[j*acuWindowWidth*3], 
	     &acuScreenGrabBuffer[k*acuWindowWidth*3],
	     acuWindowWidth*3);
    }
    */
    for (j = 0; j < acuScreenGrabHeight; j++) {
      k = acuScreenGrabHeight - j - 1;
      memcpy(&acuScreenGrabFlipper[j*acuScreenGrabWidth*3], 
	     &acuScreenGrabBuffer[k*acuScreenGrabWidth*3],
	     acuScreenGrabWidth*3);
    }
    temp = acuScreenGrabBuffer;
    acuScreenGrabBuffer = acuScreenGrabFlipper;
    acuScreenGrabFlipper = temp;
  }

  switch (acuScreenGrabFormat) {

  case ACU_FILE_FORMAT_JPEG:
#ifdef NO_JPEG
  acuDebug(ACU_DEBUG_PROBLEM, 
           "jpeg not yet supported on this platform.");
#else
    acuWriteJpegFile(filename, acuScreenGrabBuffer, 
		     acuScreenGrabWidth, acuScreenGrabHeight, 
		     acuScreenGrabQuality);
#endif
    break;

  case ACU_FILE_FORMAT_RAW:
    acuWriteRawFile(filename, acuScreenGrabBuffer,
		    acuScreenGrabWidth*acuScreenGrabHeight*3);
    break;

  case ACU_FILE_FORMAT_PPM:
    fp = fopen(filename, "wb");
    fprintf(fp, "%s %d %d %d\n", "P6", 
	    acuScreenGrabWidth, acuScreenGrabHeight, 255);
    fwrite(acuScreenGrabBuffer, 
	   acuScreenGrabWidth*acuScreenGrabHeight*3, 1, fp);
    fclose(fp);
    break;

  case ACU_FILE_FORMAT_TIFF:
    acuWriteTiffFile(filename, acuScreenGrabBuffer, 
		     acuScreenGrabWidth, acuScreenGrabHeight);
    break;
  }
}


/**
 * Debugging/error messages
 * 
 * get/set methods:
 * acuSetIntegerv(ACU_DEBUG_LEVEL, &level);
 * acuGetIntegerv(ACU_DEBUG_LEVEL, &level;)
 *
 * constants for the level:
 * ACU_DEBUG_EMERGENCY, ACU_DEBUG_PROBLEM,
 * ACU_DEBUG_USEFUL, ACU_DEBUG_MUMBLE
 * ACU_DEBUG_CHATTER
 */
void acuDebug(acuEnum severity, char *message) {
  if (severity <= acuDebugLevel) {
    fprintf(stderr, "%s\n", message);
  }
  if (severity == ACU_DEBUG_EMERGENCY) {
    exit(-1);
  }
}

void acuDebugString(acuEnum severity) {
  acuDebug(severity, acuDebugStr);
}


/**
 * Get and set methods for passing and manipulating parameters.
 * If anyone can think of a more vague definition, please
 * supply it here.
 *
 * Note that ACU_WINDOW_WIDTH, ACU_WINDOW_HEIGHT, and ACU_WINDOW_SIZE,
 * are not valid until the window actually exists -- this means that
 * they cannot be called in an init method. That is, they have to be
 * called after the glutDisplayFunc has been called once.
 */
GLint acuGetInteger(acuEnum pname) {
  GLint param;
  acuGetIntegerv(pname, &param);
  return param;
}

void acuGetIntegerv(acuEnum pname, GLint *params) {
  switch (pname) {
  case ACU_DEBUG_LEVEL:
    params[0] = acuDebugLevel;
    break;

  case ACU_WINDOW_WIDTH:
    params[0] = acuWindowWidth;
    break;
  case ACU_WINDOW_HEIGHT:
    params[0] = acuWindowHeight;
    break;
  case ACU_WINDOW_SIZE:
    params[0] = acuWindowWidth;
    params[1] = acuWindowHeight;
    break;

  case ACU_SCREEN_GRAB_QUALITY:
    params[0] = acuScreenGrabQuality;
    break;
  case ACU_SCREEN_GRAB_FORMAT:
    params[0] = acuScreenGrabFormat;
    break;
  case ACU_SCREEN_GRAB_X:
    params[0] = acuScreenGrabX;
    break;
  case ACU_SCREEN_GRAB_Y:
    params[0] = acuScreenGrabY;
    break;
  case ACU_SCREEN_GRAB_WIDTH:
    params[0] = acuScreenGrabWidth;
    break;
  case ACU_SCREEN_GRAB_HEIGHT:
    params[0] = acuScreenGrabHeight;
    break;
  case ACU_SCREEN_GRAB_RECT:
    params[0] = acuScreenGrabX;
    params[1] = acuScreenGrabY;
    params[2] = acuScreenGrabWidth;
    params[3] = acuScreenGrabHeight;
    break;

  case ACU_TIME_STEP:
    params[0] = acuTimeStep;
    break;

  case ACU_VIDEO_WIDTH:
    params[0] = acuVideoWidth;
    break;
  case ACU_VIDEO_HEIGHT:
    params[0] = acuVideoHeight;
    break;
  case ACU_VIDEO_SIZE:
    params[0] = acuVideoWidth;
    params[1] = acuVideoHeight;
    break;
  case ACU_VIDEO_ARRAY_SIZE:
    params[0] = acuVideoArrayWidth;
    params[1] = acuVideoArrayHeight;
    break;
  case ACU_VIDEO_ARRAY_WIDTH:
    params[0] = acuVideoArrayWidth;
    break;
  case ACU_VIDEO_ARRAY_HEIGHT:
    params[0] = acuVideoArrayHeight;
    break;
  case ACU_VIDEO_PROXY_COUNT:
    params[0] = acuVideoProxyCount;
    break;
  case ACU_VIDEO_PROXY_RAW_WIDTH:
    params[0] = acuVideoProxyRawWidth;
    break;
  case ACU_VIDEO_PROXY_RAW_HEIGHT:
    params[0] = acuVideoProxyRawHeight;
    break;
  case ACU_VIDEO_FPS:
    params[0] = acuVideoFPS;
    break;

  default:
    acuDebug(ACU_DEBUG_PROBLEM, 
	     "Bad parameter passed to acuGetIntegerv");
  }
}


void acuSetInteger(acuEnum pname, GLint param) {
  acuSetIntegerv(pname, &param);
}


void acuSetIntegerv(acuEnum pname, GLint *params) {
  switch (pname) {

  case ACU_DEBUG_LEVEL:
    acuDebugLevel = params[0];
    break;

  /* NOTE:
   * ACU_WINDOW_WIDTH and HEIGHT are a way of telling acu
   * what the width and height should be, but ACU_WINDOW_SIZE
   * is a *request* to call glutReshapeWindow to change the
   * window size
   */

  case ACU_WINDOW_WIDTH:
    acuWindowWidth = params[0];
    break;

  case ACU_WINDOW_HEIGHT:
    acuWindowHeight = params[0];
    break;

  case ACU_WINDOW_SIZE:
    acuWindowWidth = params[0];
    acuWindowHeight = params[1];
    glutReshapeWindow(acuWindowWidth, acuWindowHeight);

    /* these sizes will no longer be valid */
    if (acuScreenGrabBuffer) free(acuScreenGrabBuffer);
    if (acuScreenGrabFlipper) free(acuScreenGrabFlipper);
    acuScreenGrabBuffer = NULL;
    acuScreenGrabFlipper = NULL;
    break;

  case ACU_SCREEN_GRAB_QUALITY:
    acuScreenGrabQuality = params[0];
    break;
  case ACU_SCREEN_GRAB_FORMAT:
    acuScreenGrabFormat = params[0];
    break;
  case ACU_SCREEN_GRAB_X:
    acuScreenGrabX = params[0];
    break;
  case ACU_SCREEN_GRAB_Y:
    acuScreenGrabY = params[0];
    break;
  case ACU_SCREEN_GRAB_WIDTH:
    acuScreenGrabWidth = params[0];
    break;
  case ACU_SCREEN_GRAB_HEIGHT:
    acuScreenGrabHeight = params[0];
    break;
  case ACU_SCREEN_GRAB_RECT:
    acuScreenGrabX = params[0];
    acuScreenGrabY = params[1];
    acuScreenGrabWidth = params[2];
    acuScreenGrabHeight = params[3];
    break;

  case ACU_TIME_STEP:
    acuTimeStep = params[0];
    break;

  case ACU_VIDEO_ARRAY_WIDTH:
    acuVideoArrayWidth = params[0];
    break;
  case ACU_VIDEO_ARRAY_HEIGHT:
    acuVideoArrayHeight = params[0];
    break;
  case ACU_VIDEO_ARRAY_SIZE:
    acuVideoArrayWidth = params[0];
    acuVideoArrayHeight = params[1];
    break;
  case ACU_VIDEO_PROXY_COUNT:
    acuVideoProxyCount = params[0];
    break;
  case ACU_VIDEO_PROXY_RAW_WIDTH:
    acuVideoProxyRawWidth = params[0];
    break;
  case ACU_VIDEO_PROXY_RAW_HEIGHT:
    acuVideoProxyRawHeight = params[0];
    break;
  case ACU_VIDEO_FPS:
    acuVideoFPS = params[0];
    break;
  
  default:
    acuDebug(ACU_DEBUG_PROBLEM, 
	     "Bad parameter passed to acuSetIntegerv");
  }
}


GLfloat acuGetFloat(acuEnum pname) {
  GLfloat param;
  acuGetFloatv(pname, &param);
  return param;
}


void acuGetFloatv(acuEnum pname, GLfloat *params) {
  switch (pname) {

  case ACU_WINDOW_BG_COLOR:
    params[0] = acuWindowBgColor[0];
    params[1] = acuWindowBgColor[1];
    params[2] = acuWindowBgColor[2];
    break;

  // this only works for mazo right now, should be fixed - tom 6/17/99
  case ACU_SCREEN_EYE:
    params[0] = mazoEyeX;
    params[1] = mazoEyeY;
    params[2] = mazoDist;
    break;

  case ACU_SCREEN_FOV:
    params[0] = acuScreenFov;
    break;

  case ACU_SCREEN_NEAR:
    params[0] = acuScreenNear;
    break;

  case ACU_SCREEN_FAR:
    params[0] = acuScreenFar;
    break;

  default:
    acuDebug(ACU_DEBUG_PROBLEM, 
	     "Bad parameter passed to acuGetFloatv");
  }
}


void acuSetFloat(acuEnum pname, GLfloat param) {
  acuSetFloatv(pname, &param);
}

void acuSetFloatv(acuEnum pname, GLfloat *params) {
  switch (pname) {

  case ACU_WINDOW_BG_COLOR:
    acuWindowBgColor[0] = params[0];
    acuWindowBgColor[1] = params[1];
    acuWindowBgColor[2] = params[2];
    break;

  case ACU_SCREEN_FOV:
    acuScreenFov = params[0];
    updateMazoViewConstants();
    break;

  case ACU_SCREEN_NEAR:
    acuScreenNear = params[0];
    updateMazoViewConstants();
    break;

  case ACU_SCREEN_FAR:
    acuScreenFar = params[0];
    updateMazoViewConstants();
    break;

  default:
    acuDebug(ACU_DEBUG_PROBLEM, 
	     "Bad parameter passed to acuSetFloatv");
  }
}


GLboolean acuGetBoolean(acuEnum pname) {
  GLboolean param;
  acuGetBooleanv(pname, &param);
  return param;
}

void acuGetBooleanv(acuEnum pname, GLboolean *params) {
  switch (pname) {

  case ACU_VIDEO_MIRROR_IMAGE:
    params[0] = acuVideoMirrorImage;
    break;

  case ACU_VIDEO_PROXY_REPEATING:
    params[0] = acuVideoProxyRepeating;
    break;

  case ACU_SCREEN_GRAB_FLIP:
    params[0] = acuScreenGrabFlip;
    break;

  case ACU_WINDOW_CLEAR:
    params[0] = acuWindowClear;
    break;

  case ACU_FONT_NORMALIZE:
    params[0] = acuFontNormalize;
    break;

  default:
    acuDebug(ACU_DEBUG_PROBLEM, 
	     "Bad parameter passed to acuGetBooleanv");
  }
}


void acuSetBoolean(acuEnum pname, GLboolean param) {
  acuSetBooleanv(pname, &param);
}

void acuSetBooleanv(acuEnum pname, GLboolean *params) {
  switch (pname) {

  case ACU_VIDEO_MIRROR_IMAGE:
    acuVideoMirrorImage = params[0];
    break;

  case ACU_WINDOW_CLEAR:
    acuWindowClear = params[0];
    break;

  case ACU_SCREEN_GRAB_FLIP:
    acuScreenGrabFlip = params[0];
    break;

  case ACU_FONT_NORMALIZE:
    acuFontNormalize = params[0];
    break;

  default:
    acuDebug(ACU_DEBUG_PROBLEM, 
	     "Bad parameter passed to acuSetBooleanv");
  }
}

char *acuGetString(acuEnum pname) {
  switch(pname) {
    
  case ACU_RESOURCE_LOCATION:
    strcpy(returnString, acuResourceLocation);
    return returnString;

  default:
    acuDebug(ACU_DEBUG_PROBLEM, "Bad parameter passed to acuGetString");
    strcpy(returnString, "");
    return returnString;
  }
}

void acuSetString(acuEnum pname, char *param) {
  
  switch (pname) {
    
  case ACU_RESOURCE_LOCATION:
    strcpy(acuResourceLocation, param);
    break;
  
  default:
    acuDebug(ACU_DEBUG_PROBLEM, "Bad parameter passed to acuSetString");
  
  }
}

/* 
 * acuResourceFile should switch the pathname separator and use
 * the current ACU_RESOURCE_LOCATION. But we use fopen, which seems
 * to always want forward slashes. Maybe that will change on Macs.
 * Anyway, for now we have this simple implementation.
 */
char *acuResourceFile(char *location) {
  strcpy(returnString, acuResourceLocation);
  strcat(returnString, location);
  return returnString;
}
