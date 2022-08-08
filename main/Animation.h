#pragma once

#include <memory>
#include <inttypes.h>

#include <Image.h>

void animationSetup();
void animationStart(std::unique_ptr<Image>&& image);

