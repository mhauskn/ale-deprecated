#ifndef DISPLAY_SCREEN_H
#define DISPLAY_SCREEN_H

#include <stdio.h>
#include <stdlib.h>
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include "SDL/SDL_rotozoom.h"
#include "SDL/SDL_gfxPrimitives.h"
#include "Constants.h"
#include "export_screen.h"
#include "MediaSrc.hxx"

class SDLEventHandler {
public:
    // Returns true if it handles this SDL Event. False if not and the event
    // will be passed to other handlers.
    virtual bool handleSDLEvent(const SDL_Event& event) = 0;

    // Print the usage information about this handler
    virtual void usage() = 0;
};

class DisplayScreen : public SDLEventHandler {
public:
    DisplayScreen(ExportScreen* export_screen);
    virtual ~DisplayScreen();

    // Displays the current frame buffer directly from the mediasource
    void display_screen(const MediaSource& mediaSrc);

    // Displays a screen_matrix. Typically used by classes that draw custom
    // things to the screen.
    void display_screen(const IntMatrix& screen_matrix, int image_widht, int image_height);

    // Draws a png image to the screen from a file
    void display_png(const string& filename);

    // Registers a handler for keyboard and mouse events
    void registerEventHandler(SDLEventHandler* handler);

    // Implements pause functionality
    bool handleSDLEvent(const SDL_Event& event);

    void usage();

    // Dimensions of the SDL window
    int screen_height, screen_width;

protected:
    // Checks for SDL events such as keypresses.
    // TODO: Run in a different thread?
    void poll();

    // Maintains the paused/unpaused state of the game
    bool paused;
  
    SDL_Surface *screen, *image;
    ExportScreen* export_screen;

    // Handlers for SDL Events
    std::vector<SDLEventHandler*> handlers;
};

#endif
