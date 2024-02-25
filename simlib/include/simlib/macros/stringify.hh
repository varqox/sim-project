#pragma once

#define STRINGIFY(...) PRIMITIVE_STRINGIFY(__VA_ARGS__)
#define PRIMITIVE_STRINGIFY(...) #__VA_ARGS__
