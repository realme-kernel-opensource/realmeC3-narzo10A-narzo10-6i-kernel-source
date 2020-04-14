/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _CAMERA_TYPEDEFS_H
#define _CAMERA_TYPEDEFS_H


/* ------------------------*/
/* Basic Type Definitions */
/* -----------------------*/

typedef long LONG;
typedef unsigned char UBYTE;
typedef short SHORT;

#define kal_int8 signed char
#define kal_int16 signed short
#define kal_int32 signed int
typedef long long kal_int64;
#define kal_uint8 unsigned char
#define kal_uint16 unsigned short
#define kal_uint32 unsigned int
typedef unsigned long long kal_uint64;
typedef char kal_char;

typedef unsigned int *UINT32P;
typedef volatile unsigned short *UINT16P;
typedef volatile unsigned char *UINT8P;
typedef unsigned char *U8P;


typedef unsigned char U8;
typedef signed char S8;
typedef unsigned short U16;
typedef signed short S16;
typedef unsigned int U32;
typedef signed int S32;
typedef unsigned long long U64;
typedef signed long long S64;

#define UINT8 unsigned char
#define UINT16 unsigned short
#define UINT32 unsigned int
typedef unsigned short USHORT;
#define INT8 signed char
typedef signed short INT16;
#define INT32 signed int
typedef unsigned int DWORD;
typedef void VOID;
#define BYTE unsigned char
typedef float FLOAT;

typedef char *LPCSTR;
typedef short *LPWSTR;


/* -----------*/
/* Constants */
/* ----------*/
#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
#define TRUE  (1)
#endif

#ifndef NULL
#define NULL  (0)
#endif

/* enum boolean {false, true}; */
enum { RX, TX, NONE };

#ifndef BOOL
#define BOOL unsigned char
#endif
#define kal_bool bool
#define	KAL_FALSE 0
#define KAL_TRUE 1


#endif				/* _CAMERA_TYPEDEFS_H */
