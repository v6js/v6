#pragma once

#ifndef JNIEXPORT
#define JNIEXPORT __declspec(dllexport)
#endif
#define JNIIMPORT __declspec(dllimport)

typedef int jint;
typedef long long jlong;
typedef signed char jbyte;
