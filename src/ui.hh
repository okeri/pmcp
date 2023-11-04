#pragma once

#include "Terminal.hh"

class PlayerView;
class Status;
class Lyrics;
class Help;
class Spectralizer;

namespace ui {

void render(Status& status, Terminal::Plane& plane);
void render(PlayerView& view, Terminal::Plane& plane);
void render(Help& help, Terminal::Plane& plane);
void render(Lyrics& lyrics, Terminal::Plane& plane);
void render(Spectralizer& spectres, Terminal::Plane& plane);

}  // namespace ui
