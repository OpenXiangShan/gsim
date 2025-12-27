/*
 * gsimFst.h - single-header libfst (writer/reader) bundle.
 * Sourced from zyj-gsim/include/libfst with minor tweaks:
 *  - inlined fastlz/lz4 and fstapi sources
 *  - defaults to HAVE_LIBPTHREAD + FST_WRITER_PARALLEL enabled
 *  - lz4 visibility forced to static for header-only use
 * System deps: zlib (libz) and pthreads, plus standard C runtime.
 */
#ifndef GSIM_FST_H
#define GSIM_FST_H

#ifndef HAVE_LIBPTHREAD
#define HAVE_LIBPTHREAD 1
#endif
#ifndef FST_WRITER_PARALLEL
#define FST_WRITER_PARALLEL 1
#endif
/* force lz4 public symbols to have internal linkage in header-only mode */
#ifndef LZ4LIB_VISIBILITY
#define LZ4LIB_VISIBILITY static
#endif
/* expose static-linking-only APIs expected by lz4.c */
#ifndef LZ4_STATIC_LINKING_ONLY
#define LZ4_STATIC_LINKING_ONLY 1
#endif

/* ==== fstapi.h ==== */
/*
 * Copyright (c) 2009-2018 Tony Bybell.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef FST_API_H
#define FST_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h>
#include <inttypes.h>
#if defined(_MSC_VER)
/* inlined fst_win_unistd.h */
#include <stdlib.h>
#ifdef _WIN64
#include <io.h>
#else
#include <sys/io.h>
#endif
#include <process.h>
#define ftruncate _chsize_s
#define unlink _unlink
#define fileno _fileno
#define lseek _lseeki64
#ifdef _WIN64
#define ssize_t __int64
#define SSIZE_MAX 9223372036854775807i64
#else
#define ssize_t long
#define SSIZE_MAX 2147483647L
#endif
#include <stdint.h>
#else
#include <unistd.h>
#endif
#include <time.h>

#define FST_RDLOAD "FSTLOAD | "

typedef uint32_t fstHandle;
typedef uint32_t fstEnumHandle;

enum fstWriterPackType {
    FST_WR_PT_ZLIB             = 0,
    FST_WR_PT_FASTLZ           = 1,
    FST_WR_PT_LZ4              = 2
};

enum fstFileType {
    FST_FT_MIN                 = 0,

    FST_FT_VERILOG             = 0,
    FST_FT_VHDL                = 1,
    FST_FT_VERILOG_VHDL        = 2,

    FST_FT_MAX                 = 2
};

enum fstBlockType {
    FST_BL_HDR                 = 0,
    FST_BL_VCDATA              = 1,
    FST_BL_BLACKOUT            = 2,
    FST_BL_GEOM                = 3,
    FST_BL_HIER                = 4,
    FST_BL_VCDATA_DYN_ALIAS    = 5,
    FST_BL_HIER_LZ4            = 6,
    FST_BL_HIER_LZ4DUO         = 7,
    FST_BL_VCDATA_DYN_ALIAS2   = 8,

    FST_BL_ZWRAPPER            = 254,   /* indicates that whole trace is gz wrapped */
    FST_BL_SKIP                = 255    /* used while block is being written */
};

enum fstScopeType {
    FST_ST_MIN                 = 0,

    FST_ST_VCD_MODULE          = 0,
    FST_ST_VCD_TASK            = 1,
    FST_ST_VCD_FUNCTION        = 2,
    FST_ST_VCD_BEGIN           = 3,
    FST_ST_VCD_FORK            = 4,
    FST_ST_VCD_GENERATE        = 5,
    FST_ST_VCD_STRUCT          = 6,
    FST_ST_VCD_UNION           = 7,
    FST_ST_VCD_CLASS           = 8,
    FST_ST_VCD_INTERFACE       = 9,
    FST_ST_VCD_PACKAGE         = 10,
    FST_ST_VCD_PROGRAM         = 11,

    FST_ST_VHDL_ARCHITECTURE   = 12,
    FST_ST_VHDL_PROCEDURE      = 13,
    FST_ST_VHDL_FUNCTION       = 14,
    FST_ST_VHDL_RECORD         = 15,
    FST_ST_VHDL_PROCESS        = 16,
    FST_ST_VHDL_BLOCK          = 17,
    FST_ST_VHDL_FOR_GENERATE   = 18,
    FST_ST_VHDL_IF_GENERATE    = 19,
    FST_ST_VHDL_GENERATE       = 20,
    FST_ST_VHDL_PACKAGE        = 21,

    FST_ST_MAX                 = 21,

    FST_ST_GEN_ATTRBEGIN       = 252,
    FST_ST_GEN_ATTREND         = 253,

    FST_ST_VCD_SCOPE           = 254,
    FST_ST_VCD_UPSCOPE         = 255
};

enum fstVarType {
    FST_VT_MIN                 = 0,     /* start of vartypes */

    FST_VT_VCD_EVENT           = 0,
    FST_VT_VCD_INTEGER         = 1,
    FST_VT_VCD_PARAMETER       = 2,
    FST_VT_VCD_REAL            = 3,
    FST_VT_VCD_REAL_PARAMETER  = 4,
    FST_VT_VCD_REG             = 5,
    FST_VT_VCD_SUPPLY0         = 6,
    FST_VT_VCD_SUPPLY1         = 7,
    FST_VT_VCD_TIME            = 8,
    FST_VT_VCD_TRI             = 9,
    FST_VT_VCD_TRIAND          = 10,
    FST_VT_VCD_TRIOR           = 11,
    FST_VT_VCD_TRIREG          = 12,
    FST_VT_VCD_TRI0            = 13,
    FST_VT_VCD_TRI1            = 14,
    FST_VT_VCD_WAND            = 15,
    FST_VT_VCD_WIRE            = 16,
    FST_VT_VCD_WOR             = 17,
    FST_VT_VCD_PORT            = 18,
    FST_VT_VCD_SPARRAY         = 19,    /* used to define the rownum (index) port for a sparse array */
    FST_VT_VCD_REALTIME        = 20,

    FST_VT_GEN_STRING          = 21,    /* generic string type   (max len is defined dynamically via fstWriterEmitVariableLengthValueChange) */

    FST_VT_SV_BIT              = 22,
    FST_VT_SV_LOGIC            = 23,
    FST_VT_SV_INT              = 24,    /* declare as size = 32 */
    FST_VT_SV_SHORTINT         = 25,    /* declare as size = 16 */
    FST_VT_SV_LONGINT          = 26,    /* declare as size = 64 */
    FST_VT_SV_BYTE             = 27,    /* declare as size = 8  */
    FST_VT_SV_ENUM             = 28,    /* declare as appropriate type range */
    FST_VT_SV_SHORTREAL        = 29,    /* declare and emit same as FST_VT_VCD_REAL (needs to be emitted as double, not a float) */

    FST_VT_MAX                 = 29     /* end of vartypes */
};

enum fstVarDir {
    FST_VD_MIN         = 0,

    FST_VD_IMPLICIT    = 0,
    FST_VD_INPUT       = 1,
    FST_VD_OUTPUT      = 2,
    FST_VD_INOUT       = 3,
    FST_VD_BUFFER      = 4,
    FST_VD_LINKAGE     = 5,

    FST_VD_MAX         = 5
};

enum fstHierType {
    FST_HT_MIN         = 0,

    FST_HT_SCOPE       = 0,
    FST_HT_UPSCOPE     = 1,
    FST_HT_VAR         = 2,
    FST_HT_ATTRBEGIN   = 3,
    FST_HT_ATTREND     = 4,

    /* FST_HT_TREEBEGIN and FST_HT_TREEEND are not yet used by FST but are currently used when fstHier bridges other formats */
    FST_HT_TREEBEGIN   = 5,
    FST_HT_TREEEND     = 6,

    FST_HT_MAX         = 6
};

enum fstAttrType {
    FST_AT_MIN         = 0,

    FST_AT_MISC        = 0,     /* self-contained: does not need matching FST_HT_ATTREND */
    FST_AT_ARRAY       = 1,
    FST_AT_ENUM        = 2,
    FST_AT_PACK        = 3,

    FST_AT_MAX         = 3
};

enum fstMiscType {
    FST_MT_MIN         = 0,

    FST_MT_COMMENT     = 0,     /* use fstWriterSetComment() to emit */
    FST_MT_ENVVAR      = 1,     /* use fstWriterSetEnvVar() to emit */
    FST_MT_SUPVAR      = 2,     /* use fstWriterCreateVar2() to emit */
    FST_MT_PATHNAME    = 3,     /* reserved for fstWriterSetSourceStem() string -> number management */
    FST_MT_SOURCESTEM  = 4,     /* use fstWriterSetSourceStem() to emit */
    FST_MT_SOURCEISTEM = 5,     /* use fstWriterSetSourceInstantiationStem() to emit */
    FST_MT_VALUELIST   = 6,	/* use fstWriterSetValueList() to emit, followed by fstWriterCreateVar*() */
    FST_MT_ENUMTABLE   = 7,	/* use fstWriterCreateEnumTable() and fstWriterEmitEnumTableRef() to emit */
    FST_MT_UNKNOWN     = 8,

    FST_MT_MAX         = 8
};

enum fstArrayType {
    FST_AR_MIN         = 0,

    FST_AR_NONE        = 0,
    FST_AR_UNPACKED    = 1,
    FST_AR_PACKED      = 2,
    FST_AR_SPARSE      = 3,

    FST_AR_MAX         = 3
};

enum fstEnumValueType {
    FST_EV_SV_INTEGER           = 0,
    FST_EV_SV_BIT               = 1,
    FST_EV_SV_LOGIC             = 2,
    FST_EV_SV_INT               = 3,
    FST_EV_SV_SHORTINT          = 4,
    FST_EV_SV_LONGINT           = 5,
    FST_EV_SV_BYTE              = 6,
    FST_EV_SV_UNSIGNED_INTEGER  = 7,
    FST_EV_SV_UNSIGNED_BIT      = 8,
    FST_EV_SV_UNSIGNED_LOGIC    = 9,
    FST_EV_SV_UNSIGNED_INT      = 10,
    FST_EV_SV_UNSIGNED_SHORTINT = 11,
    FST_EV_SV_UNSIGNED_LONGINT  = 12,
    FST_EV_SV_UNSIGNED_BYTE     = 13,

    FST_EV_REG			= 14,
    FST_EV_TIME			= 15,

    FST_EV_MAX                  = 15
};

enum fstPackType {
    FST_PT_NONE          = 0,
    FST_PT_UNPACKED      = 1,
    FST_PT_PACKED        = 2,
    FST_PT_TAGGED_PACKED = 3,

    FST_PT_MAX           = 3
};

enum fstSupplementalVarType {
    FST_SVT_MIN                    = 0,

    FST_SVT_NONE                   = 0,

    FST_SVT_VHDL_SIGNAL            = 1,
    FST_SVT_VHDL_VARIABLE          = 2,
    FST_SVT_VHDL_CONSTANT          = 3,
    FST_SVT_VHDL_FILE              = 4,
    FST_SVT_VHDL_MEMORY            = 5,

    FST_SVT_MAX                    = 5
};

enum fstSupplementalDataType {
    FST_SDT_MIN                    = 0,

    FST_SDT_NONE                   = 0,

    FST_SDT_VHDL_BOOLEAN           = 1,
    FST_SDT_VHDL_BIT               = 2,
    FST_SDT_VHDL_BIT_VECTOR        = 3,
    FST_SDT_VHDL_STD_ULOGIC        = 4,
    FST_SDT_VHDL_STD_ULOGIC_VECTOR = 5,
    FST_SDT_VHDL_STD_LOGIC         = 6,
    FST_SDT_VHDL_STD_LOGIC_VECTOR  = 7,
    FST_SDT_VHDL_UNSIGNED          = 8,
    FST_SDT_VHDL_SIGNED            = 9,
    FST_SDT_VHDL_INTEGER           = 10,
    FST_SDT_VHDL_REAL              = 11,
    FST_SDT_VHDL_NATURAL           = 12,
    FST_SDT_VHDL_POSITIVE          = 13,
    FST_SDT_VHDL_TIME              = 14,
    FST_SDT_VHDL_CHARACTER         = 15,
    FST_SDT_VHDL_STRING            = 16,

    FST_SDT_MAX                    = 16,

    FST_SDT_SVT_SHIFT_COUNT        = 10, /* FST_SVT_* is ORed in by fstWriterCreateVar2() to the left after shifting FST_SDT_SVT_SHIFT_COUNT */
    FST_SDT_ABS_MAX                = ((1<<(FST_SDT_SVT_SHIFT_COUNT))-1)
};


struct fstHier
{
unsigned char htyp;

union {
        /* if htyp == FST_HT_SCOPE */
        struct fstHierScope {
                unsigned char typ; /* FST_ST_MIN ... FST_ST_MAX */
                const char *name;
                const char *component;
                uint32_t name_length;           /* strlen(u.scope.name) */
                uint32_t component_length;      /* strlen(u.scope.component) */
                } scope;

        /* if htyp == FST_HT_VAR */
        struct fstHierVar {
                unsigned char typ; /* FST_VT_MIN ... FST_VT_MAX */
                unsigned char direction; /* FST_VD_MIN ... FST_VD_MAX */
                unsigned char svt_workspace; /* zeroed out by FST reader, for client code use */
                unsigned char sdt_workspace; /* zeroed out by FST reader, for client code use */
                unsigned int  sxt_workspace; /* zeroed out by FST reader, for client code use */
                const char *name;
                uint32_t length;
                fstHandle handle;
                uint32_t name_length; /* strlen(u.var.name) */
                unsigned is_alias : 1;
                } var;

        /* if htyp == FST_HT_ATTRBEGIN */
        struct fstHierAttr {
                unsigned char typ; /* FST_AT_MIN ... FST_AT_MAX */
                unsigned char subtype; /* from fstMiscType, fstArrayType, fstEnumValueType, fstPackType */
                const char *name;
                uint64_t arg; /* number of array elements, struct members, or some other payload (possibly ignored) */
                uint64_t arg_from_name; /* for when name is overloaded as a variable-length integer (FST_AT_MISC + FST_MT_SOURCESTEM) */
                uint32_t name_length; /* strlen(u.attr.name) */
                } attr;
        } u;
};


struct fstETab
{
char *name;
uint32_t elem_count;
char **literal_arr;
char **val_arr;
};


/*
 * writer functions
 */
void            fstWriterClose(void *ctx);
void *          fstWriterCreate(const char *nam, int use_compressed_hier);
fstEnumHandle   fstWriterCreateEnumTable(void *ctx, const char *name, uint32_t elem_count, unsigned int min_valbits, const char **literal_arr, const char **val_arr);
                /* used for Verilog/SV */
fstHandle       fstWriterCreateVar(void *ctx, enum fstVarType vt, enum fstVarDir vd,
                        uint32_t len, const char *nam, fstHandle aliasHandle);
                /* future expansion for VHDL and other languages.  The variable type, data type, etc map onto
                   the current Verilog/SV one.  The "type" string is optional for a more verbose or custom description */
fstHandle       fstWriterCreateVar2(void *ctx, enum fstVarType vt, enum fstVarDir vd,
                        uint32_t len, const char *nam, fstHandle aliasHandle,
                        const char *type, enum fstSupplementalVarType svt, enum fstSupplementalDataType sdt);
void            fstWriterEmitDumpActive(void *ctx, int enable);
void 		fstWriterEmitEnumTableRef(void *ctx, fstEnumHandle handle);
void            fstWriterEmitValueChange(void *ctx, fstHandle handle, const void *val);
void            fstWriterEmitValueChange32(void *ctx, fstHandle handle,
                        uint32_t bits, uint32_t val);
void            fstWriterEmitValueChange64(void *ctx, fstHandle handle,
                        uint32_t bits, uint64_t val);
void            fstWriterEmitValueChangeVec32(void *ctx, fstHandle handle,
                        uint32_t bits, const uint32_t *val);
void            fstWriterEmitValueChangeVec64(void *ctx, fstHandle handle,
                        uint32_t bits, const uint64_t *val);
void            fstWriterEmitVariableLengthValueChange(void *ctx, fstHandle handle, const void *val, uint32_t len);
void            fstWriterEmitTimeChange(void *ctx, uint64_t tim);
void            fstWriterFlushContext(void *ctx);
int             fstWriterGetDumpSizeLimitReached(void *ctx);
int             fstWriterGetFseekFailed(void *ctx);
void            fstWriterSetAttrBegin(void *ctx, enum fstAttrType attrtype, int subtype,
                        const char *attrname, uint64_t arg);
void            fstWriterSetAttrEnd(void *ctx);
void            fstWriterSetComment(void *ctx, const char *comm);
void            fstWriterSetDate(void *ctx, const char *dat);
void            fstWriterSetDumpSizeLimit(void *ctx, uint64_t numbytes);
void            fstWriterSetEnvVar(void *ctx, const char *envvar);
void            fstWriterSetFileType(void *ctx, enum fstFileType filetype);
void            fstWriterSetPackType(void *ctx, enum fstWriterPackType typ);
void            fstWriterSetParallelMode(void *ctx, int enable);
void            fstWriterSetRepackOnClose(void *ctx, int enable);       /* type = 0 (none), 1 (libz) */
void            fstWriterSetScope(void *ctx, enum fstScopeType scopetype,
                        const char *scopename, const char *scopecomp);
void            fstWriterSetSourceInstantiationStem(void *ctx, const char *path, unsigned int line, unsigned int use_realpath);
void            fstWriterSetSourceStem(void *ctx, const char *path, unsigned int line, unsigned int use_realpath);
void            fstWriterSetTimescale(void *ctx, int ts);
void            fstWriterSetTimescaleFromString(void *ctx, const char *s);
void            fstWriterSetTimezero(void *ctx, int64_t tim);
void            fstWriterSetUpscope(void *ctx);
void		fstWriterSetValueList(void *ctx, const char *vl);
void            fstWriterSetVersion(void *ctx, const char *vers);


/*
 * reader functions
 */
void            fstReaderClose(void *ctx);
void            fstReaderClrFacProcessMask(void *ctx, fstHandle facidx);
void            fstReaderClrFacProcessMaskAll(void *ctx);
uint64_t        fstReaderGetAliasCount(void *ctx);
const char *    fstReaderGetCurrentFlatScope(void *ctx);
void *          fstReaderGetCurrentScopeUserInfo(void *ctx);
int             fstReaderGetCurrentScopeLen(void *ctx);
const char *    fstReaderGetDateString(void *ctx);
int             fstReaderGetDoubleEndianMatchState(void *ctx);
uint64_t        fstReaderGetDumpActivityChangeTime(void *ctx, uint32_t idx);
unsigned char   fstReaderGetDumpActivityChangeValue(void *ctx, uint32_t idx);
uint64_t        fstReaderGetEndTime(void *ctx);
int             fstReaderGetFacProcessMask(void *ctx, fstHandle facidx);
int             fstReaderGetFileType(void *ctx);
int             fstReaderGetFseekFailed(void *ctx);
fstHandle       fstReaderGetMaxHandle(void *ctx);
uint64_t        fstReaderGetMemoryUsedByWriter(void *ctx);
uint32_t        fstReaderGetNumberDumpActivityChanges(void *ctx);
uint64_t        fstReaderGetScopeCount(void *ctx);
uint64_t        fstReaderGetStartTime(void *ctx);
signed char     fstReaderGetTimescale(void *ctx);
int64_t         fstReaderGetTimezero(void *ctx);
uint64_t        fstReaderGetValueChangeSectionCount(void *ctx);
char *          fstReaderGetValueFromHandleAtTime(void *ctx, uint64_t tim, fstHandle facidx, char *buf);
uint64_t        fstReaderGetVarCount(void *ctx);
const char *    fstReaderGetVersionString(void *ctx);
struct fstHier *fstReaderIterateHier(void *ctx);
int             fstReaderIterateHierRewind(void *ctx);
int             fstReaderIterBlocks(void *ctx,
                        void (*value_change_callback)(void *user_callback_data_pointer, uint64_t time, fstHandle facidx, const unsigned char *value),
                        void *user_callback_data_pointer, FILE *vcdhandle);
int             fstReaderIterBlocks2(void *ctx,
                        void (*value_change_callback)(void *user_callback_data_pointer, uint64_t time, fstHandle facidx, const unsigned char *value),
                        void (*value_change_callback_varlen)(void *user_callback_data_pointer, uint64_t time, fstHandle facidx, const unsigned char *value, uint32_t len),
                        void *user_callback_data_pointer, FILE *vcdhandle);
void            fstReaderIterBlocksSetNativeDoublesOnCallback(void *ctx, int enable);
void *          fstReaderOpen(const char *nam);
void *          fstReaderOpenForUtilitiesOnly(void);
const char *    fstReaderPopScope(void *ctx);
int             fstReaderProcessHier(void *ctx, FILE *vcdhandle);
const char *    fstReaderPushScope(void *ctx, const char *nam, void *user_info);
void            fstReaderResetScope(void *ctx);
void            fstReaderSetFacProcessMask(void *ctx, fstHandle facidx);
void            fstReaderSetFacProcessMaskAll(void *ctx);
void            fstReaderSetLimitTimeRange(void *ctx, uint64_t start_time, uint64_t end_time);
void            fstReaderSetUnlimitedTimeRange(void *ctx);
void            fstReaderSetVcdExtensions(void *ctx, int enable);


/*
 * utility functions
 */
int             fstUtilityBinToEscConvertedLen(const unsigned char *s, int len); /* used for mallocs for fstUtilityBinToEsc() */
int             fstUtilityBinToEsc(unsigned char *d, const unsigned char *s, int len);
int             fstUtilityEscToBin(unsigned char *d, unsigned char *s, int len);
struct fstETab *fstUtilityExtractEnumTableFromString(const char *s);
void 		fstUtilityFreeEnumTable(struct fstETab *etab); /* must use to free fstETab properly */


#ifdef __cplusplus
}
#endif

#endif

#ifdef GSIM_FST_IMPL

/* ==== fastlz.h ==== */
/*
  FastLZ - lightning-fast lossless compression library

  Copyright (C) 2007 Ariya Hidayat (ariya@kde.org)
  Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
  Copyright (C) 2005 Ariya Hidayat (ariya@kde.org)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  SPDX-License-Identifier: MIT
*/

#ifndef FASTLZ_H
#define FASTLZ_H

#include <inttypes.h>

#define flzuint8 uint8_t
#define flzuint16 uint16_t
#define flzuint32 uint32_t


#define FASTLZ_VERSION 0x000100

#define FASTLZ_VERSION_MAJOR     0
#define FASTLZ_VERSION_MINOR     0
#define FASTLZ_VERSION_REVISION  0

#define FASTLZ_VERSION_STRING "0.1.0"

#if defined (__cplusplus)
extern "C" {
#endif

/**
  Compress a block of data in the input buffer and returns the size of
  compressed block. The size of input buffer is specified by length. The
  minimum input buffer size is 16.

  The output buffer must be at least 5% larger than the input buffer
  and can not be smaller than 66 bytes.

  If the input is not compressible, the return value might be larger than
  length (input buffer size).

  The input buffer and the output buffer can not overlap.
*/

int fastlz_compress(const void* input, int length, void* output);

/**
  Decompress a block of compressed data and returns the size of the
  decompressed block. If error occurs, e.g. the compressed data is
  corrupted or the output buffer is not large enough, then 0 (zero)
  will be returned instead.

  The input buffer and the output buffer can not overlap.

  Decompression is memory safe and guaranteed not to write the output buffer
  more than what is specified in maxout.
 */

int fastlz_decompress(const void* input, int length, void* output, int maxout);

/**
  Compress a block of data in the input buffer and returns the size of
  compressed block. The size of input buffer is specified by length. The
  minimum input buffer size is 16.

  The output buffer must be at least 5% larger than the input buffer
  and can not be smaller than 66 bytes.

  If the input is not compressible, the return value might be larger than
  length (input buffer size).

  The input buffer and the output buffer can not overlap.

  Compression level can be specified in parameter level. At the moment,
  only level 1 and level 2 are supported.
  Level 1 is the fastest compression and generally useful for short data.
  Level 2 is slightly slower but it gives better compression ratio.

  Note that the compressed data, regardless of the level, can always be
  decompressed using the function fastlz_decompress above.
*/

int fastlz_compress_level(int level, const void* input, int length, void* output);

#if defined (__cplusplus)
}
#endif

#endif /* FASTLZ_H */

/* ==== lz4.h ==== */
/*
 *  LZ4 - Fast LZ compression algorithm
 *  Header File
 *  Copyright (C) 2011-2023, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/lz4/lz4
*/
#if defined (__cplusplus)
extern "C" {
#endif

#ifndef LZ4_H_2983827168210
#define LZ4_H_2983827168210

/* --- Dependency --- */
#include <stddef.h>   /* size_t */


/**
  Introduction

  LZ4 is lossless compression algorithm, providing compression speed >500 MB/s per core,
  scalable with multi-cores CPU. It features an extremely fast decoder, with speed in
  multiple GB/s per core, typically reaching RAM speed limits on multi-core systems.

  The LZ4 compression library provides in-memory compression and decompression functions.
  It gives full buffer control to user.
  Compression can be done in:
    - a single step (described as Simple Functions)
    - a single step, reusing a context (described in Advanced Functions)
    - unbounded multiple steps (described as Streaming compression)

  lz4.h generates and decodes LZ4-compressed blocks (doc/lz4_Block_format.md).
  Decompressing such a compressed block requires additional metadata.
  Exact metadata depends on exact decompression function.
  For the typical case of LZ4_decompress_safe(),
  metadata includes block's compressed size, and maximum bound of decompressed size.
  Each application is free to encode and pass such metadata in whichever way it wants.

  lz4.h only handle blocks, it can not generate Frames.

  Blocks are different from Frames (doc/lz4_Frame_format.md).
  Frames bundle both blocks and metadata in a specified manner.
  Embedding metadata is required for compressed data to be self-contained and portable.
  Frame format is delivered through a companion API, declared in lz4frame.h.
  The `lz4` CLI can only manage frames.
*/

/*^***************************************************************
*  Export parameters
*****************************************************************/
/*
*  LZ4_DLL_EXPORT :
*  Enable exporting of functions when building a Windows DLL
*  LZ4LIB_VISIBILITY :
*  Control library symbols visibility.
*/
#ifndef LZ4LIB_VISIBILITY
#  if defined(__GNUC__) && (__GNUC__ >= 4)
#    define LZ4LIB_VISIBILITY __attribute__ ((visibility ("default")))
#  else
#    define LZ4LIB_VISIBILITY
#  endif
#endif
#if defined(LZ4_DLL_EXPORT) && (LZ4_DLL_EXPORT==1)
#  define LZ4LIB_API __declspec(dllexport) LZ4LIB_VISIBILITY
#elif defined(LZ4_DLL_IMPORT) && (LZ4_DLL_IMPORT==1)
#  define LZ4LIB_API __declspec(dllimport) LZ4LIB_VISIBILITY /* It isn't required but allows to generate better code, saving a function pointer load from the IAT and an indirect jump.*/
#else
#  define LZ4LIB_API LZ4LIB_VISIBILITY
#endif

/*! LZ4_FREESTANDING :
 *  When this macro is set to 1, it enables "freestanding mode" that is
 *  suitable for typical freestanding environment which doesn't support
 *  standard C library.
 *
 *  - LZ4_FREESTANDING is a compile-time switch.
 *  - It requires the following macros to be defined:
 *    LZ4_memcpy, LZ4_memmove, LZ4_memset.
 *  - It only enables LZ4/HC functions which don't use heap.
 *    All LZ4F_* functions are not supported.
 *  - See tests/freestanding.c to check its basic setup.
 */
#if defined(LZ4_FREESTANDING) && (LZ4_FREESTANDING == 1)
#  define LZ4_HEAPMODE 0
#  define LZ4HC_HEAPMODE 0
#  define LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION 1
#  if !defined(LZ4_memcpy)
#    error "LZ4_FREESTANDING requires macro 'LZ4_memcpy'."
#  endif
#  if !defined(LZ4_memset)
#    error "LZ4_FREESTANDING requires macro 'LZ4_memset'."
#  endif
#  if !defined(LZ4_memmove)
#    error "LZ4_FREESTANDING requires macro 'LZ4_memmove'."
#  endif
#elif ! defined(LZ4_FREESTANDING)
#  define LZ4_FREESTANDING 0
#endif


/*------   Version   ------*/
#define LZ4_VERSION_MAJOR    1    /* for breaking interface changes  */
#define LZ4_VERSION_MINOR    9    /* for new (non-breaking) interface capabilities */
#define LZ4_VERSION_RELEASE  5    /* for tweaks, bug-fixes, or development */

#define LZ4_VERSION_NUMBER (LZ4_VERSION_MAJOR *100*100 + LZ4_VERSION_MINOR *100 + LZ4_VERSION_RELEASE)

#define LZ4_LIB_VERSION LZ4_VERSION_MAJOR.LZ4_VERSION_MINOR.LZ4_VERSION_RELEASE
#define LZ4_QUOTE(str) #str
#define LZ4_EXPAND_AND_QUOTE(str) LZ4_QUOTE(str)
#define LZ4_VERSION_STRING LZ4_EXPAND_AND_QUOTE(LZ4_LIB_VERSION)  /* requires v1.7.3+ */

LZ4LIB_API int LZ4_versionNumber (void);  /**< library version number; useful to check dll version; requires v1.3.0+ */
LZ4LIB_API const char* LZ4_versionString (void);   /**< library version string; useful to check dll version; requires v1.7.5+ */


/*-************************************
*  Tuning memory usage
**************************************/
/*!
 * LZ4_MEMORY_USAGE :
 * Memory usage formula : N->2^N Bytes (examples : 10 -> 1KB; 12 -> 4KB ; 16 -> 64KB; 20 -> 1MB)
 * Increasing memory usage improves compression ratio, generally at the cost of speed.
 * Reduced memory usage may improve speed at the cost of ratio, thanks to better cache locality.
 * Default value is 14, for 16KB, which nicely fits into most L1 caches.
 */
#ifndef LZ4_MEMORY_USAGE
# define LZ4_MEMORY_USAGE LZ4_MEMORY_USAGE_DEFAULT
#endif

#define LZ4_MEMORY_USAGE_MIN 10
#define LZ4_MEMORY_USAGE_DEFAULT 14
#define LZ4_MEMORY_USAGE_MAX 20

#if (LZ4_MEMORY_USAGE < LZ4_MEMORY_USAGE_MIN)
#  error "LZ4_MEMORY_USAGE is too small !"
#endif

#if (LZ4_MEMORY_USAGE > LZ4_MEMORY_USAGE_MAX)
#  error "LZ4_MEMORY_USAGE is too large !"
#endif

/*-************************************
*  Simple Functions
**************************************/
/*! LZ4_compress_default() :
 *  Compresses 'srcSize' bytes from buffer 'src'
 *  into already allocated 'dst' buffer of size 'dstCapacity'.
 *  Compression is guaranteed to succeed if 'dstCapacity' >= LZ4_compressBound(srcSize).
 *  It also runs faster, so it's a recommended setting.
 *  If the function cannot compress 'src' into a more limited 'dst' budget,
 *  compression stops *immediately*, and the function result is zero.
 *  In which case, 'dst' content is undefined (invalid).
 *      srcSize : max supported value is LZ4_MAX_INPUT_SIZE.
 *      dstCapacity : size of buffer 'dst' (which must be already allocated)
 *     @return  : the number of bytes written into buffer 'dst' (necessarily <= dstCapacity)
 *                or 0 if compression fails
 * Note : This function is protected against buffer overflow scenarios (never writes outside 'dst' buffer, nor read outside 'source' buffer).
 */
LZ4LIB_API int LZ4_compress_default(const char* src, char* dst, int srcSize, int dstCapacity);

/*! LZ4_decompress_safe() :
 * @compressedSize : is the exact complete size of the compressed block.
 * @dstCapacity : is the size of destination buffer (which must be already allocated),
 *                presumed an upper bound of decompressed size.
 * @return : the number of bytes decompressed into destination buffer (necessarily <= dstCapacity)
 *           If destination buffer is not large enough, decoding will stop and output an error code (negative value).
 *           If the source stream is detected malformed, the function will stop decoding and return a negative result.
 * Note 1 : This function is protected against malicious data packets :
 *          it will never writes outside 'dst' buffer, nor read outside 'source' buffer,
 *          even if the compressed block is maliciously modified to order the decoder to do these actions.
 *          In such case, the decoder stops immediately, and considers the compressed block malformed.
 * Note 2 : compressedSize and dstCapacity must be provided to the function, the compressed block does not contain them.
 *          The implementation is free to send / store / derive this information in whichever way is most beneficial.
 *          If there is a need for a different format which bundles together both compressed data and its metadata, consider looking at lz4frame.h instead.
 */
LZ4LIB_API int LZ4_decompress_safe (const char* src, char* dst, int compressedSize, int dstCapacity);


/*-************************************
*  Advanced Functions
**************************************/
#define LZ4_MAX_INPUT_SIZE        0x7E000000   /* 2 113 929 216 bytes */
#define LZ4_COMPRESSBOUND(isize)  ((unsigned)(isize) > (unsigned)LZ4_MAX_INPUT_SIZE ? 0 : (isize) + ((isize)/255) + 16)

/*! LZ4_compressBound() :
    Provides the maximum size that LZ4 compression may output in a "worst case" scenario (input data not compressible)
    This function is primarily useful for memory allocation purposes (destination buffer size).
    Macro LZ4_COMPRESSBOUND() is also provided for compilation-time evaluation (stack memory allocation for example).
    Note that LZ4_compress_default() compresses faster when dstCapacity is >= LZ4_compressBound(srcSize)
        inputSize  : max supported value is LZ4_MAX_INPUT_SIZE
        return : maximum output size in a "worst case" scenario
              or 0, if input size is incorrect (too large or negative)
*/
LZ4LIB_API int LZ4_compressBound(int inputSize);

/*! LZ4_compress_fast() :
    Same as LZ4_compress_default(), but allows selection of "acceleration" factor.
    The larger the acceleration value, the faster the algorithm, but also the lesser the compression.
    It's a trade-off. It can be fine tuned, with each successive value providing roughly +~3% to speed.
    An acceleration value of "1" is the same as regular LZ4_compress_default()
    Values <= 0 will be replaced by LZ4_ACCELERATION_DEFAULT (currently == 1, see lz4.c).
    Values > LZ4_ACCELERATION_MAX will be replaced by LZ4_ACCELERATION_MAX (currently == 65537, see lz4.c).
*/
LZ4LIB_API int LZ4_compress_fast (const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);


/*! LZ4_compress_fast_extState() :
 *  Same as LZ4_compress_fast(), using an externally allocated memory space for its state.
 *  Use LZ4_sizeofState() to know how much memory must be allocated,
 *  and allocate it on 8-bytes boundaries (using `malloc()` typically).
 *  Then, provide this buffer as `void* state` to compression function.
 */
LZ4LIB_API int LZ4_sizeofState(void);
LZ4LIB_API int LZ4_compress_fast_extState (void* state, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);

/*! LZ4_compress_destSize() :
 *  Reverse the logic : compresses as much data as possible from 'src' buffer
 *  into already allocated buffer 'dst', of size >= 'dstCapacity'.
 *  This function either compresses the entire 'src' content into 'dst' if it's large enough,
 *  or fill 'dst' buffer completely with as much data as possible from 'src'.
 *  note: acceleration parameter is fixed to "default".
 *
 * *srcSizePtr : in+out parameter. Initially contains size of input.
 *               Will be modified to indicate how many bytes where read from 'src' to fill 'dst'.
 *               New value is necessarily <= input value.
 * @return : Nb bytes written into 'dst' (necessarily <= dstCapacity)
 *           or 0 if compression fails.
 *
 * Note : from v1.8.2 to v1.9.1, this function had a bug (fixed in v1.9.2+):
 *        the produced compressed content could, in specific circumstances,
 *        require to be decompressed into a destination buffer larger
 *        by at least 1 byte than the content to decompress.
 *        If an application uses `LZ4_compress_destSize()`,
 *        it's highly recommended to update liblz4 to v1.9.2 or better.
 *        If this can't be done or ensured,
 *        the receiving decompression function should provide
 *        a dstCapacity which is > decompressedSize, by at least 1 byte.
 *        See https://github.com/lz4/lz4/issues/859 for details
 */
LZ4LIB_API int LZ4_compress_destSize(const char* src, char* dst, int* srcSizePtr, int targetDstSize);

/*! LZ4_decompress_safe_partial() :
 *  Decompress an LZ4 compressed block, of size 'srcSize' at position 'src',
 *  into destination buffer 'dst' of size 'dstCapacity'.
 *  Up to 'targetOutputSize' bytes will be decoded.
 *  The function stops decoding on reaching this objective.
 *  This can be useful to boost performance
 *  whenever only the beginning of a block is required.
 *
 * @return : the number of bytes decoded in `dst` (necessarily <= targetOutputSize)
 *           If source stream is detected malformed, function returns a negative result.
 *
 *  Note 1 : @return can be < targetOutputSize, if compressed block contains less data.
 *
 *  Note 2 : targetOutputSize must be <= dstCapacity
 *
 *  Note 3 : this function effectively stops decoding on reaching targetOutputSize,
 *           so dstCapacity is kind of redundant.
 *           This is because in older versions of this function,
 *           decoding operation would still write complete sequences.
 *           Therefore, there was no guarantee that it would stop writing at exactly targetOutputSize,
 *           it could write more bytes, though only up to dstCapacity.
 *           Some "margin" used to be required for this operation to work properly.
 *           Thankfully, this is no longer necessary.
 *           The function nonetheless keeps the same signature, in an effort to preserve API compatibility.
 *
 *  Note 4 : If srcSize is the exact size of the block,
 *           then targetOutputSize can be any value,
 *           including larger than the block's decompressed size.
 *           The function will, at most, generate block's decompressed size.
 *
 *  Note 5 : If srcSize is _larger_ than block's compressed size,
 *           then targetOutputSize **MUST** be <= block's decompressed size.
 *           Otherwise, *silent corruption will occur*.
 */
LZ4LIB_API int LZ4_decompress_safe_partial (const char* src, char* dst, int srcSize, int targetOutputSize, int dstCapacity);


/*-*********************************************
*  Streaming Compression Functions
***********************************************/
typedef union LZ4_stream_u LZ4_stream_t;  /* incomplete type (defined later) */

/*!
 Note about RC_INVOKED

 - RC_INVOKED is predefined symbol of rc.exe (the resource compiler which is part of MSVC/Visual Studio).
   https://docs.microsoft.com/en-us/windows/win32/menurc/predefined-macros

 - Since rc.exe is a legacy compiler, it truncates long symbol (> 30 chars)
   and reports warning "RC4011: identifier truncated".

 - To eliminate the warning, we surround long preprocessor symbol with
   "#if !defined(RC_INVOKED) ... #endif" block that means
   "skip this block when rc.exe is trying to read it".
*/
#if !defined(RC_INVOKED) /* https://docs.microsoft.com/en-us/windows/win32/menurc/predefined-macros */
#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
LZ4LIB_API LZ4_stream_t* LZ4_createStream(void);
LZ4LIB_API int           LZ4_freeStream (LZ4_stream_t* streamPtr);
#endif /* !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION) */
#endif

/*! LZ4_resetStream_fast() : v1.9.0+
 *  Use this to prepare an LZ4_stream_t for a new chain of dependent blocks
 *  (e.g., LZ4_compress_fast_continue()).
 *
 *  An LZ4_stream_t must be initialized once before usage.
 *  This is automatically done when created by LZ4_createStream().
 *  However, should the LZ4_stream_t be simply declared on stack (for example),
 *  it's necessary to initialize it first, using LZ4_initStream().
 *
 *  After init, start any new stream with LZ4_resetStream_fast().
 *  A same LZ4_stream_t can be re-used multiple times consecutively
 *  and compress multiple streams,
 *  provided that it starts each new stream with LZ4_resetStream_fast().
 *
 *  LZ4_resetStream_fast() is much faster than LZ4_initStream(),
 *  but is not compatible with memory regions containing garbage data.
 *
 *  Note: it's only useful to call LZ4_resetStream_fast()
 *        in the context of streaming compression.
 *        The *extState* functions perform their own resets.
 *        Invoking LZ4_resetStream_fast() before is redundant, and even counterproductive.
 */
LZ4LIB_API void LZ4_resetStream_fast (LZ4_stream_t* streamPtr);

/*! LZ4_loadDict() :
 *  Use this function to reference a static dictionary into LZ4_stream_t.
 *  The dictionary must remain available during compression.
 *  LZ4_loadDict() triggers a reset, so any previous data will be forgotten.
 *  The same dictionary will have to be loaded on decompression side for successful decoding.
 *  Dictionary are useful for better compression of small data (KB range).
 *  While LZ4 itself accepts any input as dictionary, dictionary efficiency is also a topic.
 *  When in doubt, employ the Zstandard's Dictionary Builder.
 *  Loading a size of 0 is allowed, and is the same as reset.
 * @return : loaded dictionary size, in bytes (note: only the last 64 KB are loaded)
 */
LZ4LIB_API int LZ4_loadDict (LZ4_stream_t* streamPtr, const char* dictionary, int dictSize);

/*! LZ4_compress_fast_continue() :
 *  Compress 'src' content using data from previously compressed blocks, for better compression ratio.
 * 'dst' buffer must be already allocated.
 *  If dstCapacity >= LZ4_compressBound(srcSize), compression is guaranteed to succeed, and runs faster.
 *
 * @return : size of compressed block
 *           or 0 if there is an error (typically, cannot fit into 'dst').
 *
 *  Note 1 : Each invocation to LZ4_compress_fast_continue() generates a new block.
 *           Each block has precise boundaries.
 *           Each block must be decompressed separately, calling LZ4_decompress_*() with relevant metadata.
 *           It's not possible to append blocks together and expect a single invocation of LZ4_decompress_*() to decompress them together.
 *
 *  Note 2 : The previous 64KB of source data is __assumed__ to remain present, unmodified, at same address in memory !
 *
 *  Note 3 : When input is structured as a double-buffer, each buffer can have any size, including < 64 KB.
 *           Make sure that buffers are separated, by at least one byte.
 *           This construction ensures that each block only depends on previous block.
 *
 *  Note 4 : If input buffer is a ring-buffer, it can have any size, including < 64 KB.
 *
 *  Note 5 : After an error, the stream status is undefined (invalid), it can only be reset or freed.
 */
LZ4LIB_API int LZ4_compress_fast_continue (LZ4_stream_t* streamPtr, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);

/*! LZ4_saveDict() :
 *  If last 64KB data cannot be guaranteed to remain available at its current memory location,
 *  save it into a safer place (char* safeBuffer).
 *  This is schematically equivalent to a memcpy() followed by LZ4_loadDict(),
 *  but is much faster, because LZ4_saveDict() doesn't need to rebuild tables.
 * @return : saved dictionary size in bytes (necessarily <= maxDictSize), or 0 if error.
 */
LZ4LIB_API int LZ4_saveDict (LZ4_stream_t* streamPtr, char* safeBuffer, int maxDictSize);


/*-**********************************************
*  Streaming Decompression Functions
*  Bufferless synchronous API
************************************************/
typedef union LZ4_streamDecode_u LZ4_streamDecode_t;   /* tracking context */

/*! LZ4_createStreamDecode() and LZ4_freeStreamDecode() :
 *  creation / destruction of streaming decompression tracking context.
 *  A tracking context can be re-used multiple times.
 */
#if !defined(RC_INVOKED) /* https://docs.microsoft.com/en-us/windows/win32/menurc/predefined-macros */
#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
LZ4LIB_API LZ4_streamDecode_t* LZ4_createStreamDecode(void);
LZ4LIB_API int                 LZ4_freeStreamDecode (LZ4_streamDecode_t* LZ4_stream);
#endif /* !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION) */
#endif

/*! LZ4_setStreamDecode() :
 *  An LZ4_streamDecode_t context can be allocated once and re-used multiple times.
 *  Use this function to start decompression of a new stream of blocks.
 *  A dictionary can optionally be set. Use NULL or size 0 for a reset order.
 *  Dictionary is presumed stable : it must remain accessible and unmodified during next decompression.
 * @return : 1 if OK, 0 if error
 */
LZ4LIB_API int LZ4_setStreamDecode (LZ4_streamDecode_t* LZ4_streamDecode, const char* dictionary, int dictSize);

/*! LZ4_decoderRingBufferSize() : v1.8.2+
 *  Note : in a ring buffer scenario (optional),
 *  blocks are presumed decompressed next to each other
 *  up to the moment there is not enough remaining space for next block (remainingSize < maxBlockSize),
 *  at which stage it resumes from beginning of ring buffer.
 *  When setting such a ring buffer for streaming decompression,
 *  provides the minimum size of this ring buffer
 *  to be compatible with any source respecting maxBlockSize condition.
 * @return : minimum ring buffer size,
 *           or 0 if there is an error (invalid maxBlockSize).
 */
LZ4LIB_API int LZ4_decoderRingBufferSize(int maxBlockSize);
#define LZ4_DECODER_RING_BUFFER_SIZE(maxBlockSize) (65536 + 14 + (maxBlockSize))  /* for static allocation; maxBlockSize presumed valid */

/*! LZ4_decompress_safe_continue() :
 *  This decoding function allows decompression of consecutive blocks in "streaming" mode.
 *  The difference with the usual independent blocks is that
 *  new blocks are allowed to find references into former blocks.
 *  A block is an unsplittable entity, and must be presented entirely to the decompression function.
 *  LZ4_decompress_safe_continue() only accepts one block at a time.
 *  It's modeled after `LZ4_decompress_safe()` and behaves similarly.
 *
 * @LZ4_streamDecode : decompression state, tracking the position in memory of past data
 * @compressedSize : exact complete size of one compressed block.
 * @dstCapacity : size of destination buffer (which must be already allocated),
 *                must be an upper bound of decompressed size.
 * @return : number of bytes decompressed into destination buffer (necessarily <= dstCapacity)
 *           If destination buffer is not large enough, decoding will stop and output an error code (negative value).
 *           If the source stream is detected malformed, the function will stop decoding and return a negative result.
 *
 *  The last 64KB of previously decoded data *must* remain available and unmodified
 *  at the memory position where they were previously decoded.
 *  If less than 64KB of data has been decoded, all the data must be present.
 *
 *  Special : if decompression side sets a ring buffer, it must respect one of the following conditions :
 *  - Decompression buffer size is _at least_ LZ4_decoderRingBufferSize(maxBlockSize).
 *    maxBlockSize is the maximum size of any single block. It can have any value > 16 bytes.
 *    In which case, encoding and decoding buffers do not need to be synchronized.
 *    Actually, data can be produced by any source compliant with LZ4 format specification, and respecting maxBlockSize.
 *  - Synchronized mode :
 *    Decompression buffer size is _exactly_ the same as compression buffer size,
 *    and follows exactly same update rule (block boundaries at same positions),
 *    and decoding function is provided with exact decompressed size of each block (exception for last block of the stream),
 *    _then_ decoding & encoding ring buffer can have any size, including small ones ( < 64 KB).
 *  - Decompression buffer is larger than encoding buffer, by a minimum of maxBlockSize more bytes.
 *    In which case, encoding and decoding buffers do not need to be synchronized,
 *    and encoding ring buffer can have any size, including small ones ( < 64 KB).
 *
 *  Whenever these conditions are not possible,
 *  save the last 64KB of decoded data into a safe buffer where it can't be modified during decompression,
 *  then indicate where this data is saved using LZ4_setStreamDecode(), before decompressing next block.
*/
LZ4LIB_API int
LZ4_decompress_safe_continue (LZ4_streamDecode_t* LZ4_streamDecode,
                        const char* src, char* dst,
                        int srcSize, int dstCapacity);


/*! LZ4_decompress_safe_usingDict() :
 *  Works the same as
 *  a combination of LZ4_setStreamDecode() followed by LZ4_decompress_safe_continue()
 *  However, it's stateless: it doesn't need any LZ4_streamDecode_t state.
 *  Dictionary is presumed stable : it must remain accessible and unmodified during decompression.
 *  Performance tip : Decompression speed can be substantially increased
 *                    when dst == dictStart + dictSize.
 */
LZ4LIB_API int
LZ4_decompress_safe_usingDict(const char* src, char* dst,
                              int srcSize, int dstCapacity,
                              const char* dictStart, int dictSize);

/*! LZ4_decompress_safe_partial_usingDict() :
 *  Behaves the same as LZ4_decompress_safe_partial()
 *  with the added ability to specify a memory segment for past data.
 *  Performance tip : Decompression speed can be substantially increased
 *                    when dst == dictStart + dictSize.
 */
LZ4LIB_API int
LZ4_decompress_safe_partial_usingDict(const char* src, char* dst,
                                      int compressedSize,
                                      int targetOutputSize, int maxOutputSize,
                                      const char* dictStart, int dictSize);

#endif /* LZ4_H_2983827168210 */


/*^*************************************
 * !!!!!!   STATIC LINKING ONLY   !!!!!!
 ***************************************/

/*-****************************************************************************
 * Experimental section
 *
 * Symbols declared in this section must be considered unstable. Their
 * signatures or semantics may change, or they may be removed altogether in the
 * future. They are therefore only safe to depend on when the caller is
 * statically linked against the library.
 *
 * To protect against unsafe usage, not only are the declarations guarded,
 * the definitions are hidden by default
 * when building LZ4 as a shared/dynamic library.
 *
 * In order to access these declarations,
 * define LZ4_STATIC_LINKING_ONLY in your application
 * before including LZ4's headers.
 *
 * In order to make their implementations accessible dynamically, you must
 * define LZ4_PUBLISH_STATIC_FUNCTIONS when building the LZ4 library.
 ******************************************************************************/

#ifdef LZ4_STATIC_LINKING_ONLY

#ifndef LZ4_STATIC_3504398509
#define LZ4_STATIC_3504398509

#ifdef LZ4_PUBLISH_STATIC_FUNCTIONS
# define LZ4LIB_STATIC_API LZ4LIB_API
#else
# define LZ4LIB_STATIC_API
#endif


/*! LZ4_compress_fast_extState_fastReset() :
 *  A variant of LZ4_compress_fast_extState().
 *
 *  Using this variant avoids an expensive initialization step.
 *  It is only safe to call if the state buffer is known to be correctly initialized already
 *  (see above comment on LZ4_resetStream_fast() for a definition of "correctly initialized").
 *  From a high level, the difference is that
 *  this function initializes the provided state with a call to something like LZ4_resetStream_fast()
 *  while LZ4_compress_fast_extState() starts with a call to LZ4_resetStream().
 */
LZ4LIB_STATIC_API int LZ4_compress_fast_extState_fastReset (void* state, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);

/*! LZ4_compress_destSize_extState() :
 *  Same as LZ4_compress_destSize(), but using an externally allocated state.
 *  Also: exposes @acceleration
 */
int LZ4_compress_destSize_extState(void* state, const char* src, char* dst, int* srcSizePtr, int targetDstSize, int acceleration);

/*! LZ4_attach_dictionary() :
 *  This is an experimental API that allows
 *  efficient use of a static dictionary many times.
 *
 *  Rather than re-loading the dictionary buffer into a working context before
 *  each compression, or copying a pre-loaded dictionary's LZ4_stream_t into a
 *  working LZ4_stream_t, this function introduces a no-copy setup mechanism,
 *  in which the working stream references the dictionary stream in-place.
 *
 *  Several assumptions are made about the state of the dictionary stream.
 *  Currently, only streams which have been prepared by LZ4_loadDict() should
 *  be expected to work.
 *
 *  Alternatively, the provided dictionaryStream may be NULL,
 *  in which case any existing dictionary stream is unset.
 *
 *  If a dictionary is provided, it replaces any pre-existing stream history.
 *  The dictionary contents are the only history that can be referenced and
 *  logically immediately precede the data compressed in the first subsequent
 *  compression call.
 *
 *  The dictionary will only remain attached to the working stream through the
 *  first compression call, at the end of which it is cleared. The dictionary
 *  stream (and source buffer) must remain in-place / accessible / unchanged
 *  through the completion of the first compression call on the stream.
 */
LZ4LIB_STATIC_API void
LZ4_attach_dictionary(LZ4_stream_t* workingStream,
                const LZ4_stream_t* dictionaryStream);


/*! In-place compression and decompression
 *
 * It's possible to have input and output sharing the same buffer,
 * for highly constrained memory environments.
 * In both cases, it requires input to lay at the end of the buffer,
 * and decompression to start at beginning of the buffer.
 * Buffer size must feature some margin, hence be larger than final size.
 *
 * |<------------------------buffer--------------------------------->|
 *                             |<-----------compressed data--------->|
 * |<-----------decompressed size------------------>|
 *                                                  |<----margin---->|
 *
 * This technique is more useful for decompression,
 * since decompressed size is typically larger,
 * and margin is short.
 *
 * In-place decompression will work inside any buffer
 * which size is >= LZ4_DECOMPRESS_INPLACE_BUFFER_SIZE(decompressedSize).
 * This presumes that decompressedSize > compressedSize.
 * Otherwise, it means compression actually expanded data,
 * and it would be more efficient to store such data with a flag indicating it's not compressed.
 * This can happen when data is not compressible (already compressed, or encrypted).
 *
 * For in-place compression, margin is larger, as it must be able to cope with both
 * history preservation, requiring input data to remain unmodified up to LZ4_DISTANCE_MAX,
 * and data expansion, which can happen when input is not compressible.
 * As a consequence, buffer size requirements are much higher,
 * and memory savings offered by in-place compression are more limited.
 *
 * There are ways to limit this cost for compression :
 * - Reduce history size, by modifying LZ4_DISTANCE_MAX.
 *   Note that it is a compile-time constant, so all compressions will apply this limit.
 *   Lower values will reduce compression ratio, except when input_size < LZ4_DISTANCE_MAX,
 *   so it's a reasonable trick when inputs are known to be small.
 * - Require the compressor to deliver a "maximum compressed size".
 *   This is the `dstCapacity` parameter in `LZ4_compress*()`.
 *   When this size is < LZ4_COMPRESSBOUND(inputSize), then compression can fail,
 *   in which case, the return code will be 0 (zero).
 *   The caller must be ready for these cases to happen,
 *   and typically design a backup scheme to send data uncompressed.
 * The combination of both techniques can significantly reduce
 * the amount of margin required for in-place compression.
 *
 * In-place compression can work in any buffer
 * which size is >= (maxCompressedSize)
 * with maxCompressedSize == LZ4_COMPRESSBOUND(srcSize) for guaranteed compression success.
 * LZ4_COMPRESS_INPLACE_BUFFER_SIZE() depends on both maxCompressedSize and LZ4_DISTANCE_MAX,
 * so it's possible to reduce memory requirements by playing with them.
 */

#define LZ4_DECOMPRESS_INPLACE_MARGIN(compressedSize)          (((compressedSize) >> 8) + 32)
#define LZ4_DECOMPRESS_INPLACE_BUFFER_SIZE(decompressedSize)   ((decompressedSize) + LZ4_DECOMPRESS_INPLACE_MARGIN(decompressedSize))  /**< note: presumes that compressedSize < decompressedSize. note2: margin is overestimated a bit, since it could use compressedSize instead */

#ifndef LZ4_DISTANCE_MAX   /* history window size; can be user-defined at compile time */
#  define LZ4_DISTANCE_MAX 65535   /* set to maximum value by default */
#endif

#define LZ4_COMPRESS_INPLACE_MARGIN                           (LZ4_DISTANCE_MAX + 32)   /* LZ4_DISTANCE_MAX can be safely replaced by srcSize when it's smaller */
#define LZ4_COMPRESS_INPLACE_BUFFER_SIZE(maxCompressedSize)   ((maxCompressedSize) + LZ4_COMPRESS_INPLACE_MARGIN)  /**< maxCompressedSize is generally LZ4_COMPRESSBOUND(inputSize), but can be set to any lower value, with the risk that compression can fail (return code 0(zero)) */

#endif   /* LZ4_STATIC_3504398509 */
#endif   /* LZ4_STATIC_LINKING_ONLY */



#ifndef LZ4_H_98237428734687
#define LZ4_H_98237428734687

/*-************************************************************
 *  Private Definitions
 **************************************************************
 * Do not use these definitions directly.
 * They are only exposed to allow static allocation of `LZ4_stream_t` and `LZ4_streamDecode_t`.
 * Accessing members will expose user code to API and/or ABI break in future versions of the library.
 **************************************************************/
#define LZ4_HASHLOG   (LZ4_MEMORY_USAGE-2)
#define LZ4_HASHTABLESIZE (1 << LZ4_MEMORY_USAGE)
#define LZ4_HASH_SIZE_U32 (1 << LZ4_HASHLOG)       /* required as macro for static allocation */

#if defined(__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
# include <stdint.h>
  typedef  int8_t  LZ4_i8;
  typedef uint8_t  LZ4_byte;
  typedef uint16_t LZ4_u16;
  typedef uint32_t LZ4_u32;
#else
  typedef   signed char  LZ4_i8;
  typedef unsigned char  LZ4_byte;
  typedef unsigned short LZ4_u16;
  typedef unsigned int   LZ4_u32;
#endif

/*! LZ4_stream_t :
 *  Never ever use below internal definitions directly !
 *  These definitions are not API/ABI safe, and may change in future versions.
 *  If you need static allocation, declare or allocate an LZ4_stream_t object.
**/

typedef struct LZ4_stream_t_internal LZ4_stream_t_internal;
struct LZ4_stream_t_internal {
    LZ4_u32 hashTable[LZ4_HASH_SIZE_U32];
    const LZ4_byte* dictionary;
    const LZ4_stream_t_internal* dictCtx;
    LZ4_u32 currentOffset;
    LZ4_u32 tableType;
    LZ4_u32 dictSize;
    /* Implicit padding to ensure structure is aligned */
};

#define LZ4_STREAM_MINSIZE  ((1UL << (LZ4_MEMORY_USAGE)) + 32)  /* static size, for inter-version compatibility */
union LZ4_stream_u {
    char minStateSize[LZ4_STREAM_MINSIZE];
    LZ4_stream_t_internal internal_donotuse;
}; /* previously typedef'd to LZ4_stream_t */


/*! LZ4_initStream() : v1.9.0+
 *  An LZ4_stream_t structure must be initialized at least once.
 *  This is automatically done when invoking LZ4_createStream(),
 *  but it's not when the structure is simply declared on stack (for example).
 *
 *  Use LZ4_initStream() to properly initialize a newly declared LZ4_stream_t.
 *  It can also initialize any arbitrary buffer of sufficient size,
 *  and will @return a pointer of proper type upon initialization.
 *
 *  Note : initialization fails if size and alignment conditions are not respected.
 *         In which case, the function will @return NULL.
 *  Note2: An LZ4_stream_t structure guarantees correct alignment and size.
 *  Note3: Before v1.9.0, use LZ4_resetStream() instead
**/
LZ4LIB_API LZ4_stream_t* LZ4_initStream (void* stateBuffer, size_t size);


/*! LZ4_streamDecode_t :
 *  Never ever use below internal definitions directly !
 *  These definitions are not API/ABI safe, and may change in future versions.
 *  If you need static allocation, declare or allocate an LZ4_streamDecode_t object.
**/
typedef struct {
    const LZ4_byte* externalDict;
    const LZ4_byte* prefixEnd;
    size_t extDictSize;
    size_t prefixSize;
} LZ4_streamDecode_t_internal;

#define LZ4_STREAMDECODE_MINSIZE 32
union LZ4_streamDecode_u {
    char minStateSize[LZ4_STREAMDECODE_MINSIZE];
    LZ4_streamDecode_t_internal internal_donotuse;
} ;   /* previously typedef'd to LZ4_streamDecode_t */



/*-************************************
*  Obsolete Functions
**************************************/

/*! Deprecation warnings
 *
 *  Deprecated functions make the compiler generate a warning when invoked.
 *  This is meant to invite users to update their source code.
 *  Should deprecation warnings be a problem, it is generally possible to disable them,
 *  typically with -Wno-deprecated-declarations for gcc
 *  or _CRT_SECURE_NO_WARNINGS in Visual.
 *
 *  Another method is to define LZ4_DISABLE_DEPRECATE_WARNINGS
 *  before including the header file.
 */
#ifdef LZ4_DISABLE_DEPRECATE_WARNINGS
#  define LZ4_DEPRECATED(message)   /* disable deprecation warnings */
#else
#  if defined (__cplusplus) && (__cplusplus >= 201402) /* C++14 or greater */
#    define LZ4_DEPRECATED(message) [[deprecated(message)]]
#  elif defined(_MSC_VER)
#    define LZ4_DEPRECATED(message) __declspec(deprecated(message))
#  elif defined(__clang__) || (defined(__GNUC__) && (__GNUC__ * 10 + __GNUC_MINOR__ >= 45))
#    define LZ4_DEPRECATED(message) __attribute__((deprecated(message)))
#  elif defined(__GNUC__) && (__GNUC__ * 10 + __GNUC_MINOR__ >= 31)
#    define LZ4_DEPRECATED(message) __attribute__((deprecated))
#  else
#    pragma message("WARNING: LZ4_DEPRECATED needs custom implementation for this compiler")
#    define LZ4_DEPRECATED(message)   /* disabled */
#  endif
#endif /* LZ4_DISABLE_DEPRECATE_WARNINGS */

/*! Obsolete compression functions (since v1.7.3) */
LZ4_DEPRECATED("use LZ4_compress_default() instead")       LZ4LIB_API int LZ4_compress               (const char* src, char* dest, int srcSize);
LZ4_DEPRECATED("use LZ4_compress_default() instead")       LZ4LIB_API int LZ4_compress_limitedOutput (const char* src, char* dest, int srcSize, int maxOutputSize);
LZ4_DEPRECATED("use LZ4_compress_fast_extState() instead") LZ4LIB_API int LZ4_compress_withState               (void* state, const char* source, char* dest, int inputSize);
LZ4_DEPRECATED("use LZ4_compress_fast_extState() instead") LZ4LIB_API int LZ4_compress_limitedOutput_withState (void* state, const char* source, char* dest, int inputSize, int maxOutputSize);
LZ4_DEPRECATED("use LZ4_compress_fast_continue() instead") LZ4LIB_API int LZ4_compress_continue                (LZ4_stream_t* LZ4_streamPtr, const char* source, char* dest, int inputSize);
LZ4_DEPRECATED("use LZ4_compress_fast_continue() instead") LZ4LIB_API int LZ4_compress_limitedOutput_continue  (LZ4_stream_t* LZ4_streamPtr, const char* source, char* dest, int inputSize, int maxOutputSize);

/*! Obsolete decompression functions (since v1.8.0) */
LZ4_DEPRECATED("use LZ4_decompress_fast() instead") LZ4LIB_API int LZ4_uncompress (const char* source, char* dest, int outputSize);
LZ4_DEPRECATED("use LZ4_decompress_safe() instead") LZ4LIB_API int LZ4_uncompress_unknownOutputSize (const char* source, char* dest, int isize, int maxOutputSize);

/* Obsolete streaming functions (since v1.7.0)
 * degraded functionality; do not use!
 *
 * In order to perform streaming compression, these functions depended on data
 * that is no longer tracked in the state. They have been preserved as well as
 * possible: using them will still produce a correct output. However, they don't
 * actually retain any history between compression calls. The compression ratio
 * achieved will therefore be no better than compressing each chunk
 * independently.
 */
LZ4_DEPRECATED("Use LZ4_createStream() instead") LZ4LIB_API void* LZ4_create (char* inputBuffer);
LZ4_DEPRECATED("Use LZ4_createStream() instead") LZ4LIB_API int   LZ4_sizeofStreamState(void);
LZ4_DEPRECATED("Use LZ4_resetStream() instead")  LZ4LIB_API int   LZ4_resetStreamState(void* state, char* inputBuffer);
LZ4_DEPRECATED("Use LZ4_saveDict() instead")     LZ4LIB_API char* LZ4_slideInputBuffer (void* state);

/*! Obsolete streaming decoding functions (since v1.7.0) */
/* legacy prefix-64k entry points kept for compatibility; implemented using usingDict() variants */
LZ4LIB_API int LZ4_decompress_safe_withPrefix64k (const char* src, char* dst, int compressedSize, int maxDstSize);
LZ4LIB_API int LZ4_decompress_fast_withPrefix64k (const char* src, char* dst, int originalSize);

/*! Obsolete LZ4_decompress_fast variants (since v1.9.0) :
 *  These functions used to be faster than LZ4_decompress_safe(),
 *  but this is no longer the case. They are now slower.
 *  This is because LZ4_decompress_fast() doesn't know the input size,
 *  and therefore must progress more cautiously into the input buffer to not read beyond the end of block.
 *  On top of that `LZ4_decompress_fast()` is not protected vs malformed or malicious inputs, making it a security liability.
 *  As a consequence, LZ4_decompress_fast() is strongly discouraged, and deprecated.
 *
 *  The last remaining LZ4_decompress_fast() specificity is that
 *  it can decompress a block without knowing its compressed size.
 *  Such functionality can be achieved in a more secure manner
 *  by employing LZ4_decompress_safe_partial().
 *
 *  Parameters:
 *  originalSize : is the uncompressed size to regenerate.
 *                 `dst` must be already allocated, its size must be >= 'originalSize' bytes.
 * @return : number of bytes read from source buffer (== compressed size).
 *           The function expects to finish at block's end exactly.
 *           If the source stream is detected malformed, the function stops decoding and returns a negative result.
 *  note : LZ4_decompress_fast*() requires originalSize. Thanks to this information, it never writes past the output buffer.
 *         However, since it doesn't know its 'src' size, it may read an unknown amount of input, past input buffer bounds.
 *         Also, since match offsets are not validated, match reads from 'src' may underflow too.
 *         These issues never happen if input (compressed) data is correct.
 *         But they may happen if input data is invalid (error or intentional tampering).
 *         As a consequence, use these functions in trusted environments with trusted data **only**.
 */
LZ4_DEPRECATED("This function is deprecated and unsafe. Consider using LZ4_decompress_safe_partial() instead")
LZ4LIB_API int LZ4_decompress_fast (const char* src, char* dst, int originalSize);
LZ4_DEPRECATED("This function is deprecated and unsafe. Consider migrating towards LZ4_decompress_safe_continue() instead. "
               "Note that the contract will change (requires block's compressed size, instead of decompressed size)")
LZ4LIB_API int LZ4_decompress_fast_continue (LZ4_streamDecode_t* LZ4_streamDecode, const char* src, char* dst, int originalSize);
LZ4_DEPRECATED("This function is deprecated and unsafe. Consider using LZ4_decompress_safe_partial_usingDict() instead")
LZ4LIB_API int LZ4_decompress_fast_usingDict (const char* src, char* dst, int originalSize, const char* dictStart, int dictSize);

/*! LZ4_resetStream() :
 *  An LZ4_stream_t structure must be initialized at least once.
 *  This is done with LZ4_initStream(), or LZ4_resetStream().
 *  Consider switching to LZ4_initStream(),
 *  invoking LZ4_resetStream() will trigger deprecation warnings in the future.
 */
LZ4LIB_API void LZ4_resetStream (LZ4_stream_t* streamPtr);


#endif /* LZ4_H_98237428734687 */


#if defined (__cplusplus)
}
#endif

/* ==== fastlz implementation ==== */
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Always check for bound when decompressing.
 * Generally it is best to leave it defined.
 */
#define FASTLZ_SAFE


/*
 * Give hints to the compiler for branch prediction optimization.
 */
#if defined(__GNUC__) && (__GNUC__ > 2)
#define FASTLZ_EXPECT_CONDITIONAL(c)    (__builtin_expect((c), 1))
#define FASTLZ_UNEXPECT_CONDITIONAL(c)  (__builtin_expect((c), 0))
#else
#define FASTLZ_EXPECT_CONDITIONAL(c)    (c)
#define FASTLZ_UNEXPECT_CONDITIONAL(c)  (c)
#endif

/*
 * Use inlined functions for supported systems.
 */
#if defined(__GNUC__) || defined(__DMC__) || defined(__POCC__) || defined(__WATCOMC__) || defined(__SUNPRO_C)
#define FASTLZ_INLINE inline
#elif defined(__BORLANDC__) || defined(_MSC_VER) || defined(__LCC__)
#define FASTLZ_INLINE __inline
#else
#define FASTLZ_INLINE
#endif

/*
 * Prevent accessing more than 8-bit at once, except on x86 architectures.
 */
#if !defined(FASTLZ_STRICT_ALIGN)
#define FASTLZ_STRICT_ALIGN
#if defined(__i386__) || defined(__386)  /* GNU C, Sun Studio */
#undef FASTLZ_STRICT_ALIGN
#elif defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(__amd64) /* GNU C */
#undef FASTLZ_STRICT_ALIGN
#elif defined(_M_IX86) /* Intel, MSVC */
#undef FASTLZ_STRICT_ALIGN
#elif defined(__386)
#undef FASTLZ_STRICT_ALIGN
#elif defined(_X86_) /* MinGW */
#undef FASTLZ_STRICT_ALIGN
#elif defined(__I86__) /* Digital Mars */
#undef FASTLZ_STRICT_ALIGN
#endif
#endif

/* prototypes */
int fastlz_compress(const void* input, int length, void* output);
int fastlz_compress_level(int level, const void* input, int length, void* output);
int fastlz_decompress(const void* input, int length, void* output, int maxout);

#define MAX_COPY       32
#define MAX_LEN       264  /* 256 + 8 */
#define MAX_DISTANCE 8192

#if !defined(FASTLZ_STRICT_ALIGN)
#define FASTLZ_READU16(p) *((const flzuint16*)(p))
#else
#define FASTLZ_READU16(p) ((p)[0] | (p)[1]<<8)
#endif

#define HASH_LOG  13
#define HASH_SIZE (1<< HASH_LOG)
#define HASH_MASK  (HASH_SIZE-1)
#define HASH_FUNCTION(v,p) { v = FASTLZ_READU16(p); v ^= FASTLZ_READU16(p+1)^(v>>(16-HASH_LOG));v &= HASH_MASK; }
#undef FASTLZ_LEVEL
#undef MAX_DISTANCE

/* Level 1 implementation */
#define FASTLZ_LEVEL 1
#undef MAX_DISTANCE
#define MAX_DISTANCE 8192
#define FASTLZ_COMPRESSOR fastlz1_compress
#define FASTLZ_DECOMPRESSOR fastlz1_decompress
static FASTLZ_INLINE int FASTLZ_COMPRESSOR(const void* input, int length, void* output)
{
  const flzuint8* ip = (const flzuint8*) input;
  const flzuint8* ip_bound = ip + length - 2;
  const flzuint8* ip_limit = ip + length - 12;
  flzuint8* op = (flzuint8*) output;

  const flzuint8* htab[HASH_SIZE];
  const flzuint8** hslot;
  flzuint32 hval;

  flzuint32 copy;

  /* sanity check */
  if(FASTLZ_UNEXPECT_CONDITIONAL(length < 4))
  {
    if(length)
    {
      /* create literal copy only */
      *op++ = length-1;
      ip_bound++;
      while(ip <= ip_bound)
        *op++ = *ip++;
      return length+1;
    }
    else
      return 0;
  }

  /* initializes hash table */
  for (hslot = htab; hslot < htab + HASH_SIZE; hslot++)
    *hslot = ip;

  /* we start with literal copy */
  copy = 2;
  *op++ = MAX_COPY-1;
  *op++ = *ip++;
  *op++ = *ip++;

  /* main loop */
  while(FASTLZ_EXPECT_CONDITIONAL(ip < ip_limit))
  {
    const flzuint8* ref;
    flzuint32 distance;

    /* minimum match length */
    flzuint32 len = 3;

    /* comparison starting-point */
    const flzuint8* anchor = ip;

    /* check for a run */
#if FASTLZ_LEVEL==2
    if(ip[0] == ip[-1] && FASTLZ_READU16(ip-1)==FASTLZ_READU16(ip+1))
    {
      distance = 1;
      /* ip += 3; */ /* scan-build, never used */
      ref = anchor - 1 + 3;
      goto match;
    }
#endif

    /* find potential match */
    HASH_FUNCTION(hval,ip);
    hslot = htab + hval;
    ref = htab[hval];

    /* calculate distance to the match */
    distance = anchor - ref;

    /* update hash table */
    *hslot = anchor;

    /* is this a match? check the first 3 bytes */
    if(distance==0 ||
#if FASTLZ_LEVEL==1
    (distance >= MAX_DISTANCE) ||
#else
    (distance >= MAX_FARDISTANCE) ||
#endif
    *ref++ != *ip++ || *ref++!=*ip++ || *ref++!=*ip++)
      goto literal;

#if FASTLZ_LEVEL==2
    /* far, needs at least 5-byte match */
    if(distance >= MAX_DISTANCE)
    {
      if(*ip++ != *ref++ || *ip++!= *ref++)
        goto literal;
      len += 2;
    }

    match:
#endif

    /* last matched byte */
    ip = anchor + len;

    /* distance is biased */
    distance--;

    if(!distance)
    {
      /* zero distance means a run */
      flzuint8 x = ip[-1];
      while(ip < ip_bound)
        if(*ref++ != x) break; else ip++;
    }
    else
    for(;;)
    {
      /* safe because the outer check against ip limit */
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      while(ip < ip_bound)
        if(*ref++ != *ip++) break;
      break;
    }

    /* if we have copied something, adjust the copy count */
    if(copy)
      /* copy is biased, '0' means 1 byte copy */
      *(op-copy-1) = copy-1;
    else
      /* back, to overwrite the copy count */
      op--;

    /* reset literal counter */
    copy = 0;

    /* length is biased, '1' means a match of 3 bytes */
    ip -= 3;
    len = ip - anchor;

    /* encode the match */
#if FASTLZ_LEVEL==2
    if(distance < MAX_DISTANCE)
    {
      if(len < 7)
      {
        *op++ = (len << 5) + (distance >> 8);
        *op++ = (distance & 255);
      }
      else
      {
        *op++ = (7 << 5) + (distance >> 8);
        for(len-=7; len >= 255; len-= 255)
          *op++ = 255;
        *op++ = len;
        *op++ = (distance & 255);
      }
    }
    else
    {
      /* far away, but not yet in the another galaxy... */
      if(len < 7)
      {
        distance -= MAX_DISTANCE;
        *op++ = (len << 5) + 31;
        *op++ = 255;
        *op++ = distance >> 8;
        *op++ = distance & 255;
      }
      else
      {
        distance -= MAX_DISTANCE;
        *op++ = (7 << 5) + 31;
        for(len-=7; len >= 255; len-= 255)
          *op++ = 255;
        *op++ = len;
        *op++ = 255;
        *op++ = distance >> 8;
        *op++ = distance & 255;
      }
    }
#else

    if(FASTLZ_UNEXPECT_CONDITIONAL(len > MAX_LEN-2))
      while(len > MAX_LEN-2)
      {
        *op++ = (7 << 5) + (distance >> 8);
        *op++ = MAX_LEN - 2 - 7 -2;
        *op++ = (distance & 255);
        len -= MAX_LEN-2;
      }

    if(len < 7)
    {
      *op++ = (len << 5) + (distance >> 8);
      *op++ = (distance & 255);
    }
    else
    {
      *op++ = (7 << 5) + (distance >> 8);
      *op++ = len - 7;
      *op++ = (distance & 255);
    }
#endif

    /* update the hash at match boundary */
    HASH_FUNCTION(hval,ip);
    htab[hval] = ip++;
    HASH_FUNCTION(hval,ip);
    htab[hval] = ip++;

    /* assuming literal copy */
    *op++ = MAX_COPY-1;

    continue;

    literal:
      *op++ = *anchor++;
      ip = anchor;
      copy++;
      if(FASTLZ_UNEXPECT_CONDITIONAL(copy == MAX_COPY))
      {
        copy = 0;
        *op++ = MAX_COPY-1;
      }
  }

  /* left-over as literal copy */
  ip_bound++;
  while(ip <= ip_bound)
  {
    *op++ = *ip++;
    copy++;
    if(copy == MAX_COPY)
    {
      copy = 0;
      *op++ = MAX_COPY-1;
    }
  }

  /* if we have copied something, adjust the copy length */
  if(copy)
    *(op-copy-1) = copy-1;
  else
    op--;

#if FASTLZ_LEVEL==2
  /* marker for fastlz2 */
  *(flzuint8*)output |= (1 << 5);
#endif

  return op - (flzuint8*)output;
}

static FASTLZ_INLINE int FASTLZ_DECOMPRESSOR(const void* input, int length, void* output, int maxout)
{
  const flzuint8* ip = (const flzuint8*) input;
  const flzuint8* ip_limit  = ip + length;
  flzuint8* op = (flzuint8*) output;
  flzuint8* op_limit = op + maxout;
  flzuint32 ctrl = (*ip++) & 31;
  int loop = 1;

  do
  {
    const flzuint8* ref = op;
    flzuint32 len = ctrl >> 5;
    flzuint32 ofs = (ctrl & 31) << 8;

    if(ctrl >= 32)
    {
#if FASTLZ_LEVEL==2
      flzuint8 code;
#endif
      len--;
      ref -= ofs;
      if (len == 7-1)
#if FASTLZ_LEVEL==1
        len += *ip++;
      ref -= *ip++;
#else
        do
        {
          code = *ip++;
          len += code;
        } while (code==255);
      code = *ip++;
      ref -= code;

      /* match from 16-bit distance */
      if(FASTLZ_UNEXPECT_CONDITIONAL(code==255))
      if(FASTLZ_EXPECT_CONDITIONAL(ofs==(31 << 8)))
      {
        ofs = (*ip++) << 8;
        ofs += *ip++;
        ref = op - ofs - MAX_DISTANCE;
      }
#endif

#ifdef FASTLZ_SAFE
      if (FASTLZ_UNEXPECT_CONDITIONAL(op + len + 3 > op_limit))
        return 0;

      if (FASTLZ_UNEXPECT_CONDITIONAL(ref-1 < (flzuint8 *)output))
        return 0;
#endif

      if(FASTLZ_EXPECT_CONDITIONAL(ip < ip_limit))
        ctrl = *ip++;
      else
        loop = 0;

      if(ref == op)
      {
        /* optimize copy for a run */
        flzuint8 b = ref[-1];
        *op++ = b;
        *op++ = b;
        *op++ = b;
        for(; len; --len)
          *op++ = b;
      }
      else
      {
#if !defined(FASTLZ_STRICT_ALIGN)
        const flzuint16* p;
        flzuint16* q;
#endif
        /* copy from reference */
        ref--;
        *op++ = *ref++;
        *op++ = *ref++;
        *op++ = *ref++;

#if !defined(FASTLZ_STRICT_ALIGN)
        /* copy a byte, so that now it's word aligned */
        if(len & 1)
        {
          *op++ = *ref++;
          len--;
        }

        /* copy 16-bit at once */
        q = (flzuint16*) op;
        op += len;
        p = (const flzuint16*) ref;
        for(len>>=1; len > 4; len-=4)
        {
          *q++ = *p++;
          *q++ = *p++;
          *q++ = *p++;
          *q++ = *p++;
        }
        for(; len; --len)
          *q++ = *p++;
#else
        for(; len; --len)
          *op++ = *ref++;
#endif
      }
    }
    else
    {
      ctrl++;
#ifdef FASTLZ_SAFE
      if (FASTLZ_UNEXPECT_CONDITIONAL(op + ctrl > op_limit))
        return 0;
      if (FASTLZ_UNEXPECT_CONDITIONAL(ip + ctrl > ip_limit))
        return 0;
#endif

      *op++ = *ip++;
      for(--ctrl; ctrl; ctrl--)
        *op++ = *ip++;

      loop = FASTLZ_EXPECT_CONDITIONAL(ip < ip_limit);
      if(loop)
        ctrl = *ip++;
    }
  }
  while(FASTLZ_EXPECT_CONDITIONAL(loop));

  return op - (flzuint8*)output;
}
#undef FASTLZ_COMPRESSOR
#undef FASTLZ_DECOMPRESSOR
#undef MAX_DISTANCE
#undef FASTLZ_LEVEL

/* Level 2 implementation */
#define FASTLZ_LEVEL 2
#undef MAX_DISTANCE
#define MAX_DISTANCE 8191
#define MAX_FARDISTANCE (65535+MAX_DISTANCE-1)
#define FASTLZ_COMPRESSOR fastlz2_compress
#define FASTLZ_DECOMPRESSOR fastlz2_decompress
static FASTLZ_INLINE int FASTLZ_COMPRESSOR(const void* input, int length, void* output)
{
  const flzuint8* ip = (const flzuint8*) input;
  const flzuint8* ip_bound = ip + length - 2;
  const flzuint8* ip_limit = ip + length - 12;
  flzuint8* op = (flzuint8*) output;

  const flzuint8* htab[HASH_SIZE];
  const flzuint8** hslot;
  flzuint32 hval;

  flzuint32 copy;

  /* sanity check */
  if(FASTLZ_UNEXPECT_CONDITIONAL(length < 4))
  {
    if(length)
    {
      /* create literal copy only */
      *op++ = length-1;
      ip_bound++;
      while(ip <= ip_bound)
        *op++ = *ip++;
      return length+1;
    }
    else
      return 0;
  }

  /* initializes hash table */
  for (hslot = htab; hslot < htab + HASH_SIZE; hslot++)
    *hslot = ip;

  /* we start with literal copy */
  copy = 2;
  *op++ = MAX_COPY-1;
  *op++ = *ip++;
  *op++ = *ip++;

  /* main loop */
  while(FASTLZ_EXPECT_CONDITIONAL(ip < ip_limit))
  {
    const flzuint8* ref;
    flzuint32 distance;

    /* minimum match length */
    flzuint32 len = 3;

    /* comparison starting-point */
    const flzuint8* anchor = ip;

    /* check for a run */
#if FASTLZ_LEVEL==2
    if(ip[0] == ip[-1] && FASTLZ_READU16(ip-1)==FASTLZ_READU16(ip+1))
    {
      distance = 1;
      /* ip += 3; */ /* scan-build, never used */
      ref = anchor - 1 + 3;
      goto match;
    }
#endif

    /* find potential match */
    HASH_FUNCTION(hval,ip);
    hslot = htab + hval;
    ref = htab[hval];

    /* calculate distance to the match */
    distance = anchor - ref;

    /* update hash table */
    *hslot = anchor;

    /* is this a match? check the first 3 bytes */
    if(distance==0 ||
#if FASTLZ_LEVEL==1
    (distance >= MAX_DISTANCE) ||
#else
    (distance >= MAX_FARDISTANCE) ||
#endif
    *ref++ != *ip++ || *ref++!=*ip++ || *ref++!=*ip++)
      goto literal;

#if FASTLZ_LEVEL==2
    /* far, needs at least 5-byte match */
    if(distance >= MAX_DISTANCE)
    {
      if(*ip++ != *ref++ || *ip++!= *ref++)
        goto literal;
      len += 2;
    }

    match:
#endif

    /* last matched byte */
    ip = anchor + len;

    /* distance is biased */
    distance--;

    if(!distance)
    {
      /* zero distance means a run */
      flzuint8 x = ip[-1];
      while(ip < ip_bound)
        if(*ref++ != x) break; else ip++;
    }
    else
    for(;;)
    {
      /* safe because the outer check against ip limit */
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      while(ip < ip_bound)
        if(*ref++ != *ip++) break;
      break;
    }

    /* if we have copied something, adjust the copy count */
    if(copy)
      /* copy is biased, '0' means 1 byte copy */
      *(op-copy-1) = copy-1;
    else
      /* back, to overwrite the copy count */
      op--;

    /* reset literal counter */
    copy = 0;

    /* length is biased, '1' means a match of 3 bytes */
    ip -= 3;
    len = ip - anchor;

    /* encode the match */
#if FASTLZ_LEVEL==2
    if(distance < MAX_DISTANCE)
    {
      if(len < 7)
      {
        *op++ = (len << 5) + (distance >> 8);
        *op++ = (distance & 255);
      }
      else
      {
        *op++ = (7 << 5) + (distance >> 8);
        for(len-=7; len >= 255; len-= 255)
          *op++ = 255;
        *op++ = len;
        *op++ = (distance & 255);
      }
    }
    else
    {
      /* far away, but not yet in the another galaxy... */
      if(len < 7)
      {
        distance -= MAX_DISTANCE;
        *op++ = (len << 5) + 31;
        *op++ = 255;
        *op++ = distance >> 8;
        *op++ = distance & 255;
      }
      else
      {
        distance -= MAX_DISTANCE;
        *op++ = (7 << 5) + 31;
        for(len-=7; len >= 255; len-= 255)
          *op++ = 255;
        *op++ = len;
        *op++ = 255;
        *op++ = distance >> 8;
        *op++ = distance & 255;
      }
    }
#else

    if(FASTLZ_UNEXPECT_CONDITIONAL(len > MAX_LEN-2))
      while(len > MAX_LEN-2)
      {
        *op++ = (7 << 5) + (distance >> 8);
        *op++ = MAX_LEN - 2 - 7 -2;
        *op++ = (distance & 255);
        len -= MAX_LEN-2;
      }

    if(len < 7)
    {
      *op++ = (len << 5) + (distance >> 8);
      *op++ = (distance & 255);
    }
    else
    {
      *op++ = (7 << 5) + (distance >> 8);
      *op++ = len - 7;
      *op++ = (distance & 255);
    }
#endif

    /* update the hash at match boundary */
    HASH_FUNCTION(hval,ip);
    htab[hval] = ip++;
    HASH_FUNCTION(hval,ip);
    htab[hval] = ip++;

    /* assuming literal copy */
    *op++ = MAX_COPY-1;

    continue;

    literal:
      *op++ = *anchor++;
      ip = anchor;
      copy++;
      if(FASTLZ_UNEXPECT_CONDITIONAL(copy == MAX_COPY))
      {
        copy = 0;
        *op++ = MAX_COPY-1;
      }
  }

  /* left-over as literal copy */
  ip_bound++;
  while(ip <= ip_bound)
  {
    *op++ = *ip++;
    copy++;
    if(copy == MAX_COPY)
    {
      copy = 0;
      *op++ = MAX_COPY-1;
    }
  }

  /* if we have copied something, adjust the copy length */
  if(copy)
    *(op-copy-1) = copy-1;
  else
    op--;

#if FASTLZ_LEVEL==2
  /* marker for fastlz2 */
  *(flzuint8*)output |= (1 << 5);
#endif

  return op - (flzuint8*)output;
}

static FASTLZ_INLINE int FASTLZ_DECOMPRESSOR(const void* input, int length, void* output, int maxout)
{
  const flzuint8* ip = (const flzuint8*) input;
  const flzuint8* ip_limit  = ip + length;
  flzuint8* op = (flzuint8*) output;
  flzuint8* op_limit = op + maxout;
  flzuint32 ctrl = (*ip++) & 31;
  int loop = 1;

  do
  {
    const flzuint8* ref = op;
    flzuint32 len = ctrl >> 5;
    flzuint32 ofs = (ctrl & 31) << 8;

    if(ctrl >= 32)
    {
#if FASTLZ_LEVEL==2
      flzuint8 code;
#endif
      len--;
      ref -= ofs;
      if (len == 7-1)
#if FASTLZ_LEVEL==1
        len += *ip++;
      ref -= *ip++;
#else
        do
        {
          code = *ip++;
          len += code;
        } while (code==255);
      code = *ip++;
      ref -= code;

      /* match from 16-bit distance */
      if(FASTLZ_UNEXPECT_CONDITIONAL(code==255))
      if(FASTLZ_EXPECT_CONDITIONAL(ofs==(31 << 8)))
      {
        ofs = (*ip++) << 8;
        ofs += *ip++;
        ref = op - ofs - MAX_DISTANCE;
      }
#endif

#ifdef FASTLZ_SAFE
      if (FASTLZ_UNEXPECT_CONDITIONAL(op + len + 3 > op_limit))
        return 0;

      if (FASTLZ_UNEXPECT_CONDITIONAL(ref-1 < (flzuint8 *)output))
        return 0;
#endif

      if(FASTLZ_EXPECT_CONDITIONAL(ip < ip_limit))
        ctrl = *ip++;
      else
        loop = 0;

      if(ref == op)
      {
        /* optimize copy for a run */
        flzuint8 b = ref[-1];
        *op++ = b;
        *op++ = b;
        *op++ = b;
        for(; len; --len)
          *op++ = b;
      }
      else
      {
#if !defined(FASTLZ_STRICT_ALIGN)
        const flzuint16* p;
        flzuint16* q;
#endif
        /* copy from reference */
        ref--;
        *op++ = *ref++;
        *op++ = *ref++;
        *op++ = *ref++;

#if !defined(FASTLZ_STRICT_ALIGN)
        /* copy a byte, so that now it's word aligned */
        if(len & 1)
        {
          *op++ = *ref++;
          len--;
        }

        /* copy 16-bit at once */
        q = (flzuint16*) op;
        op += len;
        p = (const flzuint16*) ref;
        for(len>>=1; len > 4; len-=4)
        {
          *q++ = *p++;
          *q++ = *p++;
          *q++ = *p++;
          *q++ = *p++;
        }
        for(; len; --len)
          *q++ = *p++;
#else
        for(; len; --len)
          *op++ = *ref++;
#endif
      }
    }
    else
    {
      ctrl++;
#ifdef FASTLZ_SAFE
      if (FASTLZ_UNEXPECT_CONDITIONAL(op + ctrl > op_limit))
        return 0;
      if (FASTLZ_UNEXPECT_CONDITIONAL(ip + ctrl > ip_limit))
        return 0;
#endif

      *op++ = *ip++;
      for(--ctrl; ctrl; ctrl--)
        *op++ = *ip++;

      loop = FASTLZ_EXPECT_CONDITIONAL(ip < ip_limit);
      if(loop)
        ctrl = *ip++;
    }
  }
  while(FASTLZ_EXPECT_CONDITIONAL(loop));

  return op - (flzuint8*)output;
}
#undef FASTLZ_COMPRESSOR
#undef FASTLZ_DECOMPRESSOR
#undef MAX_FARDISTANCE
#undef MAX_DISTANCE
#undef FASTLZ_LEVEL
int fastlz_compress(const void* input, int length, void* output)
{
  if(length < 65536)
    return fastlz1_compress(input, length, output);

  return fastlz2_compress(input, length, output);
}

int fastlz_decompress(const void* input, int length, void* output, int maxout)
{
  int level = ((*(const flzuint8*)input) >> 5) + 1;

  if(level == 1)
    return fastlz1_decompress(input, length, output, maxout);
  if(level == 2)
    return fastlz2_decompress(input, length, output, maxout);

  return 0;
}

int fastlz_compress_level(int level, const void* input, int length, void* output)
{
  if(level == 1)
    return fastlz1_compress(input, length, output);
  if(level == 2)
    return fastlz2_compress(input, length, output);

  return 0;
}
#ifdef __cplusplus
}
#endif

/* ==== lz4 implementation ==== */
#ifdef __cplusplus
extern "C" {
#endif
/*
   LZ4 - Fast LZ compression algorithm
   Copyright (C) 2011-2023, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/lz4/lz4
*/

/*-************************************
*  Tuning parameters
**************************************/
/*
 * LZ4_HEAPMODE :
 * Select how stateless compression functions like `LZ4_compress_default()`
 * allocate memory for their hash table,
 * in memory stack (0:default, fastest), or in memory heap (1:requires malloc()).
 */
#ifndef LZ4_HEAPMODE
#  define LZ4_HEAPMODE 0
#endif

/*
 * LZ4_ACCELERATION_DEFAULT :
 * Select "acceleration" for LZ4_compress_fast() when parameter value <= 0
 */
#define LZ4_ACCELERATION_DEFAULT 1
/*
 * LZ4_ACCELERATION_MAX :
 * Any "acceleration" value higher than this threshold
 * get treated as LZ4_ACCELERATION_MAX instead (fix #876)
 */
#define LZ4_ACCELERATION_MAX 65537


/*-************************************
*  CPU Feature Detection
**************************************/
/* LZ4_FORCE_MEMORY_ACCESS
 * By default, access to unaligned memory is controlled by `memcpy()`, which is safe and portable.
 * Unfortunately, on some target/compiler combinations, the generated assembly is sub-optimal.
 * The below switch allow to select different access method for improved performance.
 * Method 0 (default) : use `memcpy()`. Safe and portable.
 * Method 1 : `__packed` statement. It depends on compiler extension (ie, not portable).
 *            This method is safe if your compiler supports it, and *generally* as fast or faster than `memcpy`.
 * Method 2 : direct access. This method is portable but violate C standard.
 *            It can generate buggy code on targets which assembly generation depends on alignment.
 *            But in some circumstances, it's the only known way to get the most performance (ie GCC + ARMv6)
 * See https://fastcompression.blogspot.fr/2015/08/accessing-unaligned-memory.html for details.
 * Prefer these methods in priority order (0 > 1 > 2)
 */
#ifndef LZ4_FORCE_MEMORY_ACCESS   /* can be defined externally */
#  if defined(__GNUC__) && \
  ( defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) \
  || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) )
#    define LZ4_FORCE_MEMORY_ACCESS 2
#  elif (defined(__INTEL_COMPILER) && !defined(_WIN32)) || defined(__GNUC__) || defined(_MSC_VER)
#    define LZ4_FORCE_MEMORY_ACCESS 1
#  endif
#endif

/*
 * LZ4_FORCE_SW_BITCOUNT
 * Define this parameter if your target system or compiler does not support hardware bit count
 */
#if defined(_MSC_VER) && defined(_WIN32_WCE)   /* Visual Studio for WinCE doesn't support Hardware bit count */
#  undef  LZ4_FORCE_SW_BITCOUNT  /* avoid double def */
#  define LZ4_FORCE_SW_BITCOUNT
#endif



/*-************************************
*  Dependency
**************************************/
/*
 * LZ4_SRC_INCLUDED:
 * Amalgamation flag, whether lz4.c is included
 */
#ifndef LZ4_SRC_INCLUDED
#  define LZ4_SRC_INCLUDED 1
#endif

#ifndef LZ4_DISABLE_DEPRECATE_WARNINGS
#  define LZ4_DISABLE_DEPRECATE_WARNINGS /* due to LZ4_decompress_safe_withPrefix64k */
#endif

#ifndef LZ4_STATIC_LINKING_ONLY
#  define LZ4_STATIC_LINKING_ONLY
#endif
/* see also "memory routines" below */


/*-************************************
*  Compiler Options
**************************************/
#if defined(_MSC_VER) && (_MSC_VER >= 1400)  /* Visual Studio 2005+ */
#  include <intrin.h>               /* only present in VS2005+ */
#  pragma warning(disable : 4127)   /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 6237)   /* disable: C6237: conditional expression is always 0 */
#endif  /* _MSC_VER */

#ifndef LZ4_FORCE_INLINE
#  ifdef _MSC_VER    /* Visual Studio */
#    define LZ4_FORCE_INLINE static __forceinline
#  else
#    if defined (__cplusplus) || defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
#      ifdef __GNUC__
#        define LZ4_FORCE_INLINE static inline __attribute__((always_inline))
#      else
#        define LZ4_FORCE_INLINE static inline
#      endif
#    else
#      define LZ4_FORCE_INLINE static
#    endif /* __STDC_VERSION__ */
#  endif  /* _MSC_VER */
#endif /* LZ4_FORCE_INLINE */

/* LZ4_FORCE_O2 and LZ4_FORCE_INLINE
 * gcc on ppc64le generates an unrolled SIMDized loop for LZ4_wildCopy8,
 * together with a simple 8-byte copy loop as a fall-back path.
 * However, this optimization hurts the decompression speed by >30%,
 * because the execution does not go to the optimized loop
 * for typical compressible data, and all of the preamble checks
 * before going to the fall-back path become useless overhead.
 * This optimization happens only with the -O3 flag, and -O2 generates
 * a simple 8-byte copy loop.
 * With gcc on ppc64le, all of the LZ4_decompress_* and LZ4_wildCopy8
 * functions are annotated with __attribute__((optimize("O2"))),
 * and also LZ4_wildCopy8 is forcibly inlined, so that the O2 attribute
 * of LZ4_wildCopy8 does not affect the compression speed.
 */
#if defined(__PPC64__) && defined(__LITTLE_ENDIAN__) && defined(__GNUC__) && !defined(__clang__)
#  define LZ4_FORCE_O2  __attribute__((optimize("O2")))
#  undef LZ4_FORCE_INLINE
#  define LZ4_FORCE_INLINE  static __inline __attribute__((optimize("O2"),always_inline))
#else
#  define LZ4_FORCE_O2
#endif

#if (defined(__GNUC__) && (__GNUC__ >= 3)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) || defined(__clang__)
#  define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
#  define expect(expr,value)    (expr)
#endif

#ifndef likely
#define likely(expr)     expect((expr) != 0, 1)
#endif
#ifndef unlikely
#define unlikely(expr)   expect((expr) != 0, 0)
#endif

/* Should the alignment test prove unreliable, for some reason,
 * it can be disabled by setting LZ4_ALIGN_TEST to 0 */
#ifndef LZ4_ALIGN_TEST  /* can be externally provided */
# define LZ4_ALIGN_TEST 1
#endif


/*-************************************
*  Memory routines
**************************************/

/*! LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION :
 *  Disable relatively high-level LZ4/HC functions that use dynamic memory
 *  allocation functions (malloc(), calloc(), free()).
 *
 *  Note that this is a compile-time switch. And since it disables
 *  public/stable LZ4 v1 API functions, we don't recommend using this
 *  symbol to generate a library for distribution.
 *
 *  The following public functions are removed when this symbol is defined.
 *  - lz4   : LZ4_createStream, LZ4_freeStream,
 *            LZ4_createStreamDecode, LZ4_freeStreamDecode, LZ4_create (deprecated)
 *  - lz4hc : LZ4_createStreamHC, LZ4_freeStreamHC,
 *            LZ4_createHC (deprecated), LZ4_freeHC  (deprecated)
 *  - lz4frame, lz4file : All LZ4F_* functions
 */
#if defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
#  define ALLOC(s)          lz4_error_memory_allocation_is_disabled
#  define ALLOC_AND_ZERO(s) lz4_error_memory_allocation_is_disabled
#  define FREEMEM(p)        lz4_error_memory_allocation_is_disabled
#elif defined(LZ4_USER_MEMORY_FUNCTIONS)
/* memory management functions can be customized by user project.
 * Below functions must exist somewhere in the Project
 * and be available at link time */
void* LZ4_malloc(size_t s);
void* LZ4_calloc(size_t n, size_t s);
void  LZ4_free(void* p);
# define ALLOC(s)          LZ4_malloc(s)
# define ALLOC_AND_ZERO(s) LZ4_calloc(1,s)
# define FREEMEM(p)        LZ4_free(p)
#else
# include <stdlib.h>   /* malloc, calloc, free */
# define ALLOC(s)          malloc(s)
# define ALLOC_AND_ZERO(s) calloc(1,s)
# define FREEMEM(p)        free(p)
#endif

#if ! LZ4_FREESTANDING
#  include <string.h>   /* memset, memcpy */
#endif
#if !defined(LZ4_memset)
#  define LZ4_memset(p,v,s) memset((p),(v),(s))
#endif
#define MEM_INIT(p,v,s)   LZ4_memset((p),(v),(s))


/*-************************************
*  Common Constants
**************************************/
#define MINMATCH 4

#define WILDCOPYLENGTH 8
#define LASTLITERALS   5   /* see ../doc/lz4_Block_format.md#parsing-restrictions */
#define MFLIMIT       12   /* see ../doc/lz4_Block_format.md#parsing-restrictions */
#define MATCH_SAFEGUARD_DISTANCE  ((2*WILDCOPYLENGTH) - MINMATCH)   /* ensure it's possible to write 2 x wildcopyLength without overflowing output buffer */
#define FASTLOOP_SAFE_DISTANCE 64
static const int LZ4_minLength = (MFLIMIT+1);

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define LZ4_DISTANCE_ABSOLUTE_MAX 65535
#if (LZ4_DISTANCE_MAX > LZ4_DISTANCE_ABSOLUTE_MAX)   /* max supported by LZ4 format */
#  error "LZ4_DISTANCE_MAX is too big : must be <= 65535"
#endif

#define ML_BITS  4
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)


/*-************************************
*  Error detection
**************************************/
#if defined(LZ4_DEBUG) && (LZ4_DEBUG>=1)
#  include <assert.h>
#else
#  ifndef assert
#    define assert(condition) ((void)0)
#  endif
#endif

#define LZ4_STATIC_ASSERT(c)   { enum { LZ4_static_assert = 1/(int)(!!(c)) }; }   /* use after variable declarations */

#if defined(LZ4_DEBUG) && (LZ4_DEBUG>=2)
#  include <stdio.h>
   static int g_debuglog_enable = 1;
#  define DEBUGLOG(l, ...) {                          \
        if ((g_debuglog_enable) && (l<=LZ4_DEBUG)) {  \
            fprintf(stderr, __FILE__  " %i: ", __LINE__); \
            fprintf(stderr, __VA_ARGS__);             \
            fprintf(stderr, " \n");                   \
    }   }
#else
#  define DEBUGLOG(l, ...) {}    /* disabled */
#endif

static int LZ4_isAligned(const void* ptr, size_t alignment)
{
    return ((size_t)ptr & (alignment -1)) == 0;
}


/*-************************************
*  Types
**************************************/
#include <limits.h>
#if defined(__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
  typedef uintptr_t uptrval;
#else
# if UINT_MAX != 4294967295UL
#   error "LZ4 code (when not C++ or C99) assumes that sizeof(int) == 4"
# endif
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
  typedef size_t              uptrval;   /* generally true, except OpenVMS-64 */
#endif

#if defined(__x86_64__)
  typedef U64    reg_t;   /* 64-bits in x32 mode */
#else
  typedef size_t reg_t;   /* 32-bits in x32 mode */
#endif

typedef enum {
    notLimited = 0,
    limitedOutput = 1,
    fillOutput = 2
} limitedOutput_directive;


/*-************************************
*  Reading and writing into memory
**************************************/

/**
 * LZ4 relies on memcpy with a constant size being inlined. In freestanding
 * environments, the compiler can't assume the implementation of memcpy() is
 * standard compliant, so it can't apply its specialized memcpy() inlining
 * logic. When possible, use __builtin_memcpy() to tell the compiler to analyze
 * memcpy() as if it were standard compliant, so it can inline it in freestanding
 * environments. This is needed when decompressing the Linux Kernel, for example.
 */
#if !defined(LZ4_memcpy)
#  if defined(__GNUC__) && (__GNUC__ >= 4)
#    define LZ4_memcpy(dst, src, size) __builtin_memcpy(dst, src, size)
#  else
#    define LZ4_memcpy(dst, src, size) memcpy(dst, src, size)
#  endif
#endif

#if !defined(LZ4_memmove)
#  if defined(__GNUC__) && (__GNUC__ >= 4)
#    define LZ4_memmove __builtin_memmove
#  else
#    define LZ4_memmove memmove
#  endif
#endif

static unsigned LZ4_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental */
    return one.c[0];
}

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define LZ4_PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#elif defined(_MSC_VER)
#define LZ4_PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

#if defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==2)
/* lie to the compiler about data alignment; use with caution */

static U16 LZ4_read16(const void* memPtr) { return *(const U16*) memPtr; }
static U32 LZ4_read32(const void* memPtr) { return *(const U32*) memPtr; }
static reg_t LZ4_read_ARCH(const void* memPtr) { return *(const reg_t*) memPtr; }

static void LZ4_write16(void* memPtr, U16 value) { *(U16*)memPtr = value; }
static void LZ4_write32(void* memPtr, U32 value) { *(U32*)memPtr = value; }

#elif defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==1)

/* __pack instructions are safer, but compiler specific, hence potentially problematic for some compilers */
/* currently only defined for gcc and icc */
LZ4_PACK(typedef struct { U16 u16; }) LZ4_unalign16;
LZ4_PACK(typedef struct { U32 u32; }) LZ4_unalign32;
LZ4_PACK(typedef struct { reg_t uArch; }) LZ4_unalignST;

static U16 LZ4_read16(const void* ptr) { return ((const LZ4_unalign16*)ptr)->u16; }
static U32 LZ4_read32(const void* ptr) { return ((const LZ4_unalign32*)ptr)->u32; }
static reg_t LZ4_read_ARCH(const void* ptr) { return ((const LZ4_unalignST*)ptr)->uArch; }

static void LZ4_write16(void* memPtr, U16 value) { ((LZ4_unalign16*)memPtr)->u16 = value; }
static void LZ4_write32(void* memPtr, U32 value) { ((LZ4_unalign32*)memPtr)->u32 = value; }

#else  /* safe and portable access using memcpy() */

static U16 LZ4_read16(const void* memPtr)
{
    U16 val; LZ4_memcpy(&val, memPtr, sizeof(val)); return val;
}

static U32 LZ4_read32(const void* memPtr)
{
    U32 val; LZ4_memcpy(&val, memPtr, sizeof(val)); return val;
}

static reg_t LZ4_read_ARCH(const void* memPtr)
{
    reg_t val; LZ4_memcpy(&val, memPtr, sizeof(val)); return val;
}

static void LZ4_write16(void* memPtr, U16 value)
{
    LZ4_memcpy(memPtr, &value, sizeof(value));
}

static void LZ4_write32(void* memPtr, U32 value)
{
    LZ4_memcpy(memPtr, &value, sizeof(value));
}

#endif /* LZ4_FORCE_MEMORY_ACCESS */


static U16 LZ4_readLE16(const void* memPtr)
{
    if (LZ4_isLittleEndian()) {
        return LZ4_read16(memPtr);
    } else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)((U16)p[0] + (p[1]<<8));
    }
}

#ifdef LZ4_STATIC_LINKING_ONLY_ENDIANNESS_INDEPENDENT_OUTPUT
static U32 LZ4_readLE32(const void* memPtr)
{
    if (LZ4_isLittleEndian()) {
        return LZ4_read32(memPtr);
    } else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U32)p[0] + (p[1]<<8) + (p[2]<<16) + (p[3]<<24);
    }
}
#endif

static void LZ4_writeLE16(void* memPtr, U16 value)
{
    if (LZ4_isLittleEndian()) {
        LZ4_write16(memPtr, value);
    } else {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE) value;
        p[1] = (BYTE)(value>>8);
    }
}

/* customized variant of memcpy, which can overwrite up to 8 bytes beyond dstEnd */
LZ4_FORCE_INLINE
void LZ4_wildCopy8(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;

    do { LZ4_memcpy(d,s,8); d+=8; s+=8; } while (d<e);
}

static const unsigned inc32table[8] = {0, 1, 2,  1,  0,  4, 4, 4};
static const int      dec64table[8] = {0, 0, 0, -1, -4,  1, 2, 3};


#ifndef LZ4_FAST_DEC_LOOP
#  if defined __i386__ || defined _M_IX86 || defined __x86_64__ || defined _M_X64
#    define LZ4_FAST_DEC_LOOP 1
#  elif defined(__aarch64__) && defined(__APPLE__)
#    define LZ4_FAST_DEC_LOOP 1
#  elif defined(__aarch64__) && !defined(__clang__)
     /* On non-Apple aarch64, we disable this optimization for clang because
      * on certain mobile chipsets, performance is reduced with clang. For
      * more information refer to https://github.com/lz4/lz4/pull/707 */
#    define LZ4_FAST_DEC_LOOP 1
#  else
#    define LZ4_FAST_DEC_LOOP 0
#  endif
#endif

#if LZ4_FAST_DEC_LOOP

LZ4_FORCE_INLINE void
LZ4_memcpy_using_offset_base(BYTE* dstPtr, const BYTE* srcPtr, BYTE* dstEnd, const size_t offset)
{
    assert(srcPtr + offset == dstPtr);
    if (offset < 8) {
        LZ4_write32(dstPtr, 0);   /* silence an msan warning when offset==0 */
        dstPtr[0] = srcPtr[0];
        dstPtr[1] = srcPtr[1];
        dstPtr[2] = srcPtr[2];
        dstPtr[3] = srcPtr[3];
        srcPtr += inc32table[offset];
        LZ4_memcpy(dstPtr+4, srcPtr, 4);
        srcPtr -= dec64table[offset];
        dstPtr += 8;
    } else {
        LZ4_memcpy(dstPtr, srcPtr, 8);
        dstPtr += 8;
        srcPtr += 8;
    }

    LZ4_wildCopy8(dstPtr, srcPtr, dstEnd);
}

/* customized variant of memcpy, which can overwrite up to 32 bytes beyond dstEnd
 * this version copies two times 16 bytes (instead of one time 32 bytes)
 * because it must be compatible with offsets >= 16. */
LZ4_FORCE_INLINE void
LZ4_wildCopy32(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;

    do { LZ4_memcpy(d,s,16); LZ4_memcpy(d+16,s+16,16); d+=32; s+=32; } while (d<e);
}

/* LZ4_memcpy_using_offset()  presumes :
 * - dstEnd >= dstPtr + MINMATCH
 * - there is at least 8 bytes available to write after dstEnd */
LZ4_FORCE_INLINE void
LZ4_memcpy_using_offset(BYTE* dstPtr, const BYTE* srcPtr, BYTE* dstEnd, const size_t offset)
{
    BYTE v[8];

    assert(dstEnd >= dstPtr + MINMATCH);

    switch(offset) {
    case 1:
        MEM_INIT(v, *srcPtr, 8);
        break;
    case 2:
        LZ4_memcpy(v, srcPtr, 2);
        LZ4_memcpy(&v[2], srcPtr, 2);
#if defined(_MSC_VER) && (_MSC_VER <= 1937) /* MSVC 2022 ver 17.7 or earlier */
#  pragma warning(push)
#  pragma warning(disable : 6385) /* warning C6385: Reading invalid data from 'v'. */
#endif
        LZ4_memcpy(&v[4], v, 4);
#if defined(_MSC_VER) && (_MSC_VER <= 1937) /* MSVC 2022 ver 17.7 or earlier */
#  pragma warning(pop)
#endif
        break;
    case 4:
        LZ4_memcpy(v, srcPtr, 4);
        LZ4_memcpy(&v[4], srcPtr, 4);
        break;
    default:
        LZ4_memcpy_using_offset_base(dstPtr, srcPtr, dstEnd, offset);
        return;
    }

    LZ4_memcpy(dstPtr, v, 8);
    dstPtr += 8;
    while (dstPtr < dstEnd) {
        LZ4_memcpy(dstPtr, v, 8);
        dstPtr += 8;
    }
}
#endif


/*-************************************
*  Common functions
**************************************/
static unsigned LZ4_NbCommonBytes (reg_t val)
{
    assert(val != 0);
    if (LZ4_isLittleEndian()) {
        if (sizeof(val) == 8) {
#       if defined(_MSC_VER) && (_MSC_VER >= 1800) && (defined(_M_AMD64) && !defined(_M_ARM64EC)) && !defined(LZ4_FORCE_SW_BITCOUNT)
/*-*************************************************************************************************
* ARM64EC is a Microsoft-designed ARM64 ABI compatible with AMD64 applications on ARM64 Windows 11.
* The ARM64EC ABI does not support AVX/AVX2/AVX512 instructions, nor their relevant intrinsics
* including _tzcnt_u64. Therefore, we need to neuter the _tzcnt_u64 code path for ARM64EC.
****************************************************************************************************/
#         if defined(__clang__) && (__clang_major__ < 10)
            /* Avoid undefined clang-cl intrinsics issue.
             * See https://github.com/lz4/lz4/pull/1017 for details. */
            return (unsigned)__builtin_ia32_tzcnt_u64(val) >> 3;
#         else
            /* x64 CPUS without BMI support interpret `TZCNT` as `REP BSF` */
            return (unsigned)_tzcnt_u64(val) >> 3;
#         endif
#       elif defined(_MSC_VER) && defined(_WIN64) && !defined(LZ4_FORCE_SW_BITCOUNT)
            unsigned long r = 0;
            _BitScanForward64(&r, (U64)val);
            return (unsigned)r >> 3;
#       elif (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                                        !defined(LZ4_FORCE_SW_BITCOUNT)
            return (unsigned)__builtin_ctzll((U64)val) >> 3;
#       else
            const U64 m = 0x0101010101010101ULL;
            val ^= val - 1;
            return (unsigned)(((U64)((val & (m - 1)) * m)) >> 56);
#       endif
        } else /* 32 bits */ {
#       if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(LZ4_FORCE_SW_BITCOUNT)
            unsigned long r;
            _BitScanForward(&r, (U32)val);
            return (unsigned)r >> 3;
#       elif (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                        !defined(__TINYC__) && !defined(LZ4_FORCE_SW_BITCOUNT)
            return (unsigned)__builtin_ctz((U32)val) >> 3;
#       else
            const U32 m = 0x01010101;
            return (unsigned)((((val - 1) ^ val) & (m - 1)) * m) >> 24;
#       endif
        }
    } else   /* Big Endian CPU */ {
        if (sizeof(val)==8) {
#       if (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                        !defined(__TINYC__) && !defined(LZ4_FORCE_SW_BITCOUNT)
            return (unsigned)__builtin_clzll((U64)val) >> 3;
#       else
#if 1
            /* this method is probably faster,
             * but adds a 128 bytes lookup table */
            static const unsigned char ctz7_tab[128] = {
                7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
            };
            U64 const mask = 0x0101010101010101ULL;
            U64 const t = (((val >> 8) - mask) | val) & mask;
            return ctz7_tab[(t * 0x0080402010080402ULL) >> 57];
#else
            /* this method doesn't consume memory space like the previous one,
             * but it contains several branches,
             * that may end up slowing execution */
            static const U32 by32 = sizeof(val)*4;  /* 32 on 64 bits (goal), 16 on 32 bits.
            Just to avoid some static analyzer complaining about shift by 32 on 32-bits target.
            Note that this code path is never triggered in 32-bits mode. */
            unsigned r;
            if (!(val>>by32)) { r=4; } else { r=0; val>>=by32; }
            if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
            r += (!val);
            return r;
#endif
#       endif
        } else /* 32 bits */ {
#       if (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                                        !defined(LZ4_FORCE_SW_BITCOUNT)
            return (unsigned)__builtin_clz((U32)val) >> 3;
#       else
            val >>= 8;
            val = ((((val + 0x00FFFF00) | 0x00FFFFFF) + val) |
              (val + 0x00FF0000)) >> 24;
            return (unsigned)val ^ 3;
#       endif
        }
    }
}


#define STEPSIZE sizeof(reg_t)
LZ4_FORCE_INLINE
unsigned LZ4_count(const BYTE* pIn, const BYTE* pMatch, const BYTE* pInLimit)
{
    const BYTE* const pStart = pIn;

    if (likely(pIn < pInLimit-(STEPSIZE-1))) {
        reg_t const diff = LZ4_read_ARCH(pMatch) ^ LZ4_read_ARCH(pIn);
        if (!diff) {
            pIn+=STEPSIZE; pMatch+=STEPSIZE;
        } else {
            return LZ4_NbCommonBytes(diff);
    }   }

    while (likely(pIn < pInLimit-(STEPSIZE-1))) {
        reg_t const diff = LZ4_read_ARCH(pMatch) ^ LZ4_read_ARCH(pIn);
        if (!diff) { pIn+=STEPSIZE; pMatch+=STEPSIZE; continue; }
        pIn += LZ4_NbCommonBytes(diff);
        return (unsigned)(pIn - pStart);
    }

    if ((STEPSIZE==8) && (pIn<(pInLimit-3)) && (LZ4_read32(pMatch) == LZ4_read32(pIn))) { pIn+=4; pMatch+=4; }
    if ((pIn<(pInLimit-1)) && (LZ4_read16(pMatch) == LZ4_read16(pIn))) { pIn+=2; pMatch+=2; }
    if ((pIn<pInLimit) && (*pMatch == *pIn)) pIn++;
    return (unsigned)(pIn - pStart);
}


#ifndef LZ4_COMMONDEFS_ONLY
/*-************************************
*  Local Constants
**************************************/
static const int LZ4_64Klimit = ((64 KB) + (MFLIMIT-1));
static const U32 LZ4_skipTrigger = 6;  /* Increase this value ==> compression run slower on incompressible data */


/*-************************************
*  Local Structures and types
**************************************/
typedef enum { clearedTable = 0, byPtr, byU32, byU16 } tableType_t;

/**
 * This enum distinguishes several different modes of accessing previous
 * content in the stream.
 *
 * - noDict        : There is no preceding content.
 * - withPrefix64k : Table entries up to ctx->dictSize before the current blob
 *                   blob being compressed are valid and refer to the preceding
 *                   content (of length ctx->dictSize), which is available
 *                   contiguously preceding in memory the content currently
 *                   being compressed.
 * - usingExtDict  : Like withPrefix64k, but the preceding content is somewhere
 *                   else in memory, starting at ctx->dictionary with length
 *                   ctx->dictSize.
 * - usingDictCtx  : Everything concerning the preceding content is
 *                   in a separate context, pointed to by ctx->dictCtx.
 *                   ctx->dictionary, ctx->dictSize, and table entries
 *                   in the current context that refer to positions
 *                   preceding the beginning of the current compression are
 *                   ignored. Instead, ctx->dictCtx->dictionary and ctx->dictCtx
 *                   ->dictSize describe the location and size of the preceding
 *                   content, and matches are found by looking in the ctx
 *                   ->dictCtx->hashTable.
 */
typedef enum { noDict = 0, withPrefix64k, usingExtDict, usingDictCtx } dict_directive;
typedef enum { noDictIssue = 0, dictSmall } dictIssue_directive;


/*-************************************
*  Local Utils
**************************************/
int LZ4_versionNumber (void) { return LZ4_VERSION_NUMBER; }
const char* LZ4_versionString(void) { return LZ4_VERSION_STRING; }
int LZ4_compressBound(int isize)  { return LZ4_COMPRESSBOUND(isize); }
int LZ4_sizeofState(void) { return sizeof(LZ4_stream_t); }


/*-****************************************
*  Internal Definitions, used only in Tests
*******************************************/
#if defined (__cplusplus)
extern "C" {
#endif

int LZ4_compress_forceExtDict (LZ4_stream_t* LZ4_dict, const char* source, char* dest, int srcSize);

int LZ4_decompress_safe_forceExtDict(const char* source, char* dest,
                                     int compressedSize, int maxOutputSize,
                                     const void* dictStart, size_t dictSize);
int LZ4_decompress_safe_partial_forceExtDict(const char* source, char* dest,
                                     int compressedSize, int targetOutputSize, int dstCapacity,
                                     const void* dictStart, size_t dictSize);
#if defined (__cplusplus)
}
#endif

/*-******************************
*  Compression functions
********************************/
LZ4_FORCE_INLINE U32 LZ4_hash4(U32 sequence, tableType_t const tableType)
{
    if (tableType == byU16)
        return ((sequence * 2654435761U) >> ((MINMATCH*8)-(LZ4_HASHLOG+1)));
    else
        return ((sequence * 2654435761U) >> ((MINMATCH*8)-LZ4_HASHLOG));
}

LZ4_FORCE_INLINE U32 LZ4_hash5(U64 sequence, tableType_t const tableType)
{
    const U32 hashLog = (tableType == byU16) ? LZ4_HASHLOG+1 : LZ4_HASHLOG;
    if (LZ4_isLittleEndian()) {
        const U64 prime5bytes = 889523592379ULL;
        return (U32)(((sequence << 24) * prime5bytes) >> (64 - hashLog));
    } else {
        const U64 prime8bytes = 11400714785074694791ULL;
        return (U32)(((sequence >> 24) * prime8bytes) >> (64 - hashLog));
    }
}

LZ4_FORCE_INLINE U32 LZ4_hashPosition(const void* const p, tableType_t const tableType)
{
    if ((sizeof(reg_t)==8) && (tableType != byU16)) return LZ4_hash5(LZ4_read_ARCH(p), tableType);

#ifdef LZ4_STATIC_LINKING_ONLY_ENDIANNESS_INDEPENDENT_OUTPUT
    return LZ4_hash4(LZ4_readLE32(p), tableType);
#else
    return LZ4_hash4(LZ4_read32(p), tableType);
#endif
}

LZ4_FORCE_INLINE void LZ4_clearHash(U32 h, void* tableBase, tableType_t const tableType)
{
    switch (tableType)
    {
    default: /* fallthrough */
    case clearedTable: { /* illegal! */ assert(0); return; }
    case byPtr: { const BYTE** hashTable = (const BYTE**)tableBase; hashTable[h] = NULL; return; }
    case byU32: { U32* hashTable = (U32*) tableBase; hashTable[h] = 0; return; }
    case byU16: { U16* hashTable = (U16*) tableBase; hashTable[h] = 0; return; }
    }
}

LZ4_FORCE_INLINE void LZ4_putIndexOnHash(U32 idx, U32 h, void* tableBase, tableType_t const tableType)
{
    switch (tableType)
    {
    default: /* fallthrough */
    case clearedTable: /* fallthrough */
    case byPtr: { /* illegal! */ assert(0); return; }
    case byU32: { U32* hashTable = (U32*) tableBase; hashTable[h] = idx; return; }
    case byU16: { U16* hashTable = (U16*) tableBase; assert(idx < 65536); hashTable[h] = (U16)idx; return; }
    }
}

/* LZ4_putPosition*() : only used in byPtr mode */
LZ4_FORCE_INLINE void LZ4_putPositionOnHash(const BYTE* p, U32 h,
                                  void* tableBase, tableType_t const tableType)
{
    const BYTE** const hashTable = (const BYTE**)tableBase;
    assert(tableType == byPtr); (void)tableType;
    hashTable[h] = p;
}

LZ4_FORCE_INLINE void LZ4_putPosition(const BYTE* p, void* tableBase, tableType_t tableType)
{
    U32 const h = LZ4_hashPosition(p, tableType);
    LZ4_putPositionOnHash(p, h, tableBase, tableType);
}

/* LZ4_getIndexOnHash() :
 * Index of match position registered in hash table.
 * hash position must be calculated by using base+index, or dictBase+index.
 * Assumption 1 : only valid if tableType == byU32 or byU16.
 * Assumption 2 : h is presumed valid (within limits of hash table)
 */
LZ4_FORCE_INLINE U32 LZ4_getIndexOnHash(U32 h, const void* tableBase, tableType_t tableType)
{
    LZ4_STATIC_ASSERT(LZ4_MEMORY_USAGE > 2);
    if (tableType == byU32) {
        const U32* const hashTable = (const U32*) tableBase;
        assert(h < (1U << (LZ4_MEMORY_USAGE-2)));
        return hashTable[h];
    }
    if (tableType == byU16) {
        const U16* const hashTable = (const U16*) tableBase;
        assert(h < (1U << (LZ4_MEMORY_USAGE-1)));
        return hashTable[h];
    }
    assert(0); return 0;  /* forbidden case */
}

static const BYTE* LZ4_getPositionOnHash(U32 h, const void* tableBase, tableType_t tableType)
{
    assert(tableType == byPtr); (void)tableType;
    { const BYTE* const* hashTable = (const BYTE* const*) tableBase; return hashTable[h]; }
}

LZ4_FORCE_INLINE const BYTE*
LZ4_getPosition(const BYTE* p,
                const void* tableBase, tableType_t tableType)
{
    U32 const h = LZ4_hashPosition(p, tableType);
    return LZ4_getPositionOnHash(h, tableBase, tableType);
}

LZ4_FORCE_INLINE void
LZ4_prepareTable(LZ4_stream_t_internal* const cctx,
           const int inputSize,
           const tableType_t tableType) {
    /* If the table hasn't been used, it's guaranteed to be zeroed out, and is
     * therefore safe to use no matter what mode we're in. Otherwise, we figure
     * out if it's safe to leave as is or whether it needs to be reset.
     */
    if ((tableType_t)cctx->tableType != clearedTable) {
        assert(inputSize >= 0);
        if ((tableType_t)cctx->tableType != tableType
          || ((tableType == byU16) && cctx->currentOffset + (unsigned)inputSize >= 0xFFFFU)
          || ((tableType == byU32) && cctx->currentOffset > 1 GB)
          || tableType == byPtr
          || inputSize >= 4 KB)
        {
            DEBUGLOG(4, "LZ4_prepareTable: Resetting table in %p", cctx);
            MEM_INIT(cctx->hashTable, 0, LZ4_HASHTABLESIZE);
            cctx->currentOffset = 0;
            cctx->tableType = (U32)clearedTable;
        } else {
            DEBUGLOG(4, "LZ4_prepareTable: Re-use hash table (no reset)");
        }
    }

    /* Adding a gap, so all previous entries are > LZ4_DISTANCE_MAX back,
     * is faster than compressing without a gap.
     * However, compressing with currentOffset == 0 is faster still,
     * so we preserve that case.
     */
    if (cctx->currentOffset != 0 && tableType == byU32) {
        DEBUGLOG(5, "LZ4_prepareTable: adding 64KB to currentOffset");
        cctx->currentOffset += 64 KB;
    }

    /* Finally, clear history */
    cctx->dictCtx = NULL;
    cctx->dictionary = NULL;
    cctx->dictSize = 0;
}

/** LZ4_compress_generic_validated() :
 *  inlined, to ensure branches are decided at compilation time.
 *  The following conditions are presumed already validated:
 *  - source != NULL
 *  - inputSize > 0
 */
LZ4_FORCE_INLINE int LZ4_compress_generic_validated(
                 LZ4_stream_t_internal* const cctx,
                 const char* const source,
                 char* const dest,
                 const int inputSize,
                 int*  inputConsumed, /* only written when outputDirective == fillOutput */
                 const int maxOutputSize,
                 const limitedOutput_directive outputDirective,
                 const tableType_t tableType,
                 const dict_directive dictDirective,
                 const dictIssue_directive dictIssue,
                 const int acceleration)
{
    int result;
    const BYTE* ip = (const BYTE*)source;

    U32 const startIndex = cctx->currentOffset;
    const BYTE* base = (const BYTE*)source - startIndex;
    const BYTE* lowLimit;

    const LZ4_stream_t_internal* dictCtx = (const LZ4_stream_t_internal*) cctx->dictCtx;
    const BYTE* const dictionary =
        dictDirective == usingDictCtx ? dictCtx->dictionary : cctx->dictionary;
    const U32 dictSize =
        dictDirective == usingDictCtx ? dictCtx->dictSize : cctx->dictSize;
    const U32 dictDelta =
        (dictDirective == usingDictCtx) ? startIndex - dictCtx->currentOffset : 0;   /* make indexes in dictCtx comparable with indexes in current context */

    int const maybe_extMem = (dictDirective == usingExtDict) || (dictDirective == usingDictCtx);
    U32 const prefixIdxLimit = startIndex - dictSize;   /* used when dictDirective == dictSmall */
    const BYTE* const dictEnd = dictionary ? dictionary + dictSize : dictionary;
    const BYTE* anchor = (const BYTE*) source;
    const BYTE* const iend = ip + inputSize;
    const BYTE* const mflimitPlusOne = iend - MFLIMIT + 1;
    const BYTE* const matchlimit = iend - LASTLITERALS;

    /* the dictCtx currentOffset is indexed on the start of the dictionary,
     * while a dictionary in the current context precedes the currentOffset */
    const BYTE* dictBase = (dictionary == NULL) ? NULL :
                           (dictDirective == usingDictCtx) ?
                            dictionary + dictSize - dictCtx->currentOffset :
                            dictionary + dictSize - startIndex;

    BYTE* op = (BYTE*) dest;
    BYTE* const olimit = op + maxOutputSize;

    U32 offset = 0;
    U32 forwardH;

    DEBUGLOG(5, "LZ4_compress_generic_validated: srcSize=%i, tableType=%u", inputSize, tableType);
    assert(ip != NULL);
    if (tableType == byU16) assert(inputSize<LZ4_64Klimit);  /* Size too large (not within 64K limit) */
    if (tableType == byPtr) assert(dictDirective==noDict);   /* only supported use case with byPtr */
    /* If init conditions are not met, we don't have to mark stream
     * as having dirty context, since no action was taken yet */
    if (outputDirective == fillOutput && maxOutputSize < 1) { return 0; } /* Impossible to store anything */
    assert(acceleration >= 1);

    lowLimit = (const BYTE*)source - (dictDirective == withPrefix64k ? dictSize : 0);

    /* Update context state */
    if (dictDirective == usingDictCtx) {
        /* Subsequent linked blocks can't use the dictionary. */
        /* Instead, they use the block we just compressed. */
        cctx->dictCtx = NULL;
        cctx->dictSize = (U32)inputSize;
    } else {
        cctx->dictSize += (U32)inputSize;
    }
    cctx->currentOffset += (U32)inputSize;
    cctx->tableType = (U32)tableType;

    if (inputSize<LZ4_minLength) goto _last_literals;        /* Input too small, no compression (all literals) */

    /* First Byte */
    {   U32 const h = LZ4_hashPosition(ip, tableType);
        if (tableType == byPtr) {
            LZ4_putPositionOnHash(ip, h, cctx->hashTable, byPtr);
        } else {
            LZ4_putIndexOnHash(startIndex, h, cctx->hashTable, tableType);
    }   }
    ip++; forwardH = LZ4_hashPosition(ip, tableType);

    /* Main Loop */
    for ( ; ; ) {
        const BYTE* match;
        BYTE* token;
        const BYTE* filledIp;

        /* Find a match */
        if (tableType == byPtr) {
            const BYTE* forwardIp = ip;
            int step = 1;
            int searchMatchNb = acceleration << LZ4_skipTrigger;
            do {
                U32 const h = forwardH;
                ip = forwardIp;
                forwardIp += step;
                step = (searchMatchNb++ >> LZ4_skipTrigger);

                if (unlikely(forwardIp > mflimitPlusOne)) goto _last_literals;
                assert(ip < mflimitPlusOne);

                match = LZ4_getPositionOnHash(h, cctx->hashTable, tableType);
                forwardH = LZ4_hashPosition(forwardIp, tableType);
                LZ4_putPositionOnHash(ip, h, cctx->hashTable, tableType);

            } while ( (match+LZ4_DISTANCE_MAX < ip)
                   || (LZ4_read32(match) != LZ4_read32(ip)) );

        } else {   /* byU32, byU16 */

            const BYTE* forwardIp = ip;
            int step = 1;
            int searchMatchNb = acceleration << LZ4_skipTrigger;
            do {
                U32 const h = forwardH;
                U32 const current = (U32)(forwardIp - base);
                U32 matchIndex = LZ4_getIndexOnHash(h, cctx->hashTable, tableType);
                assert(matchIndex <= current);
                assert(forwardIp - base < (ptrdiff_t)(2 GB - 1));
                ip = forwardIp;
                forwardIp += step;
                step = (searchMatchNb++ >> LZ4_skipTrigger);

                if (unlikely(forwardIp > mflimitPlusOne)) goto _last_literals;
                assert(ip < mflimitPlusOne);

                if (dictDirective == usingDictCtx) {
                    if (matchIndex < startIndex) {
                        /* there was no match, try the dictionary */
                        assert(tableType == byU32);
                        matchIndex = LZ4_getIndexOnHash(h, dictCtx->hashTable, byU32);
                        match = dictBase + matchIndex;
                        matchIndex += dictDelta;   /* make dictCtx index comparable with current context */
                        lowLimit = dictionary;
                    } else {
                        match = base + matchIndex;
                        lowLimit = (const BYTE*)source;
                    }
                } else if (dictDirective == usingExtDict) {
                    if (matchIndex < startIndex) {
                        DEBUGLOG(7, "extDict candidate: matchIndex=%5u  <  startIndex=%5u", matchIndex, startIndex);
                        assert(startIndex - matchIndex >= MINMATCH);
                        assert(dictBase);
                        match = dictBase + matchIndex;
                        lowLimit = dictionary;
                    } else {
                        match = base + matchIndex;
                        lowLimit = (const BYTE*)source;
                    }
                } else {   /* single continuous memory segment */
                    match = base + matchIndex;
                }
                forwardH = LZ4_hashPosition(forwardIp, tableType);
                LZ4_putIndexOnHash(current, h, cctx->hashTable, tableType);

                DEBUGLOG(7, "candidate at pos=%u  (offset=%u \n", matchIndex, current - matchIndex);
                if ((dictIssue == dictSmall) && (matchIndex < prefixIdxLimit)) { continue; }    /* match outside of valid area */
                assert(matchIndex < current);
                if ( ((tableType != byU16) || (LZ4_DISTANCE_MAX < LZ4_DISTANCE_ABSOLUTE_MAX))
                  && (matchIndex+LZ4_DISTANCE_MAX < current)) {
                    continue;
                } /* too far */
                assert((current - matchIndex) <= LZ4_DISTANCE_MAX);  /* match now expected within distance */

                if (LZ4_read32(match) == LZ4_read32(ip)) {
                    if (maybe_extMem) offset = current - matchIndex;
                    break;   /* match found */
                }

            } while(1);
        }

        /* Catch up */
        filledIp = ip;
        assert(ip > anchor); /* this is always true as ip has been advanced before entering the main loop */
        if ((match > lowLimit) && unlikely(ip[-1] == match[-1])) {
            do { ip--; match--; } while (((ip > anchor) & (match > lowLimit)) && (unlikely(ip[-1] == match[-1])));
        }

        /* Encode Literals */
        {   unsigned const litLength = (unsigned)(ip - anchor);
            token = op++;
            if ((outputDirective == limitedOutput) &&  /* Check output buffer overflow */
                (unlikely(op + litLength + (2 + 1 + LASTLITERALS) + (litLength/255) > olimit)) ) {
                return 0;   /* cannot compress within `dst` budget. Stored indexes in hash table are nonetheless fine */
            }
            if ((outputDirective == fillOutput) &&
                (unlikely(op + (litLength+240)/255 /* litlen */ + litLength /* literals */ + 2 /* offset */ + 1 /* token */ + MFLIMIT - MINMATCH /* min last literals so last match is <= end - MFLIMIT */ > olimit))) {
                op--;
                goto _last_literals;
            }
            if (litLength >= RUN_MASK) {
                int len = (int)(litLength - RUN_MASK);
                *token = (RUN_MASK<<ML_BITS);
                for(; len >= 255 ; len-=255) *op++ = 255;
                *op++ = (BYTE)len;
            }
            else *token = (BYTE)(litLength<<ML_BITS);

            /* Copy Literals */
            LZ4_wildCopy8(op, anchor, op+litLength);
            op+=litLength;
            DEBUGLOG(6, "seq.start:%i, literals=%u, match.start:%i",
                        (int)(anchor-(const BYTE*)source), litLength, (int)(ip-(const BYTE*)source));
        }

_next_match:
        /* at this stage, the following variables must be correctly set :
         * - ip : at start of LZ operation
         * - match : at start of previous pattern occurrence; can be within current prefix, or within extDict
         * - offset : if maybe_ext_memSegment==1 (constant)
         * - lowLimit : must be == dictionary to mean "match is within extDict"; must be == source otherwise
         * - token and *token : position to write 4-bits for match length; higher 4-bits for literal length supposed already written
         */

        if ((outputDirective == fillOutput) &&
            (op + 2 /* offset */ + 1 /* token */ + MFLIMIT - MINMATCH /* min last literals so last match is <= end - MFLIMIT */ > olimit)) {
            /* the match was too close to the end, rewind and go to last literals */
            op = token;
            goto _last_literals;
        }

        /* Encode Offset */
        if (maybe_extMem) {   /* static test */
            DEBUGLOG(6, "             with offset=%u  (ext if > %i)", offset, (int)(ip - (const BYTE*)source));
            assert(offset <= LZ4_DISTANCE_MAX && offset > 0);
            LZ4_writeLE16(op, (U16)offset); op+=2;
        } else  {
            DEBUGLOG(6, "             with offset=%u  (same segment)", (U32)(ip - match));
            assert(ip-match <= LZ4_DISTANCE_MAX);
            LZ4_writeLE16(op, (U16)(ip - match)); op+=2;
        }

        /* Encode MatchLength */
        {   unsigned matchCode;

            if ( (dictDirective==usingExtDict || dictDirective==usingDictCtx)
              && (lowLimit==dictionary) /* match within extDict */ ) {
                const BYTE* limit = ip + (dictEnd-match);
                assert(dictEnd > match);
                if (limit > matchlimit) limit = matchlimit;
                matchCode = LZ4_count(ip+MINMATCH, match+MINMATCH, limit);
                ip += (size_t)matchCode + MINMATCH;
                if (ip==limit) {
                    unsigned const more = LZ4_count(limit, (const BYTE*)source, matchlimit);
                    matchCode += more;
                    ip += more;
                }
                DEBUGLOG(6, "             with matchLength=%u starting in extDict", matchCode+MINMATCH);
            } else {
                matchCode = LZ4_count(ip+MINMATCH, match+MINMATCH, matchlimit);
                ip += (size_t)matchCode + MINMATCH;
                DEBUGLOG(6, "             with matchLength=%u", matchCode+MINMATCH);
            }

            if ((outputDirective) &&    /* Check output buffer overflow */
                (unlikely(op + (1 + LASTLITERALS) + (matchCode+240)/255 > olimit)) ) {
                if (outputDirective == fillOutput) {
                    /* Match description too long : reduce it */
                    U32 newMatchCode = 15 /* in token */ - 1 /* to avoid needing a zero byte */ + ((U32)(olimit - op) - 1 - LASTLITERALS) * 255;
                    ip -= matchCode - newMatchCode;
                    assert(newMatchCode < matchCode);
                    matchCode = newMatchCode;
                    if (unlikely(ip <= filledIp)) {
                        /* We have already filled up to filledIp so if ip ends up less than filledIp
                         * we have positions in the hash table beyond the current position. This is
                         * a problem if we reuse the hash table. So we have to remove these positions
                         * from the hash table.
                         */
                        const BYTE* ptr;
                        DEBUGLOG(5, "Clearing %u positions", (U32)(filledIp - ip));
                        for (ptr = ip; ptr <= filledIp; ++ptr) {
                            U32 const h = LZ4_hashPosition(ptr, tableType);
                            LZ4_clearHash(h, cctx->hashTable, tableType);
                        }
                    }
                } else {
                    assert(outputDirective == limitedOutput);
                    return 0;   /* cannot compress within `dst` budget. Stored indexes in hash table are nonetheless fine */
                }
            }
            if (matchCode >= ML_MASK) {
                *token += ML_MASK;
                matchCode -= ML_MASK;
                LZ4_write32(op, 0xFFFFFFFF);
                while (matchCode >= 4*255) {
                    op+=4;
                    LZ4_write32(op, 0xFFFFFFFF);
                    matchCode -= 4*255;
                }
                op += matchCode / 255;
                *op++ = (BYTE)(matchCode % 255);
            } else
                *token += (BYTE)(matchCode);
        }
        /* Ensure we have enough space for the last literals. */
        assert(!(outputDirective == fillOutput && op + 1 + LASTLITERALS > olimit));

        anchor = ip;

        /* Test end of chunk */
        if (ip >= mflimitPlusOne) break;

        /* Fill table */
        {   U32 const h = LZ4_hashPosition(ip-2, tableType);
            if (tableType == byPtr) {
                LZ4_putPositionOnHash(ip-2, h, cctx->hashTable, byPtr);
            } else {
                U32 const idx = (U32)((ip-2) - base);
                LZ4_putIndexOnHash(idx, h, cctx->hashTable, tableType);
        }   }

        /* Test next position */
        if (tableType == byPtr) {

            match = LZ4_getPosition(ip, cctx->hashTable, tableType);
            LZ4_putPosition(ip, cctx->hashTable, tableType);
            if ( (match+LZ4_DISTANCE_MAX >= ip)
              && (LZ4_read32(match) == LZ4_read32(ip)) )
            { token=op++; *token=0; goto _next_match; }

        } else {   /* byU32, byU16 */

            U32 const h = LZ4_hashPosition(ip, tableType);
            U32 const current = (U32)(ip-base);
            U32 matchIndex = LZ4_getIndexOnHash(h, cctx->hashTable, tableType);
            assert(matchIndex < current);
            if (dictDirective == usingDictCtx) {
                if (matchIndex < startIndex) {
                    /* there was no match, try the dictionary */
                    assert(tableType == byU32);
                    matchIndex = LZ4_getIndexOnHash(h, dictCtx->hashTable, byU32);
                    match = dictBase + matchIndex;
                    lowLimit = dictionary;   /* required for match length counter */
                    matchIndex += dictDelta;
                } else {
                    match = base + matchIndex;
                    lowLimit = (const BYTE*)source;  /* required for match length counter */
                }
            } else if (dictDirective==usingExtDict) {
                if (matchIndex < startIndex) {
                    assert(dictBase);
                    match = dictBase + matchIndex;
                    lowLimit = dictionary;   /* required for match length counter */
                } else {
                    match = base + matchIndex;
                    lowLimit = (const BYTE*)source;   /* required for match length counter */
                }
            } else {   /* single memory segment */
                match = base + matchIndex;
            }
            LZ4_putIndexOnHash(current, h, cctx->hashTable, tableType);
            assert(matchIndex < current);
            if ( ((dictIssue==dictSmall) ? (matchIndex >= prefixIdxLimit) : 1)
              && (((tableType==byU16) && (LZ4_DISTANCE_MAX == LZ4_DISTANCE_ABSOLUTE_MAX)) ? 1 : (matchIndex+LZ4_DISTANCE_MAX >= current))
              && (LZ4_read32(match) == LZ4_read32(ip)) ) {
                token=op++;
                *token=0;
                if (maybe_extMem) offset = current - matchIndex;
                DEBUGLOG(6, "seq.start:%i, literals=%u, match.start:%i",
                            (int)(anchor-(const BYTE*)source), 0, (int)(ip-(const BYTE*)source));
                goto _next_match;
            }
        }

        /* Prepare next loop */
        forwardH = LZ4_hashPosition(++ip, tableType);

    }

_last_literals:
    /* Encode Last Literals */
    {   size_t lastRun = (size_t)(iend - anchor);
        if ( (outputDirective) &&  /* Check output buffer overflow */
            (op + lastRun + 1 + ((lastRun+255-RUN_MASK)/255) > olimit)) {
            if (outputDirective == fillOutput) {
                /* adapt lastRun to fill 'dst' */
                assert(olimit >= op);
                lastRun  = (size_t)(olimit-op) - 1/*token*/;
                lastRun -= (lastRun + 256 - RUN_MASK) / 256;  /*additional length tokens*/
            } else {
                assert(outputDirective == limitedOutput);
                return 0;   /* cannot compress within `dst` budget. Stored indexes in hash table are nonetheless fine */
            }
        }
        DEBUGLOG(6, "Final literal run : %i literals", (int)lastRun);
        if (lastRun >= RUN_MASK) {
            size_t accumulator = lastRun - RUN_MASK;
            *op++ = RUN_MASK << ML_BITS;
            for(; accumulator >= 255 ; accumulator-=255) *op++ = 255;
            *op++ = (BYTE) accumulator;
        } else {
            *op++ = (BYTE)(lastRun<<ML_BITS);
        }
        LZ4_memcpy(op, anchor, lastRun);
        ip = anchor + lastRun;
        op += lastRun;
    }

    if (outputDirective == fillOutput) {
        *inputConsumed = (int) (((const char*)ip)-source);
    }
    result = (int)(((char*)op) - dest);
    assert(result > 0);
    DEBUGLOG(5, "LZ4_compress_generic: compressed %i bytes into %i bytes", inputSize, result);
    return result;
}

/** LZ4_compress_generic() :
 *  inlined, to ensure branches are decided at compilation time;
 *  takes care of src == (NULL, 0)
 *  and forward the rest to LZ4_compress_generic_validated */
LZ4_FORCE_INLINE int LZ4_compress_generic(
                 LZ4_stream_t_internal* const cctx,
                 const char* const src,
                 char* const dst,
                 const int srcSize,
                 int *inputConsumed, /* only written when outputDirective == fillOutput */
                 const int dstCapacity,
                 const limitedOutput_directive outputDirective,
                 const tableType_t tableType,
                 const dict_directive dictDirective,
                 const dictIssue_directive dictIssue,
                 const int acceleration)
{
    DEBUGLOG(5, "LZ4_compress_generic: srcSize=%i, dstCapacity=%i",
                srcSize, dstCapacity);

    if ((U32)srcSize > (U32)LZ4_MAX_INPUT_SIZE) { return 0; }  /* Unsupported srcSize, too large (or negative) */
    if (srcSize == 0) {   /* src == NULL supported if srcSize == 0 */
        if (outputDirective != notLimited && dstCapacity <= 0) return 0;  /* no output, can't write anything */
        DEBUGLOG(5, "Generating an empty block");
        assert(outputDirective == notLimited || dstCapacity >= 1);
        assert(dst != NULL);
        dst[0] = 0;
        if (outputDirective == fillOutput) {
            assert (inputConsumed != NULL);
            *inputConsumed = 0;
        }
        return 1;
    }
    assert(src != NULL);

    return LZ4_compress_generic_validated(cctx, src, dst, srcSize,
                inputConsumed, /* only written into if outputDirective == fillOutput */
                dstCapacity, outputDirective,
                tableType, dictDirective, dictIssue, acceleration);
}


int LZ4_compress_fast_extState(void* state, const char* source, char* dest, int inputSize, int maxOutputSize, int acceleration)
{
    LZ4_stream_t_internal* const ctx = & LZ4_initStream(state, sizeof(LZ4_stream_t)) -> internal_donotuse;
    assert(ctx != NULL);
    if (acceleration < 1) acceleration = LZ4_ACCELERATION_DEFAULT;
    if (acceleration > LZ4_ACCELERATION_MAX) acceleration = LZ4_ACCELERATION_MAX;
    if (maxOutputSize >= LZ4_compressBound(inputSize)) {
        if (inputSize < LZ4_64Klimit) {
            return LZ4_compress_generic(ctx, source, dest, inputSize, NULL, 0, notLimited, byU16, noDict, noDictIssue, acceleration);
        } else {
            const tableType_t tableType = ((sizeof(void*)==4) && ((uptrval)source > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            return LZ4_compress_generic(ctx, source, dest, inputSize, NULL, 0, notLimited, tableType, noDict, noDictIssue, acceleration);
        }
    } else {
        if (inputSize < LZ4_64Klimit) {
            return LZ4_compress_generic(ctx, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, byU16, noDict, noDictIssue, acceleration);
        } else {
            const tableType_t tableType = ((sizeof(void*)==4) && ((uptrval)source > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            return LZ4_compress_generic(ctx, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, noDict, noDictIssue, acceleration);
        }
    }
}

/**
 * LZ4_compress_fast_extState_fastReset() :
 * A variant of LZ4_compress_fast_extState().
 *
 * Using this variant avoids an expensive initialization step. It is only safe
 * to call if the state buffer is known to be correctly initialized already
 * (see comment in lz4.h on LZ4_resetStream_fast() for a definition of
 * "correctly initialized").
 */
int LZ4_compress_fast_extState_fastReset(void* state, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration)
{
    LZ4_stream_t_internal* const ctx = &((LZ4_stream_t*)state)->internal_donotuse;
    if (acceleration < 1) acceleration = LZ4_ACCELERATION_DEFAULT;
    if (acceleration > LZ4_ACCELERATION_MAX) acceleration = LZ4_ACCELERATION_MAX;
    assert(ctx != NULL);

    if (dstCapacity >= LZ4_compressBound(srcSize)) {
        if (srcSize < LZ4_64Klimit) {
            const tableType_t tableType = byU16;
            LZ4_prepareTable(ctx, srcSize, tableType);
            if (ctx->currentOffset) {
                return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, 0, notLimited, tableType, noDict, dictSmall, acceleration);
            } else {
                return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, 0, notLimited, tableType, noDict, noDictIssue, acceleration);
            }
        } else {
            const tableType_t tableType = ((sizeof(void*)==4) && ((uptrval)src > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            LZ4_prepareTable(ctx, srcSize, tableType);
            return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, 0, notLimited, tableType, noDict, noDictIssue, acceleration);
        }
    } else {
        if (srcSize < LZ4_64Klimit) {
            const tableType_t tableType = byU16;
            LZ4_prepareTable(ctx, srcSize, tableType);
            if (ctx->currentOffset) {
                return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, dstCapacity, limitedOutput, tableType, noDict, dictSmall, acceleration);
            } else {
                return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, dstCapacity, limitedOutput, tableType, noDict, noDictIssue, acceleration);
            }
        } else {
            const tableType_t tableType = ((sizeof(void*)==4) && ((uptrval)src > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            LZ4_prepareTable(ctx, srcSize, tableType);
            return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, dstCapacity, limitedOutput, tableType, noDict, noDictIssue, acceleration);
        }
    }
}


int LZ4_compress_fast(const char* src, char* dest, int srcSize, int dstCapacity, int acceleration)
{
    int result;
#if (LZ4_HEAPMODE)
    LZ4_stream_t* const ctxPtr = (LZ4_stream_t*)ALLOC(sizeof(LZ4_stream_t));   /* malloc-calloc always properly aligned */
    if (ctxPtr == NULL) return 0;
#else
    LZ4_stream_t ctx;
    LZ4_stream_t* const ctxPtr = &ctx;
#endif
    result = LZ4_compress_fast_extState(ctxPtr, src, dest, srcSize, dstCapacity, acceleration);

#if (LZ4_HEAPMODE)
    FREEMEM(ctxPtr);
#endif
    return result;
}


int LZ4_compress_default(const char* src, char* dst, int srcSize, int dstCapacity)
{
    return LZ4_compress_fast(src, dst, srcSize, dstCapacity, 1);
}


/* Note!: This function leaves the stream in an unclean/broken state!
 * It is not safe to subsequently use the same state with a _fastReset() or
 * _continue() call without resetting it. */
static int LZ4_compress_destSize_extState_internal(LZ4_stream_t* state, const char* src, char* dst, int* srcSizePtr, int targetDstSize, int acceleration)
{
    void* const s = LZ4_initStream(state, sizeof (*state));
    assert(s != NULL); (void)s;

    if (targetDstSize >= LZ4_compressBound(*srcSizePtr)) {  /* compression success is guaranteed */
        return LZ4_compress_fast_extState(state, src, dst, *srcSizePtr, targetDstSize, acceleration);
    } else {
        if (*srcSizePtr < LZ4_64Klimit) {
            return LZ4_compress_generic(&state->internal_donotuse, src, dst, *srcSizePtr, srcSizePtr, targetDstSize, fillOutput, byU16, noDict, noDictIssue, acceleration);
        } else {
            tableType_t const addrMode = ((sizeof(void*)==4) && ((uptrval)src > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            return LZ4_compress_generic(&state->internal_donotuse, src, dst, *srcSizePtr, srcSizePtr, targetDstSize, fillOutput, addrMode, noDict, noDictIssue, acceleration);
    }   }
}

int LZ4_compress_destSize_extState(void* state, const char* src, char* dst, int* srcSizePtr, int targetDstSize, int acceleration)
{
    int const r = LZ4_compress_destSize_extState_internal((LZ4_stream_t*)state, src, dst, srcSizePtr, targetDstSize, acceleration);
    /* clean the state on exit */
    LZ4_initStream(state, sizeof (LZ4_stream_t));
    return r;
}


int LZ4_compress_destSize(const char* src, char* dst, int* srcSizePtr, int targetDstSize)
{
#if (LZ4_HEAPMODE)
    LZ4_stream_t* const ctx = (LZ4_stream_t*)ALLOC(sizeof(LZ4_stream_t));   /* malloc-calloc always properly aligned */
    if (ctx == NULL) return 0;
#else
    LZ4_stream_t ctxBody;
    LZ4_stream_t* const ctx = &ctxBody;
#endif

    int result = LZ4_compress_destSize_extState_internal(ctx, src, dst, srcSizePtr, targetDstSize, 1);

#if (LZ4_HEAPMODE)
    FREEMEM(ctx);
#endif
    return result;
}



/*-******************************
*  Streaming functions
********************************/

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
LZ4_stream_t* LZ4_createStream(void)
{
    LZ4_stream_t* const lz4s = (LZ4_stream_t*)ALLOC(sizeof(LZ4_stream_t));
    LZ4_STATIC_ASSERT(sizeof(LZ4_stream_t) >= sizeof(LZ4_stream_t_internal));
    DEBUGLOG(4, "LZ4_createStream %p", lz4s);
    if (lz4s == NULL) return NULL;
    LZ4_initStream(lz4s, sizeof(*lz4s));
    return lz4s;
}
#endif

static size_t LZ4_stream_t_alignment(void)
{
#if LZ4_ALIGN_TEST
    typedef struct { char c; LZ4_stream_t t; } t_a;
    return sizeof(t_a) - sizeof(LZ4_stream_t);
#else
    return 1;  /* effectively disabled */
#endif
}

LZ4_stream_t* LZ4_initStream (void* buffer, size_t size)
{
    DEBUGLOG(5, "LZ4_initStream");
    if (buffer == NULL) { return NULL; }
    if (size < sizeof(LZ4_stream_t)) { return NULL; }
    if (!LZ4_isAligned(buffer, LZ4_stream_t_alignment())) return NULL;
    MEM_INIT(buffer, 0, sizeof(LZ4_stream_t_internal));
    return (LZ4_stream_t*)buffer;
}

/* resetStream is now deprecated,
 * prefer initStream() which is more general */
void LZ4_resetStream (LZ4_stream_t* LZ4_stream)
{
    DEBUGLOG(5, "LZ4_resetStream (ctx:%p)", LZ4_stream);
    MEM_INIT(LZ4_stream, 0, sizeof(LZ4_stream_t_internal));
}

void LZ4_resetStream_fast(LZ4_stream_t* ctx) {
    LZ4_prepareTable(&(ctx->internal_donotuse), 0, byU32);
}

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
int LZ4_freeStream (LZ4_stream_t* LZ4_stream)
{
    if (!LZ4_stream) return 0;   /* support free on NULL */
    DEBUGLOG(5, "LZ4_freeStream %p", LZ4_stream);
    FREEMEM(LZ4_stream);
    return (0);
}
#endif


#define HASH_UNIT sizeof(reg_t)
int LZ4_loadDict (LZ4_stream_t* LZ4_dict, const char* dictionary, int dictSize)
{
    LZ4_stream_t_internal* const dict = &LZ4_dict->internal_donotuse;
    const tableType_t tableType = byU32;
    const BYTE* p = (const BYTE*)dictionary;
    const BYTE* const dictEnd = p + dictSize;
    U32 idx32;

    DEBUGLOG(4, "LZ4_loadDict (%i bytes from %p into %p)", dictSize, dictionary, LZ4_dict);

    /* It's necessary to reset the context,
     * and not just continue it with prepareTable()
     * to avoid any risk of generating overflowing matchIndex
     * when compressing using this dictionary */
    LZ4_resetStream(LZ4_dict);

    /* We always increment the offset by 64 KB, since, if the dict is longer,
     * we truncate it to the last 64k, and if it's shorter, we still want to
     * advance by a whole window length so we can provide the guarantee that
     * there are only valid offsets in the window, which allows an optimization
     * in LZ4_compress_fast_continue() where it uses noDictIssue even when the
     * dictionary isn't a full 64k. */
    dict->currentOffset += 64 KB;

    if (dictSize < (int)HASH_UNIT) {
        return 0;
    }

    if ((dictEnd - p) > 64 KB) p = dictEnd - 64 KB;
    dict->dictionary = p;
    dict->dictSize = (U32)(dictEnd - p);
    dict->tableType = (U32)tableType;
    idx32 = dict->currentOffset - dict->dictSize;

    while (p <= dictEnd-HASH_UNIT) {
        U32 const h = LZ4_hashPosition(p, tableType);
        LZ4_putIndexOnHash(idx32, h, dict->hashTable, tableType);
        p+=3; idx32+=3;
    }

    return (int)dict->dictSize;
}

void LZ4_attach_dictionary(LZ4_stream_t* workingStream, const LZ4_stream_t* dictionaryStream)
{
    const LZ4_stream_t_internal* dictCtx = (dictionaryStream == NULL) ? NULL :
        &(dictionaryStream->internal_donotuse);

    DEBUGLOG(4, "LZ4_attach_dictionary (%p, %p, size %u)",
             workingStream, dictionaryStream,
             dictCtx != NULL ? dictCtx->dictSize : 0);

    if (dictCtx != NULL) {
        /* If the current offset is zero, we will never look in the
         * external dictionary context, since there is no value a table
         * entry can take that indicate a miss. In that case, we need
         * to bump the offset to something non-zero.
         */
        if (workingStream->internal_donotuse.currentOffset == 0) {
            workingStream->internal_donotuse.currentOffset = 64 KB;
        }

        /* Don't actually attach an empty dictionary.
         */
        if (dictCtx->dictSize == 0) {
            dictCtx = NULL;
        }
    }
    workingStream->internal_donotuse.dictCtx = dictCtx;
}


static void LZ4_renormDictT(LZ4_stream_t_internal* LZ4_dict, int nextSize)
{
    assert(nextSize >= 0);
    if (LZ4_dict->currentOffset + (unsigned)nextSize > 0x80000000) {   /* potential ptrdiff_t overflow (32-bits mode) */
        /* rescale hash table */
        U32 const delta = LZ4_dict->currentOffset - 64 KB;
        const BYTE* dictEnd = LZ4_dict->dictionary + LZ4_dict->dictSize;
        int i;
        DEBUGLOG(4, "LZ4_renormDictT");
        for (i=0; i<LZ4_HASH_SIZE_U32; i++) {
            if (LZ4_dict->hashTable[i] < delta) LZ4_dict->hashTable[i]=0;
            else LZ4_dict->hashTable[i] -= delta;
        }
        LZ4_dict->currentOffset = 64 KB;
        if (LZ4_dict->dictSize > 64 KB) LZ4_dict->dictSize = 64 KB;
        LZ4_dict->dictionary = dictEnd - LZ4_dict->dictSize;
    }
}


int LZ4_compress_fast_continue (LZ4_stream_t* LZ4_stream,
                                const char* source, char* dest,
                                int inputSize, int maxOutputSize,
                                int acceleration)
{
    const tableType_t tableType = byU32;
    LZ4_stream_t_internal* const streamPtr = &LZ4_stream->internal_donotuse;
    const char* dictEnd = streamPtr->dictSize ? (const char*)streamPtr->dictionary + streamPtr->dictSize : NULL;

    DEBUGLOG(5, "LZ4_compress_fast_continue (inputSize=%i, dictSize=%u)", inputSize, streamPtr->dictSize);

    LZ4_renormDictT(streamPtr, inputSize);   /* fix index overflow */
    if (acceleration < 1) acceleration = LZ4_ACCELERATION_DEFAULT;
    if (acceleration > LZ4_ACCELERATION_MAX) acceleration = LZ4_ACCELERATION_MAX;

    /* invalidate tiny dictionaries */
    if ( (streamPtr->dictSize < 4)     /* tiny dictionary : not enough for a hash */
      && (dictEnd != source)           /* prefix mode */
      && (inputSize > 0)               /* tolerance : don't lose history, in case next invocation would use prefix mode */
      && (streamPtr->dictCtx == NULL)  /* usingDictCtx */
      ) {
        DEBUGLOG(5, "LZ4_compress_fast_continue: dictSize(%u) at addr:%p is too small", streamPtr->dictSize, streamPtr->dictionary);
        /* remove dictionary existence from history, to employ faster prefix mode */
        streamPtr->dictSize = 0;
        streamPtr->dictionary = (const BYTE*)source;
        dictEnd = source;
    }

    /* Check overlapping input/dictionary space */
    {   const char* const sourceEnd = source + inputSize;
        if ((sourceEnd > (const char*)streamPtr->dictionary) && (sourceEnd < dictEnd)) {
            streamPtr->dictSize = (U32)(dictEnd - sourceEnd);
            if (streamPtr->dictSize > 64 KB) streamPtr->dictSize = 64 KB;
            if (streamPtr->dictSize < 4) streamPtr->dictSize = 0;
            streamPtr->dictionary = (const BYTE*)dictEnd - streamPtr->dictSize;
        }
    }

    /* prefix mode : source data follows dictionary */
    if (dictEnd == source) {
        if ((streamPtr->dictSize < 64 KB) && (streamPtr->dictSize < streamPtr->currentOffset))
            return LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, withPrefix64k, dictSmall, acceleration);
        else
            return LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, withPrefix64k, noDictIssue, acceleration);
    }

    /* external dictionary mode */
    {   int result;
        if (streamPtr->dictCtx) {
            /* We depend here on the fact that dictCtx'es (produced by
             * LZ4_loadDict) guarantee that their tables contain no references
             * to offsets between dictCtx->currentOffset - 64 KB and
             * dictCtx->currentOffset - dictCtx->dictSize. This makes it safe
             * to use noDictIssue even when the dict isn't a full 64 KB.
             */
            if (inputSize > 4 KB) {
                /* For compressing large blobs, it is faster to pay the setup
                 * cost to copy the dictionary's tables into the active context,
                 * so that the compression loop is only looking into one table.
                 */
                LZ4_memcpy(streamPtr, streamPtr->dictCtx, sizeof(*streamPtr));
                result = LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, usingExtDict, noDictIssue, acceleration);
            } else {
                result = LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, usingDictCtx, noDictIssue, acceleration);
            }
        } else {  /* small data <= 4 KB */
            if ((streamPtr->dictSize < 64 KB) && (streamPtr->dictSize < streamPtr->currentOffset)) {
                result = LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, usingExtDict, dictSmall, acceleration);
            } else {
                result = LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, usingExtDict, noDictIssue, acceleration);
            }
        }
        streamPtr->dictionary = (const BYTE*)source;
        streamPtr->dictSize = (U32)inputSize;
        return result;
    }
}


/* Hidden debug function, to force-test external dictionary mode */
int LZ4_compress_forceExtDict (LZ4_stream_t* LZ4_dict, const char* source, char* dest, int srcSize)
{
    LZ4_stream_t_internal* const streamPtr = &LZ4_dict->internal_donotuse;
    int result;

    LZ4_renormDictT(streamPtr, srcSize);

    if ((streamPtr->dictSize < 64 KB) && (streamPtr->dictSize < streamPtr->currentOffset)) {
        result = LZ4_compress_generic(streamPtr, source, dest, srcSize, NULL, 0, notLimited, byU32, usingExtDict, dictSmall, 1);
    } else {
        result = LZ4_compress_generic(streamPtr, source, dest, srcSize, NULL, 0, notLimited, byU32, usingExtDict, noDictIssue, 1);
    }

    streamPtr->dictionary = (const BYTE*)source;
    streamPtr->dictSize = (U32)srcSize;

    return result;
}


/*! LZ4_saveDict() :
 *  If previously compressed data block is not guaranteed to remain available at its memory location,
 *  save it into a safer place (char* safeBuffer).
 *  Note : no need to call LZ4_loadDict() afterwards, dictionary is immediately usable,
 *         one can therefore call LZ4_compress_fast_continue() right after.
 * @return : saved dictionary size in bytes (necessarily <= dictSize), or 0 if error.
 */
int LZ4_saveDict (LZ4_stream_t* LZ4_dict, char* safeBuffer, int dictSize)
{
    LZ4_stream_t_internal* const dict = &LZ4_dict->internal_donotuse;

    DEBUGLOG(5, "LZ4_saveDict : dictSize=%i, safeBuffer=%p", dictSize, safeBuffer);

    if ((U32)dictSize > 64 KB) { dictSize = 64 KB; } /* useless to define a dictionary > 64 KB */
    if ((U32)dictSize > dict->dictSize) { dictSize = (int)dict->dictSize; }

    if (safeBuffer == NULL) assert(dictSize == 0);
    if (dictSize > 0) {
        const BYTE* const previousDictEnd = dict->dictionary + dict->dictSize;
        assert(dict->dictionary);
        LZ4_memmove(safeBuffer, previousDictEnd - dictSize, (size_t)dictSize);
    }

    dict->dictionary = (const BYTE*)safeBuffer;
    dict->dictSize = (U32)dictSize;

    return dictSize;
}



/*-*******************************
 *  Decompression functions
 ********************************/

typedef enum { decode_full_block = 0, partial_decode = 1 } earlyEnd_directive;

#undef MIN
#define MIN(a,b)    ( (a) < (b) ? (a) : (b) )


/* variant for decompress_unsafe()
 * does not know end of input
 * presumes input is well formed
 * note : will consume at least one byte */
static size_t read_long_length_no_check(const BYTE** pp)
{
    size_t b, l = 0;
    do { b = **pp; (*pp)++; l += b; } while (b==255);
    DEBUGLOG(6, "read_long_length_no_check: +length=%zu using %zu input bytes", l, l/255 + 1)
    return l;
}

/* core decoder variant for LZ4_decompress_fast*()
 * for legacy support only : these entry points are deprecated.
 * - Presumes input is correctly formed (no defense vs malformed inputs)
 * - Does not know input size (presume input buffer is "large enough")
 * - Decompress a full block (only)
 * @return : nb of bytes read from input.
 * Note : this variant is not optimized for speed, just for maintenance.
 *        the goal is to remove support of decompress_fast*() variants by v2.0
**/
LZ4_FORCE_INLINE int
LZ4_decompress_unsafe_generic(
                 const BYTE* const istart,
                 BYTE* const ostart,
                 int decompressedSize,

                 size_t prefixSize,
                 const BYTE* const dictStart,  /* only if dict==usingExtDict */
                 const size_t dictSize         /* note: =0 if dictStart==NULL */
                 )
{
    const BYTE* ip = istart;
    BYTE* op = (BYTE*)ostart;
    BYTE* const oend = ostart + decompressedSize;
    const BYTE* const prefixStart = ostart - prefixSize;

    DEBUGLOG(5, "LZ4_decompress_unsafe_generic");
    if (dictStart == NULL) assert(dictSize == 0);

    while (1) {
        /* start new sequence */
        unsigned token = *ip++;

        /* literals */
        {   size_t ll = token >> ML_BITS;
            if (ll==15) {
                /* long literal length */
                ll += read_long_length_no_check(&ip);
            }
            if ((size_t)(oend-op) < ll) return -1; /* output buffer overflow */
            LZ4_memmove(op, ip, ll); /* support in-place decompression */
            op += ll;
            ip += ll;
            if ((size_t)(oend-op) < MFLIMIT) {
                if (op==oend) break;  /* end of block */
                DEBUGLOG(5, "invalid: literals end at distance %zi from end of block", oend-op);
                /* incorrect end of block :
                 * last match must start at least MFLIMIT==12 bytes before end of output block */
                return -1;
        }   }

        /* match */
        {   size_t ml = token & 15;
            size_t const offset = LZ4_readLE16(ip);
            ip+=2;

            if (ml==15) {
                /* long literal length */
                ml += read_long_length_no_check(&ip);
            }
            ml += MINMATCH;

            if ((size_t)(oend-op) < ml) return -1; /* output buffer overflow */

            {   const BYTE* match = op - offset;

                /* out of range */
                if (offset > (size_t)(op - prefixStart) + dictSize) {
                    DEBUGLOG(6, "offset out of range");
                    return -1;
                }

                /* check special case : extDict */
                if (offset > (size_t)(op - prefixStart)) {
                    /* extDict scenario */
                    const BYTE* const dictEnd = dictStart + dictSize;
                    const BYTE* extMatch = dictEnd - (offset - (size_t)(op-prefixStart));
                    size_t const extml = (size_t)(dictEnd - extMatch);
                    if (extml > ml) {
                        /* match entirely within extDict */
                        LZ4_memmove(op, extMatch, ml);
                        op += ml;
                        ml = 0;
                    } else {
                        /* match split between extDict & prefix */
                        LZ4_memmove(op, extMatch, extml);
                        op += extml;
                        ml -= extml;
                    }
                    match = prefixStart;
                }

                /* match copy - slow variant, supporting overlap copy */
                {   size_t u;
                    for (u=0; u<ml; u++) {
                        op[u] = match[u];
            }   }   }
            op += ml;
            if ((size_t)(oend-op) < LASTLITERALS) {
                DEBUGLOG(5, "invalid: match ends at distance %zi from end of block", oend-op);
                /* incorrect end of block :
                 * last match must stop at least LASTLITERALS==5 bytes before end of output block */
                return -1;
            }
        } /* match */
    } /* main loop */
    return (int)(ip - istart);
}


/* Read the variable-length literal or match length.
 *
 * @ip : input pointer
 * @ilimit : position after which if length is not decoded, the input is necessarily corrupted.
 * @initial_check - check ip >= ipmax before start of loop.  Returns initial_error if so.
 * @error (output) - error code.  Must be set to 0 before call.
**/
typedef size_t Rvl_t;
static const Rvl_t rvl_error = (Rvl_t)(-1);
LZ4_FORCE_INLINE Rvl_t
read_variable_length(const BYTE** ip, const BYTE* ilimit,
                     int initial_check)
{
    Rvl_t s, length = 0;
    assert(ip != NULL);
    assert(*ip !=  NULL);
    assert(ilimit != NULL);
    if (initial_check && unlikely((*ip) >= ilimit)) {    /* read limit reached */
        return rvl_error;
    }
    s = **ip;
    (*ip)++;
    length += s;
    if (unlikely((*ip) > ilimit)) {    /* read limit reached */
        return rvl_error;
    }
    /* accumulator overflow detection (32-bit mode only) */
    if ((sizeof(length) < 8) && unlikely(length > ((Rvl_t)(-1)/2)) ) {
        return rvl_error;
    }
    if (likely(s != 255)) return length;
    do {
        s = **ip;
        (*ip)++;
        length += s;
        if (unlikely((*ip) > ilimit)) {    /* read limit reached */
            return rvl_error;
        }
        /* accumulator overflow detection (32-bit mode only) */
        if ((sizeof(length) < 8) && unlikely(length > ((Rvl_t)(-1)/2)) ) {
            return rvl_error;
        }
    } while (s == 255);

    return length;
}

/*! LZ4_decompress_generic() :
 *  This generic decompression function covers all use cases.
 *  It shall be instantiated several times, using different sets of directives.
 *  Note that it is important for performance that this function really get inlined,
 *  in order to remove useless branches during compilation optimization.
 */
LZ4_FORCE_INLINE int
LZ4_decompress_generic(
                 const char* const src,
                 char* const dst,
                 int srcSize,
                 int outputSize,         /* If endOnInput==endOnInputSize, this value is `dstCapacity` */

                 earlyEnd_directive partialDecoding,  /* full, partial */
                 dict_directive dict,                 /* noDict, withPrefix64k, usingExtDict */
                 const BYTE* const lowPrefix,  /* always <= dst, == dst when no prefix */
                 const BYTE* const dictStart,  /* only if dict==usingExtDict */
                 const size_t dictSize         /* note : = 0 if noDict */
                 )
{
    if ((src == NULL) || (outputSize < 0)) { return -1; }

    {   const BYTE* ip = (const BYTE*) src;
        const BYTE* const iend = ip + srcSize;

        BYTE* op = (BYTE*) dst;
        BYTE* const oend = op + outputSize;
        BYTE* cpy;

        const BYTE* const dictEnd = (dictStart == NULL) ? NULL : dictStart + dictSize;

        const int checkOffset = (dictSize < (int)(64 KB));


        /* Set up the "end" pointers for the shortcut. */
        const BYTE* const shortiend = iend - 14 /*maxLL*/ - 2 /*offset*/;
        const BYTE* const shortoend = oend - 14 /*maxLL*/ - 18 /*maxML*/;

        const BYTE* match;
        size_t offset;
        unsigned token;
        size_t length;


        DEBUGLOG(5, "LZ4_decompress_generic (srcSize:%i, dstSize:%i)", srcSize, outputSize);

        /* Special cases */
        assert(lowPrefix <= op);
        if (unlikely(outputSize==0)) {
            /* Empty output buffer */
            if (partialDecoding) return 0;
            return ((srcSize==1) && (*ip==0)) ? 0 : -1;
        }
        if (unlikely(srcSize==0)) { return -1; }

    /* LZ4_FAST_DEC_LOOP:
     * designed for modern OoO performance cpus,
     * where copying reliably 32-bytes is preferable to an unpredictable branch.
     * note : fast loop may show a regression for some client arm chips. */
#if LZ4_FAST_DEC_LOOP
        if ((oend - op) < FASTLOOP_SAFE_DISTANCE) {
            DEBUGLOG(6, "skip fast decode loop");
            goto safe_decode;
        }

        /* Fast loop : decode sequences as long as output < oend-FASTLOOP_SAFE_DISTANCE */
        DEBUGLOG(6, "using fast decode loop");
        while (1) {
            /* Main fastloop assertion: We can always wildcopy FASTLOOP_SAFE_DISTANCE */
            assert(oend - op >= FASTLOOP_SAFE_DISTANCE);
            assert(ip < iend);
            token = *ip++;
            length = token >> ML_BITS;  /* literal length */

            /* decode literal length */
            if (length == RUN_MASK) {
                size_t const addl = read_variable_length(&ip, iend-RUN_MASK, 1);
                if (addl == rvl_error) {
                    DEBUGLOG(6, "error reading long literal length");
                    goto _output_error;
                }
                length += addl;
                if (unlikely((uptrval)(op)+length<(uptrval)(op))) { goto _output_error; } /* overflow detection */
                if (unlikely((uptrval)(ip)+length<(uptrval)(ip))) { goto _output_error; } /* overflow detection */

                /* copy literals */
                LZ4_STATIC_ASSERT(MFLIMIT >= WILDCOPYLENGTH);
                if ((op+length>oend-32) || (ip+length>iend-32)) { goto safe_literal_copy; }
                LZ4_wildCopy32(op, ip, op+length);
                ip += length; op += length;
            } else if (ip <= iend-(16 + 1/*max lit + offset + nextToken*/)) {
                /* We don't need to check oend, since we check it once for each loop below */
                DEBUGLOG(7, "copy %u bytes in a 16-bytes stripe", (unsigned)length);
                /* Literals can only be <= 14, but hope compilers optimize better when copy by a register size */
                LZ4_memcpy(op, ip, 16);
                ip += length; op += length;
            } else {
                goto safe_literal_copy;
            }

            /* get offset */
            offset = LZ4_readLE16(ip); ip+=2;
            DEBUGLOG(6, " offset = %zu", offset);
            match = op - offset;
            assert(match <= op);  /* overflow check */

            /* get matchlength */
            length = token & ML_MASK;

            if (length == ML_MASK) {
                size_t const addl = read_variable_length(&ip, iend - LASTLITERALS + 1, 0);
                if (addl == rvl_error) {
                    DEBUGLOG(6, "error reading long match length");
                    goto _output_error;
                }
                length += addl;
                length += MINMATCH;
                if (unlikely((uptrval)(op)+length<(uptrval)op)) { goto _output_error; } /* overflow detection */
                if (op + length >= oend - FASTLOOP_SAFE_DISTANCE) {
                    goto safe_match_copy;
                }
            } else {
                length += MINMATCH;
                if (op + length >= oend - FASTLOOP_SAFE_DISTANCE) {
                    goto safe_match_copy;
                }

                /* Fastpath check: skip LZ4_wildCopy32 when true */
                if ((dict == withPrefix64k) || (match >= lowPrefix)) {
                    if (offset >= 8) {
                        assert(match >= lowPrefix);
                        assert(match <= op);
                        assert(op + 18 <= oend);

                        LZ4_memcpy(op, match, 8);
                        LZ4_memcpy(op+8, match+8, 8);
                        LZ4_memcpy(op+16, match+16, 2);
                        op += length;
                        continue;
            }   }   }

            if ( checkOffset && (unlikely(match + dictSize < lowPrefix)) ) {
                DEBUGLOG(6, "Error : pos=%zi, offset=%zi => outside buffers", op-lowPrefix, op-match);
                goto _output_error;
            }
            /* match starting within external dictionary */
            if ((dict==usingExtDict) && (match < lowPrefix)) {
                assert(dictEnd != NULL);
                if (unlikely(op+length > oend-LASTLITERALS)) {
                    if (partialDecoding) {
                        DEBUGLOG(7, "partialDecoding: dictionary match, close to dstEnd");
                        length = MIN(length, (size_t)(oend-op));
                    } else {
                        DEBUGLOG(6, "end-of-block condition violated")
                        goto _output_error;
                }   }

                if (length <= (size_t)(lowPrefix-match)) {
                    /* match fits entirely within external dictionary : just copy */
                    LZ4_memmove(op, dictEnd - (lowPrefix-match), length);
                    op += length;
                } else {
                    /* match stretches into both external dictionary and current block */
                    size_t const copySize = (size_t)(lowPrefix - match);
                    size_t const restSize = length - copySize;
                    LZ4_memcpy(op, dictEnd - copySize, copySize);
                    op += copySize;
                    if (restSize > (size_t)(op - lowPrefix)) {  /* overlap copy */
                        BYTE* const endOfMatch = op + restSize;
                        const BYTE* copyFrom = lowPrefix;
                        while (op < endOfMatch) { *op++ = *copyFrom++; }
                    } else {
                        LZ4_memcpy(op, lowPrefix, restSize);
                        op += restSize;
                }   }
                continue;
            }

            /* copy match within block */
            cpy = op + length;

            assert((op <= oend) && (oend-op >= 32));
            if (unlikely(offset<16)) {
                LZ4_memcpy_using_offset(op, match, cpy, offset);
            } else {
                LZ4_wildCopy32(op, match, cpy);
            }

            op = cpy;   /* wildcopy correction */
        }
    safe_decode:
#endif

        /* Main Loop : decode remaining sequences where output < FASTLOOP_SAFE_DISTANCE */
        DEBUGLOG(6, "using safe decode loop");
        while (1) {
            assert(ip < iend);
            token = *ip++;
            length = token >> ML_BITS;  /* literal length */

            /* A two-stage shortcut for the most common case:
             * 1) If the literal length is 0..14, and there is enough space,
             * enter the shortcut and copy 16 bytes on behalf of the literals
             * (in the fast mode, only 8 bytes can be safely copied this way).
             * 2) Further if the match length is 4..18, copy 18 bytes in a similar
             * manner; but we ensure that there's enough space in the output for
             * those 18 bytes earlier, upon entering the shortcut (in other words,
             * there is a combined check for both stages).
             */
            if ( (length != RUN_MASK)
                /* strictly "less than" on input, to re-enter the loop with at least one byte */
              && likely((ip < shortiend) & (op <= shortoend)) ) {
                /* Copy the literals */
                LZ4_memcpy(op, ip, 16);
                op += length; ip += length;

                /* The second stage: prepare for match copying, decode full info.
                 * If it doesn't work out, the info won't be wasted. */
                length = token & ML_MASK; /* match length */
                offset = LZ4_readLE16(ip); ip += 2;
                match = op - offset;
                assert(match <= op); /* check overflow */

                /* Do not deal with overlapping matches. */
                if ( (length != ML_MASK)
                  && (offset >= 8)
                  && (dict==withPrefix64k || match >= lowPrefix) ) {
                    /* Copy the match. */
                    LZ4_memcpy(op + 0, match + 0, 8);
                    LZ4_memcpy(op + 8, match + 8, 8);
                    LZ4_memcpy(op +16, match +16, 2);
                    op += length + MINMATCH;
                    /* Both stages worked, load the next token. */
                    continue;
                }

                /* The second stage didn't work out, but the info is ready.
                 * Propel it right to the point of match copying. */
                goto _copy_match;
            }

            /* decode literal length */
            if (length == RUN_MASK) {
                size_t const addl = read_variable_length(&ip, iend-RUN_MASK, 1);
                if (addl == rvl_error) { goto _output_error; }
                length += addl;
                if (unlikely((uptrval)(op)+length<(uptrval)(op))) { goto _output_error; } /* overflow detection */
                if (unlikely((uptrval)(ip)+length<(uptrval)(ip))) { goto _output_error; } /* overflow detection */
            }

#if LZ4_FAST_DEC_LOOP
        safe_literal_copy:
#endif
            /* copy literals */
            cpy = op+length;

            LZ4_STATIC_ASSERT(MFLIMIT >= WILDCOPYLENGTH);
            if ((cpy>oend-MFLIMIT) || (ip+length>iend-(2+1+LASTLITERALS))) {
                /* We've either hit the input parsing restriction or the output parsing restriction.
                 * In the normal scenario, decoding a full block, it must be the last sequence,
                 * otherwise it's an error (invalid input or dimensions).
                 * In partialDecoding scenario, it's necessary to ensure there is no buffer overflow.
                 */
                if (partialDecoding) {
                    /* Since we are partial decoding we may be in this block because of the output parsing
                     * restriction, which is not valid since the output buffer is allowed to be undersized.
                     */
                    DEBUGLOG(7, "partialDecoding: copying literals, close to input or output end")
                    DEBUGLOG(7, "partialDecoding: literal length = %u", (unsigned)length);
                    DEBUGLOG(7, "partialDecoding: remaining space in dstBuffer : %i", (int)(oend - op));
                    DEBUGLOG(7, "partialDecoding: remaining space in srcBuffer : %i", (int)(iend - ip));
                    /* Finishing in the middle of a literals segment,
                     * due to lack of input.
                     */
                    if (ip+length > iend) {
                        length = (size_t)(iend-ip);
                        cpy = op + length;
                    }
                    /* Finishing in the middle of a literals segment,
                     * due to lack of output space.
                     */
                    if (cpy > oend) {
                        cpy = oend;
                        assert(op<=oend);
                        length = (size_t)(oend-op);
                    }
                } else {
                     /* We must be on the last sequence (or invalid) because of the parsing limitations
                      * so check that we exactly consume the input and don't overrun the output buffer.
                      */
                    if ((ip+length != iend) || (cpy > oend)) {
                        DEBUGLOG(6, "should have been last run of literals")
                        DEBUGLOG(6, "ip(%p) + length(%i) = %p != iend (%p)", ip, (int)length, ip+length, iend);
                        DEBUGLOG(6, "or cpy(%p) > oend(%p)", cpy, oend);
                        goto _output_error;
                    }
                }
                LZ4_memmove(op, ip, length);  /* supports overlapping memory regions, for in-place decompression scenarios */
                ip += length;
                op += length;
                /* Necessarily EOF when !partialDecoding.
                 * When partialDecoding, it is EOF if we've either
                 * filled the output buffer or
                 * can't proceed with reading an offset for following match.
                 */
                if (!partialDecoding || (cpy == oend) || (ip >= (iend-2))) {
                    break;
                }
            } else {
                LZ4_wildCopy8(op, ip, cpy);   /* can overwrite up to 8 bytes beyond cpy */
                ip += length; op = cpy;
            }

            /* get offset */
            offset = LZ4_readLE16(ip); ip+=2;
            match = op - offset;

            /* get matchlength */
            length = token & ML_MASK;

    _copy_match:
            if (length == ML_MASK) {
                size_t const addl = read_variable_length(&ip, iend - LASTLITERALS + 1, 0);
                if (addl == rvl_error) { goto _output_error; }
                length += addl;
                if (unlikely((uptrval)(op)+length<(uptrval)op)) goto _output_error;   /* overflow detection */
            }
            length += MINMATCH;

#if LZ4_FAST_DEC_LOOP
        safe_match_copy:
#endif
            if ((checkOffset) && (unlikely(match + dictSize < lowPrefix))) goto _output_error;   /* Error : offset outside buffers */
            /* match starting within external dictionary */
            if ((dict==usingExtDict) && (match < lowPrefix)) {
                assert(dictEnd != NULL);
                if (unlikely(op+length > oend-LASTLITERALS)) {
                    if (partialDecoding) length = MIN(length, (size_t)(oend-op));
                    else goto _output_error;   /* doesn't respect parsing restriction */
                }

                if (length <= (size_t)(lowPrefix-match)) {
                    /* match fits entirely within external dictionary : just copy */
                    LZ4_memmove(op, dictEnd - (lowPrefix-match), length);
                    op += length;
                } else {
                    /* match stretches into both external dictionary and current block */
                    size_t const copySize = (size_t)(lowPrefix - match);
                    size_t const restSize = length - copySize;
                    LZ4_memcpy(op, dictEnd - copySize, copySize);
                    op += copySize;
                    if (restSize > (size_t)(op - lowPrefix)) {  /* overlap copy */
                        BYTE* const endOfMatch = op + restSize;
                        const BYTE* copyFrom = lowPrefix;
                        while (op < endOfMatch) *op++ = *copyFrom++;
                    } else {
                        LZ4_memcpy(op, lowPrefix, restSize);
                        op += restSize;
                }   }
                continue;
            }
            assert(match >= lowPrefix);

            /* copy match within block */
            cpy = op + length;

            /* partialDecoding : may end anywhere within the block */
            assert(op<=oend);
            if (partialDecoding && (cpy > oend-MATCH_SAFEGUARD_DISTANCE)) {
                size_t const mlen = MIN(length, (size_t)(oend-op));
                const BYTE* const matchEnd = match + mlen;
                BYTE* const copyEnd = op + mlen;
                if (matchEnd > op) {   /* overlap copy */
                    while (op < copyEnd) { *op++ = *match++; }
                } else {
                    LZ4_memcpy(op, match, mlen);
                }
                op = copyEnd;
                if (op == oend) { break; }
                continue;
            }

            if (unlikely(offset<8)) {
                LZ4_write32(op, 0);   /* silence msan warning when offset==0 */
                op[0] = match[0];
                op[1] = match[1];
                op[2] = match[2];
                op[3] = match[3];
                match += inc32table[offset];
                LZ4_memcpy(op+4, match, 4);
                match -= dec64table[offset];
            } else {
                LZ4_memcpy(op, match, 8);
                match += 8;
            }
            op += 8;

            if (unlikely(cpy > oend-MATCH_SAFEGUARD_DISTANCE)) {
                BYTE* const oCopyLimit = oend - (WILDCOPYLENGTH-1);
                if (cpy > oend-LASTLITERALS) { goto _output_error; } /* Error : last LASTLITERALS bytes must be literals (uncompressed) */
                if (op < oCopyLimit) {
                    LZ4_wildCopy8(op, match, oCopyLimit);
                    match += oCopyLimit - op;
                    op = oCopyLimit;
                }
                while (op < cpy) { *op++ = *match++; }
            } else {
                LZ4_memcpy(op, match, 8);
                if (length > 16)  { LZ4_wildCopy8(op+8, match+8, cpy); }
            }
            op = cpy;   /* wildcopy correction */
        }

        /* end of decoding */
        DEBUGLOG(5, "decoded %i bytes", (int) (((char*)op)-dst));
        return (int) (((char*)op)-dst);     /* Nb of output bytes decoded */

        /* Overflow error detected */
    _output_error:
        return (int) (-(((const char*)ip)-src))-1;
    }
}


/*===== Instantiate the API decoding functions. =====*/

LZ4_FORCE_O2
int LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize)
{
    return LZ4_decompress_generic(source, dest, compressedSize, maxDecompressedSize,
                                  decode_full_block, noDict,
                                  (BYTE*)dest, NULL, 0);
}

LZ4_FORCE_O2
int LZ4_decompress_safe_partial(const char* src, char* dst, int compressedSize, int targetOutputSize, int dstCapacity)
{
    dstCapacity = MIN(targetOutputSize, dstCapacity);
    return LZ4_decompress_generic(src, dst, compressedSize, dstCapacity,
                                  partial_decode,
                                  noDict, (BYTE*)dst, NULL, 0);
}

LZ4_FORCE_O2
int LZ4_decompress_fast(const char* source, char* dest, int originalSize)
{
    DEBUGLOG(5, "LZ4_decompress_fast");
    return LZ4_decompress_unsafe_generic(
                (const BYTE*)source, (BYTE*)dest, originalSize,
                0, NULL, 0);
}

/*===== Instantiate a few more decoding cases, used more than once. =====*/

/* forward declarations for helper used in legacy wrappers */
static int LZ4_decompress_safe_partial_withPrefix64k(const char* source, char* dest, int compressedSize, int targetOutputSize, int dstCapacity);
static int LZ4_decompress_fast_extDict(const char* source, char* dest, int originalSize,
                                       const void* dictStart, size_t dictSize);

LZ4_FORCE_O2 /* Exported legacy compatibility API. */
int LZ4_decompress_safe_withPrefix64k(const char* source, char* dest, int compressedSize, int maxOutputSize)
{
    /* Use the recommended dict-based entry point instead of the deprecated prefix64k path. */
    return LZ4_decompress_safe_usingDict(source, dest, compressedSize, maxOutputSize,
                                         dest - (64 KB), 64 KB);
}

LZ4_FORCE_O2
static int LZ4_decompress_safe_partial_withPrefix64k(const char* source, char* dest, int compressedSize, int targetOutputSize, int dstCapacity)
{
    dstCapacity = MIN(targetOutputSize, dstCapacity);
    return LZ4_decompress_safe_partial_usingDict(source, dest, compressedSize, targetOutputSize, dstCapacity,
                                                 dest - (64 KB), 64 KB);
}

/* Legacy fast API implemented on top of usingDict */
int LZ4_decompress_fast_withPrefix64k(const char* source, char* dest, int originalSize)
{
    return LZ4_decompress_fast_extDict(source, dest, originalSize,
                                       dest - (64 KB), 64 KB);
}

LZ4_FORCE_O2
static int LZ4_decompress_safe_withSmallPrefix(const char* source, char* dest, int compressedSize, int maxOutputSize,
                                               size_t prefixSize)
{
    return LZ4_decompress_generic(source, dest, compressedSize, maxOutputSize,
                                  decode_full_block, noDict,
                                  (BYTE*)dest-prefixSize, NULL, 0);
}

LZ4_FORCE_O2
static int LZ4_decompress_safe_partial_withSmallPrefix(const char* source, char* dest, int compressedSize, int targetOutputSize, int dstCapacity,
                                               size_t prefixSize)
{
    dstCapacity = MIN(targetOutputSize, dstCapacity);
    return LZ4_decompress_generic(source, dest, compressedSize, dstCapacity,
                                  partial_decode, noDict,
                                  (BYTE*)dest-prefixSize, NULL, 0);
}

LZ4_FORCE_O2
int LZ4_decompress_safe_forceExtDict(const char* source, char* dest,
                                     int compressedSize, int maxOutputSize,
                                     const void* dictStart, size_t dictSize)
{
    DEBUGLOG(5, "LZ4_decompress_safe_forceExtDict");
    return LZ4_decompress_generic(source, dest, compressedSize, maxOutputSize,
                                  decode_full_block, usingExtDict,
                                  (BYTE*)dest, (const BYTE*)dictStart, dictSize);
}

LZ4_FORCE_O2
int LZ4_decompress_safe_partial_forceExtDict(const char* source, char* dest,
                                     int compressedSize, int targetOutputSize, int dstCapacity,
                                     const void* dictStart, size_t dictSize)
{
    dstCapacity = MIN(targetOutputSize, dstCapacity);
    return LZ4_decompress_generic(source, dest, compressedSize, dstCapacity,
                                  partial_decode, usingExtDict,
                                  (BYTE*)dest, (const BYTE*)dictStart, dictSize);
}

LZ4_FORCE_O2
static int LZ4_decompress_fast_extDict(const char* source, char* dest, int originalSize,
                                       const void* dictStart, size_t dictSize)
{
    return LZ4_decompress_unsafe_generic(
                (const BYTE*)source, (BYTE*)dest, originalSize,
                0, (const BYTE*)dictStart, dictSize);
}

/* The "double dictionary" mode, for use with e.g. ring buffers: the first part
 * of the dictionary is passed as prefix, and the second via dictStart + dictSize.
 * These routines are used only once, in LZ4_decompress_*_continue().
 */
LZ4_FORCE_INLINE
int LZ4_decompress_safe_doubleDict(const char* source, char* dest, int compressedSize, int maxOutputSize,
                                   size_t prefixSize, const void* dictStart, size_t dictSize)
{
    return LZ4_decompress_generic(source, dest, compressedSize, maxOutputSize,
                                  decode_full_block, usingExtDict,
                                  (BYTE*)dest-prefixSize, (const BYTE*)dictStart, dictSize);
}

/*===== streaming decompression functions =====*/

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
LZ4_streamDecode_t* LZ4_createStreamDecode(void)
{
    LZ4_STATIC_ASSERT(sizeof(LZ4_streamDecode_t) >= sizeof(LZ4_streamDecode_t_internal));
    return (LZ4_streamDecode_t*) ALLOC_AND_ZERO(sizeof(LZ4_streamDecode_t));
}

int LZ4_freeStreamDecode (LZ4_streamDecode_t* LZ4_stream)
{
    if (LZ4_stream == NULL) { return 0; }  /* support free on NULL */
    FREEMEM(LZ4_stream);
    return 0;
}
#endif

/*! LZ4_setStreamDecode() :
 *  Use this function to instruct where to find the dictionary.
 *  This function is not necessary if previous data is still available where it was decoded.
 *  Loading a size of 0 is allowed (same effect as no dictionary).
 * @return : 1 if OK, 0 if error
 */
int LZ4_setStreamDecode (LZ4_streamDecode_t* LZ4_streamDecode, const char* dictionary, int dictSize)
{
    LZ4_streamDecode_t_internal* lz4sd = &LZ4_streamDecode->internal_donotuse;
    lz4sd->prefixSize = (size_t)dictSize;
    if (dictSize) {
        assert(dictionary != NULL);
        lz4sd->prefixEnd = (const BYTE*) dictionary + dictSize;
    } else {
        lz4sd->prefixEnd = (const BYTE*) dictionary;
    }
    lz4sd->externalDict = NULL;
    lz4sd->extDictSize  = 0;
    return 1;
}

/*! LZ4_decoderRingBufferSize() :
 *  when setting a ring buffer for streaming decompression (optional scenario),
 *  provides the minimum size of this ring buffer
 *  to be compatible with any source respecting maxBlockSize condition.
 *  Note : in a ring buffer scenario,
 *  blocks are presumed decompressed next to each other.
 *  When not enough space remains for next block (remainingSize < maxBlockSize),
 *  decoding resumes from beginning of ring buffer.
 * @return : minimum ring buffer size,
 *           or 0 if there is an error (invalid maxBlockSize).
 */
int LZ4_decoderRingBufferSize(int maxBlockSize)
{
    if (maxBlockSize < 0) return 0;
    if (maxBlockSize > LZ4_MAX_INPUT_SIZE) return 0;
    if (maxBlockSize < 16) maxBlockSize = 16;
    return LZ4_DECODER_RING_BUFFER_SIZE(maxBlockSize);
}

/*
*_continue() :
    These decoding functions allow decompression of multiple blocks in "streaming" mode.
    Previously decoded blocks must still be available at the memory position where they were decoded.
    If it's not possible, save the relevant part of decoded data into a safe buffer,
    and indicate where it stands using LZ4_setStreamDecode()
*/
LZ4_FORCE_O2
int LZ4_decompress_safe_continue (LZ4_streamDecode_t* LZ4_streamDecode, const char* source, char* dest, int compressedSize, int maxOutputSize)
{
    LZ4_streamDecode_t_internal* lz4sd = &LZ4_streamDecode->internal_donotuse;
    int result;

    if (lz4sd->prefixSize == 0) {
        /* The first call, no dictionary yet. */
        assert(lz4sd->extDictSize == 0);
        result = LZ4_decompress_safe(source, dest, compressedSize, maxOutputSize);
        if (result <= 0) return result;
        lz4sd->prefixSize = (size_t)result;
        lz4sd->prefixEnd = (BYTE*)dest + result;
    } else if (lz4sd->prefixEnd == (BYTE*)dest) {
        /* They're rolling the current segment. */
        if (lz4sd->prefixSize >= 64 KB - 1)
            result = LZ4_decompress_safe_forceExtDict(source, dest, compressedSize, maxOutputSize,
                                                      lz4sd->prefixEnd - lz4sd->prefixSize, lz4sd->prefixSize);
        else if (lz4sd->extDictSize == 0)
            result = LZ4_decompress_safe_withSmallPrefix(source, dest, compressedSize, maxOutputSize,
                                                         lz4sd->prefixSize);
        else
            result = LZ4_decompress_safe_doubleDict(source, dest, compressedSize, maxOutputSize,
                                                    lz4sd->prefixSize, lz4sd->externalDict, lz4sd->extDictSize);
        if (result <= 0) return result;
        lz4sd->prefixSize += (size_t)result;
        lz4sd->prefixEnd  += result;
    } else {
        /* The buffer wraps around, or they're switching to another buffer. */
        lz4sd->extDictSize = lz4sd->prefixSize;
        lz4sd->externalDict = lz4sd->prefixEnd - lz4sd->extDictSize;
        result = LZ4_decompress_safe_forceExtDict(source, dest, compressedSize, maxOutputSize,
                                                  lz4sd->externalDict, lz4sd->extDictSize);
        if (result <= 0) return result;
        lz4sd->prefixSize = (size_t)result;
        lz4sd->prefixEnd  = (BYTE*)dest + result;
    }

    return result;
}

LZ4_FORCE_O2 int
LZ4_decompress_fast_continue (LZ4_streamDecode_t* LZ4_streamDecode,
                        const char* source, char* dest, int originalSize)
{
    LZ4_streamDecode_t_internal* const lz4sd =
        (assert(LZ4_streamDecode!=NULL), &LZ4_streamDecode->internal_donotuse);
    int result;

    DEBUGLOG(5, "LZ4_decompress_fast_continue (toDecodeSize=%i)", originalSize);
    assert(originalSize >= 0);

    if (lz4sd->prefixSize == 0) {
        DEBUGLOG(5, "first invocation : no prefix nor extDict");
        assert(lz4sd->extDictSize == 0);
        result = LZ4_decompress_fast(source, dest, originalSize);
        if (result <= 0) return result;
        lz4sd->prefixSize = (size_t)originalSize;
        lz4sd->prefixEnd = (BYTE*)dest + originalSize;
    } else if (lz4sd->prefixEnd == (BYTE*)dest) {
        DEBUGLOG(5, "continue using existing prefix");
        result = LZ4_decompress_unsafe_generic(
                        (const BYTE*)source, (BYTE*)dest, originalSize,
                        lz4sd->prefixSize,
                        lz4sd->externalDict, lz4sd->extDictSize);
        if (result <= 0) return result;
        lz4sd->prefixSize += (size_t)originalSize;
        lz4sd->prefixEnd  += originalSize;
    } else {
        DEBUGLOG(5, "prefix becomes extDict");
        lz4sd->extDictSize = lz4sd->prefixSize;
        lz4sd->externalDict = lz4sd->prefixEnd - lz4sd->extDictSize;
        result = LZ4_decompress_fast_extDict(source, dest, originalSize,
                                             lz4sd->externalDict, lz4sd->extDictSize);
        if (result <= 0) return result;
        lz4sd->prefixSize = (size_t)originalSize;
        lz4sd->prefixEnd  = (BYTE*)dest + originalSize;
    }

    return result;
}


/*
Advanced decoding functions :
*_usingDict() :
    These decoding functions work the same as "_continue" ones,
    the dictionary must be explicitly provided within parameters
*/

int LZ4_decompress_safe_usingDict(const char* source, char* dest, int compressedSize, int maxOutputSize, const char* dictStart, int dictSize)
{
    if (dictSize==0)
        return LZ4_decompress_safe(source, dest, compressedSize, maxOutputSize);
    if (dictStart+dictSize == dest) {
        if (dictSize >= 64 KB - 1) {
            return LZ4_decompress_safe_forceExtDict(source, dest, compressedSize, maxOutputSize, dictStart, (size_t)dictSize);
        }
        assert(dictSize >= 0);
        return LZ4_decompress_safe_withSmallPrefix(source, dest, compressedSize, maxOutputSize, (size_t)dictSize);
    }
    assert(dictSize >= 0);
    return LZ4_decompress_safe_forceExtDict(source, dest, compressedSize, maxOutputSize, dictStart, (size_t)dictSize);
}

int LZ4_decompress_safe_partial_usingDict(const char* source, char* dest, int compressedSize, int targetOutputSize, int dstCapacity, const char* dictStart, int dictSize)
{
    if (dictSize==0)
        return LZ4_decompress_safe_partial(source, dest, compressedSize, targetOutputSize, dstCapacity);
    if (dictStart+dictSize == dest) {
        if (dictSize >= 64 KB - 1) {
            return LZ4_decompress_safe_partial_forceExtDict(source, dest, compressedSize, targetOutputSize, dstCapacity, dictStart, (size_t)dictSize);
        }
        assert(dictSize >= 0);
        return LZ4_decompress_safe_partial_withSmallPrefix(source, dest, compressedSize, targetOutputSize, dstCapacity, (size_t)dictSize);
    }
    assert(dictSize >= 0);
    return LZ4_decompress_safe_partial_forceExtDict(source, dest, compressedSize, targetOutputSize, dstCapacity, dictStart, (size_t)dictSize);
}

int LZ4_decompress_fast_usingDict(const char* source, char* dest, int originalSize, const char* dictStart, int dictSize)
{
    if (dictSize==0 || dictStart+dictSize == dest)
        return LZ4_decompress_unsafe_generic(
                        (const BYTE*)source, (BYTE*)dest, originalSize,
                        (size_t)dictSize, NULL, 0);
    assert(dictSize >= 0);
    return LZ4_decompress_fast_extDict(source, dest, originalSize, dictStart, (size_t)dictSize);
}


/*=*************************************************
*  Obsolete Functions
***************************************************/
/* obsolete compression functions */
int LZ4_compress_limitedOutput(const char* source, char* dest, int inputSize, int maxOutputSize)
{
    return LZ4_compress_default(source, dest, inputSize, maxOutputSize);
}
int LZ4_compress(const char* src, char* dest, int srcSize)
{
    return LZ4_compress_default(src, dest, srcSize, LZ4_compressBound(srcSize));
}
int LZ4_compress_limitedOutput_withState (void* state, const char* src, char* dst, int srcSize, int dstSize)
{
    return LZ4_compress_fast_extState(state, src, dst, srcSize, dstSize, 1);
}
int LZ4_compress_withState (void* state, const char* src, char* dst, int srcSize)
{
    return LZ4_compress_fast_extState(state, src, dst, srcSize, LZ4_compressBound(srcSize), 1);
}
int LZ4_compress_limitedOutput_continue (LZ4_stream_t* LZ4_stream, const char* src, char* dst, int srcSize, int dstCapacity)
{
    return LZ4_compress_fast_continue(LZ4_stream, src, dst, srcSize, dstCapacity, 1);
}
int LZ4_compress_continue (LZ4_stream_t* LZ4_stream, const char* source, char* dest, int inputSize)
{
    return LZ4_compress_fast_continue(LZ4_stream, source, dest, inputSize, LZ4_compressBound(inputSize), 1);
}

/*
These decompression functions are deprecated and should no longer be used.
They are only provided here for compatibility with older user programs.
- LZ4_uncompress is totally equivalent to LZ4_decompress_fast
- LZ4_uncompress_unknownOutputSize is totally equivalent to LZ4_decompress_safe
*/
int LZ4_uncompress (const char* source, char* dest, int outputSize)
{
    return LZ4_decompress_fast(source, dest, outputSize);
}
int LZ4_uncompress_unknownOutputSize (const char* source, char* dest, int isize, int maxOutputSize)
{
    return LZ4_decompress_safe(source, dest, isize, maxOutputSize);
}

/* Obsolete Streaming functions */

int LZ4_sizeofStreamState(void) { return sizeof(LZ4_stream_t); }

int LZ4_resetStreamState(void* state, char* inputBuffer)
{
    (void)inputBuffer;
    LZ4_resetStream((LZ4_stream_t*)state);
    return 0;
}

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
void* LZ4_create (char* inputBuffer)
{
    (void)inputBuffer;
    return LZ4_createStream();
}
#endif

char* LZ4_slideInputBuffer (void* state)
{
    /* avoid const char * -> char * conversion warning */
    return (char *)(uptrval)((LZ4_stream_t*)state)->internal_donotuse.dictionary;
}

#endif   /* LZ4_COMMONDEFS_ONLY */
#ifdef __cplusplus
}
#endif

/* ==== fstapi implementation ==== */
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Copyright (c) 2009-2023 Tony Bybell.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * possible disables:
 *
 * FST_DYNAMIC_ALIAS_DISABLE : dynamic aliases are not processed
 * FST_DYNAMIC_ALIAS2_DISABLE : new encoding for dynamic aliases is not generated
 * FST_WRITEX_DISABLE : fast write I/O routines are disabled
 *
 * possible enables:
 *
 * FST_DEBUG : not for production use, only enable for development
 * FST_REMOVE_DUPLICATE_VC : glitch removal (has writer performance impact)
 * HAVE_LIBPTHREAD -> FST_WRITER_PARALLEL : enables inclusion of parallel writer code
 * _WAVE_HAVE_JUDY : use Judy arrays instead of Jenkins (undefine if LGPL is not acceptable)
 *
 */

// #ifndef FST_CONFIG_INCLUDE
// # define FST_CONFIG_INCLUDE <config.h>
// #endif
// #include FST_CONFIG_INCLUDE

#include <errno.h>

#ifndef HAVE_LIBPTHREAD
#undef FST_WRITER_PARALLEL
#endif

#ifdef FST_WRITER_PARALLEL
#include <pthread.h>
#endif

#ifdef __MINGW32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#elif defined(__GNUC__)
#ifndef __MINGW32__
#ifndef alloca
#define alloca __builtin_alloca
#endif
#else
#include <malloc.h>
#endif
#elif defined(_MSC_VER)
#include <malloc.h>
#define alloca _alloca
#endif

#ifndef PATH_MAX
#define PATH_MAX (4096)
#endif

#if defined(_MSC_VER)
typedef int64_t fst_off_t;
#else
typedef off_t fst_off_t;
#endif

/* note that Judy versus Jenkins requires more experimentation: they are  */
/* functionally equivalent though it appears Jenkins is slightly faster.  */
/* in addition, Jenkins is not bound by the LGPL.                         */
#ifdef _WAVE_HAVE_JUDY
#include <Judy.h>
#else
/* should be more than enough for fstWriterSetSourceStem() */
#define FST_PATH_HASHMASK               ((1UL << 16) - 1)
typedef const void *Pcvoid_t;
typedef void *Pvoid_t;
typedef void **PPvoid_t;
#define JudyHSIns(a,b,c,d) JenkinsIns((a),(b),(c),(hashmask))
#define JudyHSFreeArray(a,b) JenkinsFree((a),(hashmask))
void JenkinsFree(void *base_i, uint32_t hashmask);
void **JenkinsIns(void *base_i, const unsigned char *mem, uint32_t length, uint32_t hashmask);
#endif


#ifndef FST_WRITEX_DISABLE
#define FST_WRITEX_MAX                  (64 * 1024)
#else
#define fstWritex(a,b,c) fstFwrite((b), (c), 1, fv)
#endif


/* these defines have a large impact on writer speed when a model has a */
/* huge number of symbols.  as a default, use 128MB and increment when  */
/* every 1M signals are defined.                                        */
#define FST_BREAK_SIZE                  (1UL << 27)
#define FST_BREAK_ADD_SIZE              (1UL << 22)
#define FST_BREAK_SIZE_MAX              (1UL << 31)
#define FST_ACTIVATE_HUGE_BREAK         (1000000)
#define FST_ACTIVATE_HUGE_INC           (1000000)

#define FST_WRITER_STR                  "fstWriter"
#define FST_ID_NAM_SIZ                  (512)
#define FST_ID_NAM_ATTR_SIZ             (65536+4096)
#define FST_DOUBLE_ENDTEST              (2.7182818284590452354)
#define FST_HDR_SIM_VERSION_SIZE        (128)
#define FST_HDR_DATE_SIZE               (119)
#define FST_HDR_FILETYPE_SIZE           (1)
#define FST_HDR_TIMEZERO_SIZE           (8)
#define FST_GZIO_LEN                    (32768)
#define FST_HDR_FOURPACK_DUO_SIZE       (4*1024*1024)
#define FST_ZWRAPPER_HDR_SIZE           (1+8+8)

#if defined(__APPLE__) && defined(__MACH__)
#define FST_MACOSX
#include <sys/sysctl.h>
#endif

#if defined(FST_MACOSX) || defined(__MINGW32__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
#define FST_UNBUFFERED_IO
#endif

#ifdef __GNUC__
/* Boolean expression more often true than false */
#define FST_LIKELY(x) __builtin_expect(!!(x), 1)
/* Boolean expression more often false than true */
#define FST_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define FST_LIKELY(x) (!!(x))
#define FST_UNLIKELY(x) (!!(x))
#endif

#define FST_APIMESS "FSTAPI  | "

/***********************/
/***                 ***/
/*** common function ***/
/***                 ***/
/***********************/

#ifdef __MINGW32__
#include <io.h>
#ifndef HAVE_FSEEKO
#define ftello _ftelli64
#define fseeko _fseeki64
#endif
#endif

/*
 * the recoded "extra" values...
 * note that FST_RCV_Q is currently unused and is for future expansion.
 * its intended use is as another level of escape such that any arbitrary
 * value can be stored as the value: { time_delta, 8 bits, FST_RCV_Q }.
 * this is currently not implemented so that the branchless decode is:
 * uint32_t shcnt = 2 << (vli & 1); tdelta = vli >> shcnt;
 */
#define FST_RCV_X (1 | (0<<1))
#define FST_RCV_Z (1 | (1<<1))
#define FST_RCV_H (1 | (2<<1))
#define FST_RCV_U (1 | (3<<1))
#define FST_RCV_W (1 | (4<<1))
#define FST_RCV_L (1 | (5<<1))
#define FST_RCV_D (1 | (6<<1))
#define FST_RCV_Q (1 | (7<<1))

#define FST_RCV_STR "xzhuwl-?"
/*                   01234567 */


/*
 * report abort messages
 */
static void chk_report_abort(const char *s)
{
fprintf(stderr,"Triggered %s security check, exiting.\n", s);
abort();
}


/*
 * prevent old file overwrite when currently being read
 */
static FILE *unlink_fopen(const char *nam, const char *mode)
{
unlink(nam);
return(fopen(nam, mode));
}


/*
 * system-specific temp file handling
 */
#ifdef __MINGW32__

static FILE* tmpfile_open(char **nam)
{
char *fname = NULL;
TCHAR szTempFileName[MAX_PATH];
TCHAR lpTempPathBuffer[MAX_PATH];
DWORD dwRetVal = 0;
UINT uRetVal = 0;
FILE *fh = NULL;

if(nam) /* cppcheck warning fix: nam is always defined, so this is not needed */
        {
        dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer);
        if((dwRetVal > MAX_PATH) || (dwRetVal == 0))
                {
                fprintf(stderr, FST_APIMESS "GetTempPath() failed in " __FILE__ " line %d, exiting.\n", __LINE__);
                exit(255);
                }
                else
                {
                uRetVal = GetTempFileName(lpTempPathBuffer, TEXT("FSTW"), 0, szTempFileName);
                if (uRetVal == 0)
                        {
                        fprintf(stderr, FST_APIMESS "GetTempFileName() failed in " __FILE__ " line %d, exiting.\n", __LINE__);
                        exit(255);
                        }
                        else
                        {
                        fname = strdup(szTempFileName);
                        }
                }

        if(fname)
                {
                *nam = fname;
                fh = unlink_fopen(fname, "w+b");
                }
        }

return(fh);
}

#else

static FILE* tmpfile_open(char **nam)
{
FILE *f = tmpfile(); /* replace with mkstemp() + fopen(), etc if this is not good enough */
if(nam) { *nam = NULL; }
return(f);
}

#endif


static void tmpfile_close(FILE **f, char **nam)
{
if(f)
        {
        if(*f) { fclose(*f); *f = NULL; }
        }

if(nam)
        {
        if(*nam)
                {
                unlink(*nam);
                free(*nam);
                *nam = NULL;
                }
        }
}

/*****************************************/


/*
 * to remove warn_unused_result compile time messages
 * (in the future there needs to be results checking)
 */
static size_t fstFread(void *buf, size_t siz, size_t cnt, FILE *fp)
{
return(fread(buf, siz, cnt, fp));
}

static size_t fstFwrite(const void *buf, size_t siz, size_t cnt, FILE *fp)
{
return(fwrite(buf, siz, cnt, fp));
}

static int fstFtruncate(int fd, fst_off_t length)
{
return(ftruncate(fd, length));
}


/*
 * realpath compatibility
 */
static char *fstRealpath(const char *path, char *resolved_path)
{
#if defined __USE_BSD || defined __USE_XOPEN_EXTENDED || defined __CYGWIN__ || defined HAVE_REALPATH
#if (defined(__MACH__) && defined(__APPLE__))
if(!resolved_path)
        {
        resolved_path = (char *)malloc(PATH_MAX+1); /* fixes bug on Leopard when resolved_path == NULL */
        }
#endif

return(realpath(path, resolved_path));

#else
#ifdef __MINGW32__
if(!resolved_path)
        {
        resolved_path = (char *)malloc(PATH_MAX+1);
        }
return(_fullpath(resolved_path, path, PATH_MAX));
#else
(void)path;
(void)resolved_path;
return(NULL);
#endif
#endif
}


/*
 * mmap compatibility
 */
#if defined __MINGW32__
#include <limits.h>
#define fstMmap(__addr,__len,__prot,__flags,__fd,__off) fstMmap2((__len), (__fd), (__off))
#define fstMunmap(__addr,__len)                         UnmapViewOfFile((LPCVOID)__addr)

static void *fstMmap2(size_t __len, int __fd, fst_off_t __off)
{
DWORD64 len64 = __len;  /* Must be 64-bit for shift below */
HANDLE handle = CreateFileMapping((HANDLE)_get_osfhandle(__fd), NULL,
				  PAGE_READWRITE, (DWORD)(len64 >> 32),
				  (DWORD)__len, NULL);
if (!handle) { return NULL; }

void *ptr = MapViewOfFileEx(handle, FILE_MAP_READ | FILE_MAP_WRITE,
			    0, (DWORD)__off, (SIZE_T)__len, (LPVOID)NULL);
CloseHandle(handle);
return ptr;
}
#else
#include <sys/mman.h>
#if defined(__SUNPRO_C)
#define FST_CADDR_T_CAST (caddr_t)
#else
#define FST_CADDR_T_CAST
#endif
#define fstMmap(__addr,__len,__prot,__flags,__fd,__off) (void*)mmap(FST_CADDR_T_CAST (__addr),(__len),(__prot),(__flags),(__fd),(__off))
#define fstMunmap(__addr,__len)                         { if(__addr) munmap(FST_CADDR_T_CAST (__addr),(__len)); }
#endif


/*
 * regular and variable-length integer access functions
 */
static uint32_t fstGetUint32(unsigned char *mem)
{
uint32_t u32;
unsigned char *buf = (unsigned char *)(&u32);

memcpy(buf, mem, sizeof(uint32_t));

return(*(uint32_t *)buf);
}


static int fstWriterUint64(FILE *handle, uint64_t v)
{
unsigned char buf[8];
int i;

for(i=7;i>=0;i--)
        {
        buf[i] = v & 0xff;
        v >>= 8;
        }

fstFwrite(buf, 8, 1, handle);
return(8);
}


static uint64_t fstReaderUint64(FILE *f)
{
uint64_t val = 0;
unsigned char buf[sizeof(uint64_t)];
unsigned int i;

fstFread(buf, sizeof(uint64_t), 1, f);
for(i=0;i<sizeof(uint64_t);i++)
        {
        val <<= 8;
        val |= buf[i];
        }

return(val);
}


static uint32_t fstGetVarint32(unsigned char *mem, int *skiplen)
{
unsigned char *mem_orig = mem;
uint32_t rc = 0;
while(*mem & 0x80)
        {
        mem++;
        }

*skiplen = mem - mem_orig + 1;
for(;;)
        {
        rc <<= 7;
        rc |= (uint32_t)(*mem & 0x7f);
        if(mem == mem_orig)
                {
                break;
                }
        mem--;
        }

return(rc);
}


static uint32_t fstGetVarint32Length(unsigned char *mem)
{
unsigned char *mem_orig = mem;

while(*mem & 0x80)
        {
        mem++;
        }

return(mem - mem_orig + 1);
}


static uint32_t fstGetVarint32NoSkip(unsigned char *mem)
{
unsigned char *mem_orig = mem;
uint32_t rc = 0;
while(*mem & 0x80)
        {
        mem++;
        }

for(;;)
        {
        rc <<= 7;
        rc |= (uint32_t)(*mem & 0x7f);
        if(mem == mem_orig)
                {
                break;
                }
        mem--;
        }

return(rc);
}


static unsigned char *fstCopyVarint32ToLeft(unsigned char *pnt, uint32_t v)
{
unsigned char *spnt;
uint32_t nxt = v;
int cnt = 1;
int i;

while((nxt = nxt>>7)) /* determine len to avoid temp buffer copying to cut down on load-hit-store */
        {
        cnt++;
        }

pnt -= cnt;
spnt = pnt;
cnt--;

for(i=0;i<cnt;i++) /* now generate left to right as normal */
        {
        nxt = v>>7;
        *(spnt++) = ((unsigned char)v) | 0x80;
        v = nxt;
        }
*spnt = (unsigned char)v;

return(pnt);
}


static unsigned char *fstCopyVarint64ToRight(unsigned char *pnt, uint64_t v)
{
uint64_t nxt;

while((nxt = v>>7))
        {
        *(pnt++) = ((unsigned char)v) | 0x80;
        v = nxt;
        }
*(pnt++) = (unsigned char)v;

return(pnt);
}


static uint64_t fstGetVarint64(unsigned char *mem, int *skiplen)
{
unsigned char *mem_orig = mem;
uint64_t rc = 0;
while(*mem & 0x80)
        {
        mem++;
        }

*skiplen = mem - mem_orig + 1;
for(;;)
        {
        rc <<= 7;
        rc |= (uint64_t)(*mem & 0x7f);
        if(mem == mem_orig)
                {
                break;
                }
        mem--;
        }

return(rc);
}


static uint32_t fstReaderVarint32(FILE *f)
{
const int chk_len_max = 5; /* TALOS-2023-1783 */
int chk_len = chk_len_max;
unsigned char buf[chk_len_max];
unsigned char *mem = buf;
uint32_t rc = 0;
int ch;

do
        {
        ch = fgetc(f);
        *(mem++) = ch;
        } while((ch & 0x80) && (--chk_len));

if(ch & 0x80) chk_report_abort("TALOS-2023-1783");
mem--;

for(;;)
        {
        rc <<= 7;
        rc |= (uint32_t)(*mem & 0x7f);
        if(mem == buf)
                {
                break;
                }
        mem--;
        }

return(rc);
}


static uint32_t fstReaderVarint32WithSkip(FILE *f, uint32_t *skiplen)
{
const int chk_len_max = 5; /* TALOS-2023-1783 */
int chk_len = chk_len_max;
unsigned char buf[chk_len_max];
unsigned char *mem = buf;
uint32_t rc = 0;
int ch;

do
        {
        ch = fgetc(f);
        *(mem++) = ch;
        } while((ch & 0x80) && (--chk_len));

if(ch & 0x80) chk_report_abort("TALOS-2023-1783");
*skiplen = mem - buf;
mem--;

for(;;)
        {
        rc <<= 7;
        rc |= (uint32_t)(*mem & 0x7f);
        if(mem == buf)
                {
                break;
                }
        mem--;
        }

return(rc);
}


static uint64_t fstReaderVarint64(FILE *f)
{
const int chk_len_max = 16; /* TALOS-2023-1783 */
int chk_len = chk_len_max;
unsigned char buf[chk_len_max];
unsigned char *mem = buf;
uint64_t rc = 0;
int ch;

do
        {
        ch = fgetc(f);
        *(mem++) = ch;
        } while((ch & 0x80) && (--chk_len));

if(ch & 0x80) chk_report_abort("TALOS-2023-1783");
mem--;


for(;;)
        {
        rc <<= 7;
        rc |= (uint64_t)(*mem & 0x7f);
        if(mem == buf)
                {
                break;
                }
        mem--;
        }

return(rc);
}


static int fstWriterVarint(FILE *handle, uint64_t v)
{
uint64_t nxt;
unsigned char buf[10]; /* ceil(64/7) = 10 */
unsigned char *pnt = buf;
int len;

while((nxt = v>>7))
        {
        *(pnt++) = ((unsigned char)v) | 0x80;
        v = nxt;
        }
*(pnt++) = (unsigned char)v;

len = pnt-buf;
fstFwrite(buf, len, 1, handle);
return(len);
}


/* signed integer read/write routines are currently unused */
static int64_t fstGetSVarint64(unsigned char *mem, int *skiplen)
{
unsigned char *mem_orig = mem;
int64_t rc = 0;
const int64_t one = 1;
const int siz = sizeof(int64_t) * 8;
int shift = 0;
unsigned char byt;

do      {
        byt = *(mem++);
        rc |= ((int64_t)(byt & 0x7f)) << shift;
        shift += 7;

        } while(byt & 0x80);

if((shift<siz) && (byt & 0x40))
        {
        rc |= -(one << shift); /* sign extend */
        }

*skiplen = mem - mem_orig;

return(rc);
}


#ifndef FST_DYNAMIC_ALIAS2_DISABLE
static int fstWriterSVarint(FILE *handle, int64_t v)
{
unsigned char buf[15]; /* ceil(64/7) = 10 + sign byte padded way up */
unsigned char byt;
unsigned char *pnt = buf;
int more = 1;
int len;

do      {
        byt = v | 0x80;
        v >>= 7;

        if (((!v) && (!(byt & 0x40))) || ((v == -1) && (byt & 0x40)))
                {
                more = 0;
                byt &= 0x7f;
                }

        *(pnt++) = byt;
        } while(more);

len = pnt-buf;
fstFwrite(buf, len, 1, handle);
return(len);
}
#endif


/***********************/
/***                 ***/
/*** writer function ***/
/***                 ***/
/***********************/

/*
 * private structs
 */
struct fstBlackoutChain
{
struct fstBlackoutChain *next;
uint64_t tim;
unsigned active : 1;
};


struct fstWriterContext
{
FILE *handle;
FILE *hier_handle;
FILE *geom_handle;
FILE *valpos_handle;
FILE *curval_handle;
FILE *tchn_handle;

unsigned char *vchg_mem;

fst_off_t hier_file_len;

uint32_t *valpos_mem;
unsigned char *curval_mem;

unsigned char *outval_mem; /* for two-state / Verilator-style value changes */
uint32_t outval_alloc_siz;

char *filename;

fstHandle maxhandle;
fstHandle numsigs;
uint32_t maxvalpos;

unsigned vc_emitted : 1;
unsigned is_initial_time : 1;
unsigned fourpack : 1;
unsigned fastpack : 1;

int64_t timezero;
fst_off_t section_header_truncpos;
uint32_t tchn_cnt, tchn_idx;
uint64_t curtime;
uint64_t firsttime;
uint32_t vchg_siz;
uint32_t vchg_alloc_siz;

uint32_t secnum;
fst_off_t section_start;

uint32_t numscopes;
double nan; /* nan value for uninitialized doubles */

struct fstBlackoutChain *blackout_head;
struct fstBlackoutChain *blackout_curr;
uint32_t num_blackouts;

uint64_t dump_size_limit;

unsigned char filetype; /* default is 0, FST_FT_VERILOG */

unsigned compress_hier : 1;
unsigned repack_on_close : 1;
unsigned skip_writing_section_hdr : 1;
unsigned size_limit_locked : 1;
unsigned section_header_only : 1;
unsigned flush_context_pending : 1;
unsigned parallel_enabled : 1;
unsigned parallel_was_enabled : 1;

/* should really be semaphores, but are bytes to cut down on read-modify-write window size */
unsigned char already_in_flush; /* in case control-c handlers interrupt */
unsigned char already_in_close; /* in case control-c handlers interrupt */

#ifdef FST_WRITER_PARALLEL
pthread_mutex_t mutex;
pthread_t thread;
pthread_attr_t thread_attr;
struct fstWriterContext *xc_parent;
#endif
unsigned in_pthread : 1;

size_t fst_orig_break_size;
size_t fst_orig_break_add_size;

size_t fst_break_size;
size_t fst_break_add_size;

size_t fst_huge_break_size;

fstHandle next_huge_break;

Pvoid_t path_array;
uint32_t path_array_count;

unsigned fseek_failed : 1;

char *geom_handle_nam;
char *valpos_handle_nam;
char *curval_handle_nam;
char *tchn_handle_nam;

fstEnumHandle max_enumhandle;
};

static int fstWriterFseeko(struct fstWriterContext *xc, FILE *stream, fst_off_t offset, int whence)
{
int rc = fseeko(stream, offset, whence);

if(rc<0)
        {
        xc->fseek_failed = 1;
#ifdef FST_DEBUG
        fprintf(stderr, FST_APIMESS "Seek to #%" PRId64 " (whence = %d) failed!\n", offset, whence);
        perror("Why");
#endif
        }

return(rc);
}


static uint32_t fstWriterUint32WithVarint32(struct fstWriterContext *xc, uint32_t *u, uint32_t v, const void *dbuf, uint32_t siz)
{
unsigned char *buf = xc->vchg_mem + xc->vchg_siz;
unsigned char *pnt = buf;
uint32_t nxt;
uint32_t len;

memcpy(pnt, u, sizeof(uint32_t));
pnt += 4;

while((nxt = v>>7))
        {
        *(pnt++) = ((unsigned char)v) | 0x80;
        v = nxt;
        }
*(pnt++) = (unsigned char)v;
memcpy(pnt, dbuf, siz);

len = pnt-buf + siz;
return(len);
}


static uint32_t fstWriterUint32WithVarint32AndLength(struct fstWriterContext *xc, uint32_t *u, uint32_t v, const void *dbuf, uint32_t siz)
{
unsigned char *buf = xc->vchg_mem + xc->vchg_siz;
unsigned char *pnt = buf;
uint32_t nxt;
uint32_t len;

memcpy(pnt, u, sizeof(uint32_t));
pnt += 4;

while((nxt = v>>7))
        {
        *(pnt++) = ((unsigned char)v) | 0x80;
        v = nxt;
        }
*(pnt++) = (unsigned char)v;

v = siz;
while((nxt = v>>7))
        {
        *(pnt++) = ((unsigned char)v) | 0x80;
        v = nxt;
        }
*(pnt++) = (unsigned char)v;

memcpy(pnt, dbuf, siz);

len = pnt-buf + siz;
return(len);
}


/*
 * header bytes, write here so defines are set up before anything else
 * that needs to use them
 */
static void fstWriterEmitHdrBytes(struct fstWriterContext *xc)
{
char vbuf[FST_HDR_SIM_VERSION_SIZE];
char dbuf[FST_HDR_DATE_SIZE];
double endtest = FST_DOUBLE_ENDTEST;
time_t walltime;

#define FST_HDR_OFFS_TAG                        (0)
fputc(FST_BL_HDR, xc->handle);                  /* +0 tag */

#define FST_HDR_OFFS_SECLEN                     (FST_HDR_OFFS_TAG + 1)
fstWriterUint64(xc->handle, 329);               /* +1 section length */

#define FST_HDR_OFFS_START_TIME                 (FST_HDR_OFFS_SECLEN + 8)
fstWriterUint64(xc->handle, 0);                 /* +9 start time */

#define FST_HDR_OFFS_END_TIME                   (FST_HDR_OFFS_START_TIME + 8)
fstWriterUint64(xc->handle, 0);                 /* +17 end time */

#define FST_HDR_OFFS_ENDIAN_TEST                (FST_HDR_OFFS_END_TIME + 8)
fstFwrite(&endtest, 8, 1, xc->handle);          /* +25 endian test for reals */

#define FST_HDR_OFFS_MEM_USED                   (FST_HDR_OFFS_ENDIAN_TEST + 8)
fstWriterUint64(xc->handle, xc->fst_break_size);/* +33 memory used by writer */

#define FST_HDR_OFFS_NUM_SCOPES                 (FST_HDR_OFFS_MEM_USED + 8)
fstWriterUint64(xc->handle, 0);                 /* +41 scope creation count */

#define FST_HDR_OFFS_NUM_VARS                   (FST_HDR_OFFS_NUM_SCOPES + 8)
fstWriterUint64(xc->handle, 0);                 /* +49 var creation count */

#define FST_HDR_OFFS_MAXHANDLE                  (FST_HDR_OFFS_NUM_VARS + 8)
fstWriterUint64(xc->handle, 0);                 /* +57 max var idcode */

#define FST_HDR_OFFS_SECTION_CNT                (FST_HDR_OFFS_MAXHANDLE + 8)
fstWriterUint64(xc->handle, 0);                 /* +65 vc section count */

#define FST_HDR_OFFS_TIMESCALE                  (FST_HDR_OFFS_SECTION_CNT + 8)
fputc((-9)&255, xc->handle);                    /* +73 timescale 1ns */

#define FST_HDR_OFFS_SIM_VERSION                (FST_HDR_OFFS_TIMESCALE + 1)
memset(vbuf, 0, FST_HDR_SIM_VERSION_SIZE);
strcpy(vbuf, FST_WRITER_STR);
fstFwrite(vbuf, FST_HDR_SIM_VERSION_SIZE, 1, xc->handle); /* +74 version */

#define FST_HDR_OFFS_DATE                       (FST_HDR_OFFS_SIM_VERSION + FST_HDR_SIM_VERSION_SIZE)
memset(dbuf, 0, FST_HDR_DATE_SIZE);
time(&walltime);
strcpy(dbuf, asctime(localtime(&walltime)));
fstFwrite(dbuf, FST_HDR_DATE_SIZE, 1, xc->handle);      /* +202 date */

/* date size is deliberately overspecified at 119 bytes (originally 128) in order to provide backfill for new args */

#define FST_HDR_OFFS_FILETYPE                   (FST_HDR_OFFS_DATE + FST_HDR_DATE_SIZE)
fputc(xc->filetype, xc->handle);                /* +321 filetype */

#define FST_HDR_OFFS_TIMEZERO                   (FST_HDR_OFFS_FILETYPE + FST_HDR_FILETYPE_SIZE)
fstWriterUint64(xc->handle, xc->timezero);      /* +322 timezero */

#define FST_HDR_LENGTH                          (FST_HDR_OFFS_TIMEZERO + FST_HDR_TIMEZERO_SIZE)
                                                /* +330 next section starts here */
fflush(xc->handle);
}


/*
 * mmap functions
 */
static void fstWriterMmapSanity(void *pnt, const char *file, int line, const char *usage)
{
if(pnt == NULL
#ifdef MAP_FAILED
   || pnt == MAP_FAILED
#endif
  )
	{
	fprintf(stderr, "fstMmap() assigned to %s failed: errno: %d, file %s, line %d.\n", usage, errno, file, line);
#if !defined(__MINGW32__)
	perror("Why");
#else
	LPSTR mbuf = NULL;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
		      | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(),
		      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPSTR)&mbuf, 0, NULL);
	fprintf(stderr, "%s", mbuf);
	LocalFree(mbuf);
#endif
	pnt = NULL;
	}
}


static void fstWriterCreateMmaps(struct fstWriterContext *xc)
{
fst_off_t curpos = ftello(xc->handle);

fflush(xc->hier_handle);

/* write out intermediate header */
fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_START_TIME, SEEK_SET);
fstWriterUint64(xc->handle, xc->firsttime);
fstWriterUint64(xc->handle, xc->curtime);
fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_NUM_SCOPES, SEEK_SET);
fstWriterUint64(xc->handle, xc->numscopes);
fstWriterUint64(xc->handle, xc->numsigs);
fstWriterUint64(xc->handle, xc->maxhandle);
fstWriterUint64(xc->handle, xc->secnum);
fstWriterFseeko(xc, xc->handle, curpos, SEEK_SET);
fflush(xc->handle);

/* do mappings */
if(!xc->valpos_mem)
        {
        fflush(xc->valpos_handle);
	errno = 0;
	if(xc->maxhandle)
		{
	        fstWriterMmapSanity(xc->valpos_mem = (uint32_t *)fstMmap(NULL, xc->maxhandle * 4 * sizeof(uint32_t), PROT_READ|PROT_WRITE, MAP_SHARED, fileno(xc->valpos_handle), 0), __FILE__, __LINE__, "xc->valpos_mem");
		}
        }
if(!xc->curval_mem)
        {
        fflush(xc->curval_handle);
	errno = 0;
	if(xc->maxvalpos)
		{
	        fstWriterMmapSanity(xc->curval_mem = (unsigned char *)fstMmap(NULL, xc->maxvalpos, PROT_READ|PROT_WRITE, MAP_SHARED, fileno(xc->curval_handle), 0), __FILE__, __LINE__, "xc->curval_handle");
		}
        }
}


static void fstDestroyMmaps(struct fstWriterContext *xc, int is_closing)
{
(void)is_closing;

fstMunmap(xc->valpos_mem, xc->maxhandle * 4 * sizeof(uint32_t));
xc->valpos_mem = NULL;

fstMunmap(xc->curval_mem, xc->maxvalpos);
xc->curval_mem = NULL;
}


/*
 * set up large and small memory usages
 * crossover point in model is FST_ACTIVATE_HUGE_BREAK number of signals
 */
static void fstDetermineBreakSize(struct fstWriterContext *xc)
{
#if defined(__linux__) || defined(FST_MACOSX)
int was_set = 0;

#ifdef __linux__
FILE *f = fopen("/proc/meminfo", "rb");

if(f)
        {
        char buf[257];
        char *s;
        while(!feof(f))
                {
                buf[0] = 0;
                s = fgets(buf, 256, f);
                if(s && *s)
                        {
                        if(!strncmp(s, "MemTotal:", 9))
                                {
                                size_t v = atol(s+10);
                                v *= 1024; /* convert to bytes */
                                v /= 8; /* chop down to 1/8 physical memory */
                                if(v > FST_BREAK_SIZE)
                                        {
                                        if(v > FST_BREAK_SIZE_MAX)
                                                {
                                                v = FST_BREAK_SIZE_MAX;
                                                }

                                        xc->fst_huge_break_size = v;
                                        was_set = 1;
                                        break;
                                        }
                                }
                        }
                }

        fclose(f);
        }

if(!was_set)
        {
        xc->fst_huge_break_size = FST_BREAK_SIZE;
        }
#else
int mib[2];
int64_t v;
size_t length;

mib[0] = CTL_HW;
mib[1] = HW_MEMSIZE;
length = sizeof(int64_t);
if(!sysctl(mib, 2, &v, &length, NULL, 0))
        {
        v /= 8;

        if(v > (int64_t)FST_BREAK_SIZE)
                {
                if(v > (int64_t)FST_BREAK_SIZE_MAX)
                        {
                        v = FST_BREAK_SIZE_MAX;
                        }

                xc->fst_huge_break_size = v;
                was_set = 1;
                }
        }

if(!was_set)
        {
        xc->fst_huge_break_size = FST_BREAK_SIZE;
        }
#endif
#else
xc->fst_huge_break_size = FST_BREAK_SIZE;
#endif

xc->fst_break_size = xc->fst_orig_break_size = FST_BREAK_SIZE;
xc->fst_break_add_size = xc->fst_orig_break_add_size = FST_BREAK_ADD_SIZE;
xc->next_huge_break = FST_ACTIVATE_HUGE_BREAK;
}


/*
 * file creation and close
 */
void *fstWriterCreate(const char *nam, int use_compressed_hier)
{
struct fstWriterContext *xc = (struct fstWriterContext *)calloc(1, sizeof(struct fstWriterContext));

xc->compress_hier = use_compressed_hier;
fstDetermineBreakSize(xc);

if((!nam)||
        (!(xc->handle=unlink_fopen(nam, "w+b"))))
        {
        free(xc);
        xc=NULL;
        }
        else
        {
        int flen = strlen(nam);
        char *hf = (char *)calloc(1, flen + 6);

        memcpy(hf, nam, flen);
        strcpy(hf + flen, ".hier");
        xc->hier_handle = unlink_fopen(hf, "w+b");

        xc->geom_handle = tmpfile_open(&xc->geom_handle_nam);           /* .geom */
        xc->valpos_handle = tmpfile_open(&xc->valpos_handle_nam);       /* .offs */
        xc->curval_handle = tmpfile_open(&xc->curval_handle_nam);       /* .bits */
        xc->tchn_handle = tmpfile_open(&xc->tchn_handle_nam);           /* .tchn */
        xc->vchg_alloc_siz = xc->fst_break_size + xc->fst_break_add_size;
        xc->vchg_mem = (unsigned char *)malloc(xc->vchg_alloc_siz);

        if(xc->hier_handle && xc->geom_handle && xc->valpos_handle && xc->curval_handle && xc->vchg_mem && xc->tchn_handle)
                {
                xc->filename = strdup(nam);
                xc->is_initial_time = 1;

                fstWriterEmitHdrBytes(xc);
                xc->nan = strtod("NaN", NULL);
#ifdef FST_WRITER_PARALLEL
                pthread_mutex_init(&xc->mutex, NULL);
                pthread_attr_init(&xc->thread_attr);
                pthread_attr_setdetachstate(&xc->thread_attr, PTHREAD_CREATE_DETACHED);
#endif
                }
                else
                {
                fclose(xc->handle);
                if(xc->hier_handle) { fclose(xc->hier_handle); unlink(hf); }
                tmpfile_close(&xc->geom_handle, &xc->geom_handle_nam);
                tmpfile_close(&xc->valpos_handle, &xc->valpos_handle_nam);
                tmpfile_close(&xc->curval_handle, &xc->curval_handle_nam);
                tmpfile_close(&xc->tchn_handle, &xc->tchn_handle_nam);
                free(xc->vchg_mem);
                free(xc);
                xc=NULL;
                }

        free(hf);
        }

return(xc);
}


/*
 * generation and writing out of value change data sections
 */
static void fstWriterEmitSectionHeader(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

if(xc)
        {
        unsigned long destlen;
        unsigned char *dmem;
        int rc;

        destlen = xc->maxvalpos;
        dmem = (unsigned char *)malloc(compressBound(destlen));
        rc = compress2(dmem, &destlen, xc->curval_mem, xc->maxvalpos, 4); /* was 9...which caused performance drag on traces with many signals */

        fputc(FST_BL_SKIP, xc->handle);                 /* temporarily tag the section, use FST_BL_VCDATA on finalize */
        xc->section_start = ftello(xc->handle);
#ifdef FST_WRITER_PARALLEL
        if(xc->xc_parent) xc->xc_parent->section_start = xc->section_start;
#endif
        xc->section_header_only = 1;                    /* indicates truncate might be needed */
        fstWriterUint64(xc->handle, 0);                 /* placeholder = section length */
        fstWriterUint64(xc->handle, xc->is_initial_time ? xc->firsttime : xc->curtime);         /* begin time of section */
        fstWriterUint64(xc->handle, xc->curtime);       /* end time of section (placeholder) */
        fstWriterUint64(xc->handle, 0);                 /* placeholder = amount of buffer memory required in reader for full vc traversal */
        fstWriterVarint(xc->handle, xc->maxvalpos);     /* maxvalpos = length of uncompressed data */

        if((rc == Z_OK) && (destlen < xc->maxvalpos))
                {
                fstWriterVarint(xc->handle, destlen);   /* length of compressed data */
                }
                else
                {
                fstWriterVarint(xc->handle, xc->maxvalpos); /* length of (unable to be) compressed data */
                }
        fstWriterVarint(xc->handle, xc->maxhandle);     /* max handle associated with this data (in case of dynamic facility adds) */

        if((rc == Z_OK) && (destlen < xc->maxvalpos))
                {
                fstFwrite(dmem, destlen, 1, xc->handle);
                }
                else /* comparison between compressed / decompressed len tells if compressed */
                {
                fstFwrite(xc->curval_mem, xc->maxvalpos, 1, xc->handle);
                }

        free(dmem);
        }
}


/*
 * only to be called directly by fst code...otherwise must
 * be synced up with time changes
 */
#ifdef FST_WRITER_PARALLEL
static void fstWriterFlushContextPrivate2(void *ctx)
#else
static void fstWriterFlushContextPrivate(void *ctx)
#endif
{
#ifdef FST_DEBUG
int cnt = 0;
#endif
unsigned int i;
unsigned char *vchg_mem;
FILE *f;
fst_off_t fpos, indxpos, endpos;
uint32_t prevpos;
int zerocnt;
unsigned char *scratchpad;
unsigned char *scratchpnt;
unsigned char *tmem;
fst_off_t tlen;
fst_off_t unc_memreq = 0; /* for reader */
unsigned char *packmem;
unsigned int packmemlen;
uint32_t *vm4ip;
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
#ifdef FST_WRITER_PARALLEL
struct fstWriterContext *xc2 = xc->xc_parent;
#else
struct fstWriterContext *xc2 = xc;
#endif

#ifndef FST_DYNAMIC_ALIAS_DISABLE
Pvoid_t PJHSArray = (Pvoid_t) NULL;
#ifndef _WAVE_HAVE_JUDY
uint32_t hashmask =  xc->maxhandle;
hashmask |= hashmask >> 1;
hashmask |= hashmask >> 2;
hashmask |= hashmask >> 4;
hashmask |= hashmask >> 8;
hashmask |= hashmask >> 16;
#endif
#endif

if((xc->vchg_siz <= 1)||(xc->already_in_flush)) return;
xc->already_in_flush = 1; /* should really do this with a semaphore */

xc->section_header_only = 0;
scratchpad = (unsigned char *)malloc(xc->vchg_siz);

vchg_mem = xc->vchg_mem;

f = xc->handle;
fstWriterVarint(f, xc->maxhandle);      /* emit current number of handles */
fputc(xc->fourpack ? '4' : (xc->fastpack ? 'F' : 'Z'), f);
fpos = 1;

packmemlen = 1024;                      /* maintain a running "longest" allocation to */
packmem = (unsigned char *)malloc(packmemlen);           /* prevent continual malloc...free every loop iter */

for(i=0;i<xc->maxhandle;i++)
        {
        vm4ip = &(xc->valpos_mem[4*i]);

        if(vm4ip[2])
                {
                uint32_t offs = vm4ip[2];
                uint32_t next_offs;
                unsigned int wrlen;

                vm4ip[2] = fpos;

                scratchpnt = scratchpad + xc->vchg_siz;         /* build this buffer backwards */
                if(vm4ip[1] <= 1)
                        {
                        if(vm4ip[1] == 1)
                                {
                                wrlen = fstGetVarint32Length(vchg_mem + offs + 4); /* used to advance and determine wrlen */
#ifndef FST_REMOVE_DUPLICATE_VC
                                xc->curval_mem[vm4ip[0]] = vchg_mem[offs + 4 + wrlen]; /* checkpoint variable */
#endif
                                while(offs)
                                        {
                                        unsigned char val;
                                        uint32_t time_delta, rcv;
                                        next_offs = fstGetUint32(vchg_mem + offs);
                                        offs += 4;

                                        time_delta = fstGetVarint32(vchg_mem + offs, (int *)&wrlen);
                                        val = vchg_mem[offs+wrlen];
                                        offs = next_offs;

                                        switch(val)
                                                {
                                                case '0':
                                                case '1':               rcv = ((val&1)<<1) | (time_delta<<2);
                                                                        break; /* pack more delta bits in for 0/1 vchs */

                                                case 'x': case 'X':     rcv = FST_RCV_X | (time_delta<<4); break;
                                                case 'z': case 'Z':     rcv = FST_RCV_Z | (time_delta<<4); break;
                                                case 'h': case 'H':     rcv = FST_RCV_H | (time_delta<<4); break;
                                                case 'u': case 'U':     rcv = FST_RCV_U | (time_delta<<4); break;
                                                case 'w': case 'W':     rcv = FST_RCV_W | (time_delta<<4); break;
                                                case 'l': case 'L':     rcv = FST_RCV_L | (time_delta<<4); break;
                                                default:                rcv = FST_RCV_D | (time_delta<<4); break;
                                                }

                                        scratchpnt = fstCopyVarint32ToLeft(scratchpnt, rcv);
                                        }
                                }
                                else
                                {
                                /* variable length */
                                /* fstGetUint32 (next_offs) + fstGetVarint32 (time_delta) + fstGetVarint32 (len) + payload */
                                unsigned char *pnt;
                                uint32_t record_len;
                                uint32_t time_delta;

                                while(offs)
                                        {
                                        next_offs = fstGetUint32(vchg_mem + offs);
                                        offs += 4;
                                        pnt = vchg_mem + offs;
                                        offs = next_offs;
                                        time_delta = fstGetVarint32(pnt, (int *)&wrlen);
                                        pnt += wrlen;
                                        record_len = fstGetVarint32(pnt, (int *)&wrlen);
                                        pnt += wrlen;

                                        scratchpnt -= record_len;
                                        memcpy(scratchpnt, pnt, record_len);

                                        scratchpnt = fstCopyVarint32ToLeft(scratchpnt, record_len);
                                        scratchpnt = fstCopyVarint32ToLeft(scratchpnt, (time_delta << 1)); /* reserve | 1 case for future expansion */
                                        }
                                }
                        }
                        else
                        {
                        wrlen = fstGetVarint32Length(vchg_mem + offs + 4); /* used to advance and determine wrlen */
#ifndef FST_REMOVE_DUPLICATE_VC
                        memcpy(xc->curval_mem + vm4ip[0], vchg_mem + offs + 4 + wrlen, vm4ip[1]); /* checkpoint variable */
#endif
                        while(offs)
                                {
                                unsigned int idx;
                                char is_binary = 1;
                                unsigned char *pnt;
                                uint32_t time_delta;

                                next_offs = fstGetUint32(vchg_mem + offs);
                                offs += 4;

                                time_delta = fstGetVarint32(vchg_mem + offs, (int *)&wrlen);

                                pnt = vchg_mem+offs+wrlen;
                                offs = next_offs;

                                for(idx=0;idx<vm4ip[1];idx++)
                                        {
                                        if((pnt[idx] == '0') || (pnt[idx] == '1'))
                                                {
                                                continue;
                                                }
                                                else
                                                {
                                                is_binary = 0;
                                                break;
                                                }
                                        }

                                if(is_binary)
                                        {
                                        unsigned char acc = 0;
                                        /* new algorithm */
                                        idx = ((vm4ip[1]+7) & ~7);
                                        switch(vm4ip[1] & 7)
                                                {
                                                case 0: do {    acc  = (pnt[idx+7-8] & 1) << 0; /* fallthrough */
                                                case 7:         acc |= (pnt[idx+6-8] & 1) << 1; /* fallthrough */
                                                case 6:         acc |= (pnt[idx+5-8] & 1) << 2; /* fallthrough */
                                                case 5:         acc |= (pnt[idx+4-8] & 1) << 3; /* fallthrough */
                                                case 4:         acc |= (pnt[idx+3-8] & 1) << 4; /* fallthrough */
                                                case 3:         acc |= (pnt[idx+2-8] & 1) << 5; /* fallthrough */
                                                case 2:         acc |= (pnt[idx+1-8] & 1) << 6; /* fallthrough */
                                                case 1:         acc |= (pnt[idx+0-8] & 1) << 7;
                                                                *(--scratchpnt) = acc;
                                                                idx -= 8;
                                                        } while(idx);
                                                }

                                        scratchpnt = fstCopyVarint32ToLeft(scratchpnt, (time_delta << 1));
                                        }
                                        else
                                        {
                                        scratchpnt -= vm4ip[1];
                                        memcpy(scratchpnt, pnt, vm4ip[1]);

                                        scratchpnt = fstCopyVarint32ToLeft(scratchpnt, (time_delta << 1) | 1);
                                        }
                                }
                        }

                wrlen = scratchpad + xc->vchg_siz - scratchpnt;
                unc_memreq += wrlen;
                if(wrlen > 32)
                        {
                        unsigned long destlen = wrlen;
                        unsigned char *dmem;
                        unsigned int rc;

                        if(!xc->fastpack)
                                {
                                if(wrlen <= packmemlen)
                                        {
                                        dmem = packmem;
                                        }
                                        else
                                        {
                                        free(packmem);
                                        dmem = packmem = (unsigned char *)malloc(compressBound(packmemlen = wrlen));
                                        }

                                rc = compress2(dmem, &destlen, scratchpnt, wrlen, 4);
                                if(rc == Z_OK)
                                        {
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                        PPvoid_t pv = JudyHSIns(&PJHSArray, dmem, destlen, NULL);
                                        if(*pv)
                                                {
                                                uint32_t pvi = (intptr_t)(*pv);
                                                vm4ip[2] = -pvi;
                                                }
                                                else
                                                {
                                                *pv = (void *)(intptr_t)(i+1);
#endif
                                                fpos += fstWriterVarint(f, wrlen);
                                                fpos += destlen;
                                                fstFwrite(dmem, destlen, 1, f);
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                                }
#endif
                                        }
                                        else
                                        {
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                        PPvoid_t pv = JudyHSIns(&PJHSArray, scratchpnt, wrlen, NULL);
                                        if(*pv)
                                                {
                                                uint32_t pvi = (intptr_t)(*pv);
                                                vm4ip[2] = -pvi;
                                                }
                                                else
                                                {
                                                *pv = (void *)(intptr_t)(i+1);
#endif
                                                fpos += fstWriterVarint(f, 0);
                                                fpos += wrlen;
                                                fstFwrite(scratchpnt, wrlen, 1, f);
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                                }
#endif
                                        }
                                }
                                else
                                {
                                /* this is extremely conservative: fastlz needs +5% for worst case, lz4 needs siz+(siz/255)+16 */
                                if(((wrlen * 2) + 2) <= packmemlen)
                                        {
                                        dmem = packmem;
                                        }
                                        else
                                        {
                                        free(packmem);
                                        dmem = packmem = (unsigned char *)malloc(packmemlen = (wrlen * 2) + 2);
                                        }

                                rc = (xc->fourpack) ? LZ4_compress_default((char *)scratchpnt, (char *)dmem, wrlen, packmemlen) : fastlz_compress(scratchpnt, wrlen, dmem);
                                if(rc < destlen)
                                        {
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                        PPvoid_t pv = JudyHSIns(&PJHSArray, dmem, rc, NULL);
                                        if(*pv)
                                                {
                                                uint32_t pvi = (intptr_t)(*pv);
                                                vm4ip[2] = -pvi;
                                                }
                                                else
                                                {
                                                *pv = (void *)(intptr_t)(i+1);
#endif
                                                fpos += fstWriterVarint(f, wrlen);
                                                fpos += rc;
                                                fstFwrite(dmem, rc, 1, f);
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                                }
#endif
                                        }
                                        else
                                        {
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                        PPvoid_t pv = JudyHSIns(&PJHSArray, scratchpnt, wrlen, NULL);
                                        if(*pv)
                                                {
                                                uint32_t pvi = (intptr_t)(*pv);
                                                vm4ip[2] = -pvi;
                                                }
                                                else
                                                {
                                                *pv = (void *)(intptr_t)(i+1);
#endif
                                                fpos += fstWriterVarint(f, 0);
                                                fpos += wrlen;
                                                fstFwrite(scratchpnt, wrlen, 1, f);
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                                }
#endif
                                        }
                                }
                        }
                        else
                        {
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                        PPvoid_t pv = JudyHSIns(&PJHSArray, scratchpnt, wrlen, NULL);
                        if(*pv)
                                {
                                uint32_t pvi = (intptr_t)(*pv);
                                vm4ip[2] = -pvi;
                                }
                                else
                                {
                                *pv = (void *)(intptr_t)(i+1);
#endif
                                fpos += fstWriterVarint(f, 0);
                                fpos += wrlen;
                                fstFwrite(scratchpnt, wrlen, 1, f);
#ifndef FST_DYNAMIC_ALIAS_DISABLE
                                }
#endif
                        }

                /* vm4ip[3] = 0; ...redundant with clearing below */
#ifdef FST_DEBUG
                cnt++;
#endif
                }
        }

#ifndef FST_DYNAMIC_ALIAS_DISABLE
JudyHSFreeArray(&PJHSArray, NULL);
#endif

free(packmem); packmem = NULL; /* packmemlen = 0; */ /* scan-build */

prevpos = 0; zerocnt = 0;
free(scratchpad); scratchpad = NULL;

indxpos = ftello(f);
xc->secnum++;

#ifndef FST_DYNAMIC_ALIAS2_DISABLE
if(1)
        {
        uint32_t prev_alias = 0;

        for(i=0;i<xc->maxhandle;i++)
                {
                vm4ip = &(xc->valpos_mem[4*i]);

                if(vm4ip[2])
                        {
                        if(zerocnt)
                                {
                                fpos += fstWriterVarint(f, (zerocnt << 1));
                                zerocnt = 0;
                                }

                        if(vm4ip[2] & 0x80000000)
                                {
                                if(vm4ip[2] != prev_alias)
                                        {
					int32_t  t_i32 = ((int32_t)(prev_alias = vm4ip[2])); /* vm4ip is generic unsigned data */
					int64_t  t_i64 = (int64_t)t_i32; /* convert to signed */
					uint64_t t_u64 = (uint64_t)t_i64; /* sign extend through 64b */

					fpos += fstWriterSVarint(f, (int64_t)((t_u64 << 1) | 1)); /* all in this block was: fpos += fstWriterSVarint(f, (((int64_t)((int32_t)(prev_alias = vm4ip[2]))) << 1) | 1); */
                                        }
                                        else
                                        {
                                        fpos += fstWriterSVarint(f, (0 << 1) | 1);
                                        }
                                }
                                else
                                {
                                fpos += fstWriterSVarint(f, ((vm4ip[2] - prevpos) << 1) | 1);
                                prevpos = vm4ip[2];
                                }
                        vm4ip[2] = 0;
                        vm4ip[3] = 0; /* clear out tchn idx */
                        }
                        else
                        {
                        zerocnt++;
                        }
                }
        }
        else
#endif
        {
        for(i=0;i<xc->maxhandle;i++)
                {
                vm4ip = &(xc->valpos_mem[4*i]);

                if(vm4ip[2])
                        {
                        if(zerocnt)
                                {
                                fpos += fstWriterVarint(f, (zerocnt << 1));
                                zerocnt = 0;
                                }

                        if(vm4ip[2] & 0x80000000)
                                {
                                fpos += fstWriterVarint(f, 0); /* signal, note that using a *signed* varint would be more efficient than this byte escape! */
                                fpos += fstWriterVarint(f, (-(int32_t)vm4ip[2]));
                                }
                                else
                                {
                                fpos += fstWriterVarint(f, ((vm4ip[2] - prevpos) << 1) | 1);
                                prevpos = vm4ip[2];
                                }
                        vm4ip[2] = 0;
                        vm4ip[3] = 0; /* clear out tchn idx */
                        }
                        else
                        {
                        zerocnt++;
                        }
                }
        }

if(zerocnt)
        {
        /* fpos += */ fstWriterVarint(f, (zerocnt << 1)); /* scan-build */
        }
#ifdef FST_DEBUG
fprintf(stderr, FST_APIMESS "value chains: %d\n", cnt);
#endif

xc->vchg_mem[0] = '!';
xc->vchg_siz = 1;

endpos = ftello(xc->handle);
fstWriterUint64(xc->handle, endpos-indxpos);            /* write delta index position at very end of block */

/*emit time changes for block */
fflush(xc->tchn_handle);
tlen = ftello(xc->tchn_handle);
fstWriterFseeko(xc, xc->tchn_handle, 0, SEEK_SET);

errno = 0;
fstWriterMmapSanity(tmem = (unsigned char *)fstMmap(NULL, tlen, PROT_READ|PROT_WRITE, MAP_SHARED, fileno(xc->tchn_handle), 0), __FILE__, __LINE__, "tmem");
if(tmem)
        {
        unsigned long destlen = tlen;
        unsigned char *dmem = (unsigned char *)malloc(compressBound(destlen));
        int rc = compress2(dmem, &destlen, tmem, tlen, 9);

        if((rc == Z_OK) && (((fst_off_t)destlen) < tlen))
                {
                fstFwrite(dmem, destlen, 1, xc->handle);
                }
                else /* comparison between compressed / decompressed len tells if compressed */
                {
                fstFwrite(tmem, tlen, 1, xc->handle);
                destlen = tlen;
                }
        free(dmem);
        fstMunmap(tmem, tlen);
        fstWriterUint64(xc->handle, tlen);              /* uncompressed */
        fstWriterUint64(xc->handle, destlen);           /* compressed */
        fstWriterUint64(xc->handle, xc->tchn_cnt);      /* number of time items */
        }

xc->tchn_cnt = xc->tchn_idx = 0;
fstWriterFseeko(xc, xc->tchn_handle, 0, SEEK_SET);
fstFtruncate(fileno(xc->tchn_handle), 0);

/* write block trailer */
endpos = ftello(xc->handle);
fstWriterFseeko(xc, xc->handle, xc->section_start, SEEK_SET);
fstWriterUint64(xc->handle, endpos - xc->section_start);        /* write block length */
fstWriterFseeko(xc, xc->handle, 8, SEEK_CUR);                           /* skip begin time */
fstWriterUint64(xc->handle, xc->curtime);                       /* write end time for section */
fstWriterUint64(xc->handle, unc_memreq);                        /* amount of buffer memory required in reader for full traversal */
fflush(xc->handle);

fstWriterFseeko(xc, xc->handle, xc->section_start-1, SEEK_SET);         /* write out FST_BL_VCDATA over FST_BL_SKIP */

#ifndef FST_DYNAMIC_ALIAS_DISABLE
#ifndef FST_DYNAMIC_ALIAS2_DISABLE
fputc(FST_BL_VCDATA_DYN_ALIAS2, xc->handle);
#else
fputc(FST_BL_VCDATA_DYN_ALIAS, xc->handle);
#endif
#else
fputc(FST_BL_VCDATA, xc->handle);
#endif

fflush(xc->handle);

fstWriterFseeko(xc, xc->handle, endpos, SEEK_SET);                              /* seek to end of file */

xc2->section_header_truncpos = endpos;                          /* cache in case of need to truncate */
if(xc->dump_size_limit)
        {
        if(endpos >= ((fst_off_t)xc->dump_size_limit))
                {
                xc2->skip_writing_section_hdr = 1;
                xc2->size_limit_locked = 1;
                xc2->is_initial_time = 1; /* to trick emit value and emit time change */
#ifdef FST_DEBUG
                fprintf(stderr, FST_APIMESS "<< dump file size limit reached, stopping dumping >>\n");
#endif
                }
        }

if(!xc2->skip_writing_section_hdr)
        {
        fstWriterEmitSectionHeader(xc);                         /* emit next section header */
        }
fflush(xc->handle);

xc->already_in_flush = 0;
}


#ifdef FST_WRITER_PARALLEL
static void *fstWriterFlushContextPrivate1(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
struct fstWriterContext *xc_parent;

pthread_mutex_lock(&(xc->xc_parent->mutex));
fstWriterFlushContextPrivate2(xc);

#ifdef FST_REMOVE_DUPLICATE_VC
free(xc->curval_mem);
#endif
free(xc->valpos_mem);
free(xc->vchg_mem);
tmpfile_close(&xc->tchn_handle, &xc->tchn_handle_nam);
xc_parent = xc->xc_parent;
free(xc);

xc_parent->in_pthread = 0;
pthread_mutex_unlock(&(xc_parent->mutex));

return(NULL);
}


static void fstWriterFlushContextPrivate(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

if(xc->parallel_enabled)
        {
        struct fstWriterContext *xc2 = (struct fstWriterContext *)malloc(sizeof(struct fstWriterContext));
        unsigned int i;

        pthread_mutex_lock(&xc->mutex);
        pthread_mutex_unlock(&xc->mutex);

        xc->xc_parent = xc;
        memcpy(xc2, xc, sizeof(struct fstWriterContext));

	if(sizeof(size_t) < sizeof(uint64_t))
		{
		/* TALOS-2023-1777 for 32b overflow */
		uint64_t chk_64 = xc->maxhandle * 4 * sizeof(uint32_t);
		size_t   chk_32 = xc->maxhandle * 4 * sizeof(uint32_t);
		if(chk_64 != chk_32) chk_report_abort("TALOS-2023-1777");
		}

        xc2->valpos_mem = (uint32_t *)malloc(xc->maxhandle * 4 * sizeof(uint32_t));
        memcpy(xc2->valpos_mem, xc->valpos_mem, xc->maxhandle * 4 * sizeof(uint32_t));

        /* curval mem is updated in the thread */
#ifdef FST_REMOVE_DUPLICATE_VC
        xc2->curval_mem = (unsigned char *)malloc(xc->maxvalpos);
        memcpy(xc2->curval_mem, xc->curval_mem, xc->maxvalpos);
#endif

        xc->vchg_mem = (unsigned char *)malloc(xc->vchg_alloc_siz);
        xc->vchg_mem[0] = '!';
        xc->vchg_siz = 1;

        for(i=0;i<xc->maxhandle;i++)
                {
                uint32_t *vm4ip = &(xc->valpos_mem[4*i]);
                vm4ip[2] = 0; /* zero out offset val */
                vm4ip[3] = 0; /* zero out last time change val */
                }

        xc->tchn_cnt = xc->tchn_idx = 0;
        xc->tchn_handle = tmpfile_open(&xc->tchn_handle_nam); /* child thread will deallocate file/name */
        fstWriterFseeko(xc, xc->tchn_handle, 0, SEEK_SET);
        fstFtruncate(fileno(xc->tchn_handle), 0);

        xc->section_header_only = 0;
        xc->secnum++;

	while (xc->in_pthread) 
		{ 
		pthread_mutex_lock(&xc->mutex); 
		pthread_mutex_unlock(&xc->mutex); 
		};

        pthread_mutex_lock(&xc->mutex);
	xc->in_pthread = 1;
        pthread_mutex_unlock(&xc->mutex);

        pthread_create(&xc->thread, &xc->thread_attr, fstWriterFlushContextPrivate1, xc2);
        }
        else
        {
        if(xc->parallel_was_enabled) /* conservatively block */
                {
                pthread_mutex_lock(&xc->mutex);
                pthread_mutex_unlock(&xc->mutex);
                }

        xc->xc_parent = xc;
        fstWriterFlushContextPrivate2(xc);
        }
}
#endif


/*
 * queues up a flush context operation
 */
void fstWriterFlushContext(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        if(xc->tchn_idx > 1)
                {
                xc->flush_context_pending = 1;
                }
        }
}


/*
 * close out FST file
 */
void fstWriterClose(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

#ifdef FST_WRITER_PARALLEL
if(xc)
        {
        pthread_mutex_lock(&xc->mutex);
        pthread_mutex_unlock(&xc->mutex);
        }
#endif

if(xc && !xc->already_in_close && !xc->already_in_flush)
        {
        unsigned char *tmem = NULL;
        fst_off_t fixup_offs, tlen, hlen;

        xc->already_in_close = 1; /* never need to zero this out as it is freed at bottom */

        if(xc->section_header_only && xc->section_header_truncpos && (xc->vchg_siz <= 1) && (!xc->is_initial_time))
                {
                fstFtruncate(fileno(xc->handle), xc->section_header_truncpos);
                fstWriterFseeko(xc, xc->handle, xc->section_header_truncpos, SEEK_SET);
                xc->section_header_only = 0;
                }
                else
                {
                xc->skip_writing_section_hdr = 1;
                if(!xc->size_limit_locked)
                        {
                        if(FST_UNLIKELY(xc->is_initial_time)) /* simulation time never advanced so mock up the changes as time zero ones */
                                {
                                fstHandle dupe_idx;

                                fstWriterEmitTimeChange(xc, 0); /* emit some time change just to have one */
                                for(dupe_idx = 0; dupe_idx < xc->maxhandle; dupe_idx++) /* now clone the values */
                                        {
                                        fstWriterEmitValueChange(xc, dupe_idx+1, xc->curval_mem + xc->valpos_mem[4*dupe_idx]);
                                        }
                                }
                        fstWriterFlushContextPrivate(xc);
#ifdef FST_WRITER_PARALLEL
                        pthread_mutex_lock(&xc->mutex);
                        pthread_mutex_unlock(&xc->mutex);

			while (xc->in_pthread) 
				{ 
				pthread_mutex_lock(&xc->mutex); 
				pthread_mutex_unlock(&xc->mutex); 
				};
#endif
                        }
                }
        fstDestroyMmaps(xc, 1);
	if(xc->outval_mem)
		{
		free(xc->outval_mem); xc->outval_mem = NULL;
		xc->outval_alloc_siz = 0;
		}

        /* write out geom section */
        fflush(xc->geom_handle);
        tlen = ftello(xc->geom_handle);
	errno = 0;
	if(tlen)
		{
	        fstWriterMmapSanity(tmem = (unsigned char *)fstMmap(NULL, tlen, PROT_READ|PROT_WRITE, MAP_SHARED, fileno(xc->geom_handle), 0), __FILE__, __LINE__, "tmem");
		}

        if(tmem)
                {
                unsigned long destlen = tlen;
                unsigned char *dmem = (unsigned char *)malloc(compressBound(destlen));
                int rc = compress2(dmem, &destlen, tmem, tlen, 9);

                if((rc != Z_OK) || (((fst_off_t)destlen) > tlen))
                        {
                        destlen = tlen;
                        }

                fixup_offs = ftello(xc->handle);
                fputc(FST_BL_SKIP, xc->handle);                 /* temporary tag */
                fstWriterUint64(xc->handle, destlen + 24);      /* section length */
                fstWriterUint64(xc->handle, tlen);              /* uncompressed */
                                                                /* compressed len is section length - 24 */
                fstWriterUint64(xc->handle, xc->maxhandle);     /* maxhandle */
                fstFwrite((((fst_off_t)destlen) != tlen) ? dmem : tmem, destlen, 1, xc->handle);
                fflush(xc->handle);

                fstWriterFseeko(xc, xc->handle, fixup_offs, SEEK_SET);
                fputc(FST_BL_GEOM, xc->handle);                 /* actual tag */

                fstWriterFseeko(xc, xc->handle, 0, SEEK_END);           /* move file pointer to end for any section adds */
                fflush(xc->handle);

                free(dmem);
                fstMunmap(tmem, tlen);
                }

        if(xc->num_blackouts)
                {
                uint64_t cur_bl = 0;
                fst_off_t bpos, eos;
                uint32_t i;

                fixup_offs = ftello(xc->handle);
                fputc(FST_BL_SKIP, xc->handle);                 /* temporary tag */
                bpos = fixup_offs + 1;
                fstWriterUint64(xc->handle, 0);                 /* section length */
                fstWriterVarint(xc->handle, xc->num_blackouts);

                for(i=0;i<xc->num_blackouts;i++)
                        {
                        fputc(xc->blackout_head->active, xc->handle);
                        fstWriterVarint(xc->handle, xc->blackout_head->tim - cur_bl);
                        cur_bl = xc->blackout_head->tim;
                        xc->blackout_curr = xc->blackout_head->next;
                        free(xc->blackout_head);
                        xc->blackout_head = xc->blackout_curr;
                        }

                eos = ftello(xc->handle);
                fstWriterFseeko(xc, xc->handle, bpos, SEEK_SET);
                fstWriterUint64(xc->handle, eos - bpos);
                fflush(xc->handle);

                fstWriterFseeko(xc, xc->handle, fixup_offs, SEEK_SET);
                fputc(FST_BL_BLACKOUT, xc->handle);     /* actual tag */

                fstWriterFseeko(xc, xc->handle, 0, SEEK_END);   /* move file pointer to end for any section adds */
                fflush(xc->handle);
                }

        if(xc->compress_hier)
                {
                fst_off_t hl, eos;
                gzFile zhandle;
                int zfd;
                int fourpack_duo = 0;
#ifndef __MINGW32__
		int fnam_len = strlen(xc->filename) + 5 + 1;
                char *fnam = (char *)malloc(fnam_len);
#endif

                fixup_offs = ftello(xc->handle);
                fputc(FST_BL_SKIP, xc->handle);                 /* temporary tag */
                hlen = ftello(xc->handle);
                fstWriterUint64(xc->handle, 0);                 /* section length */
                fstWriterUint64(xc->handle, xc->hier_file_len); /* uncompressed length */

                if(!xc->fourpack)
                        {
                        unsigned char *mem = (unsigned char *)malloc(FST_GZIO_LEN);
                        zfd = dup(fileno(xc->handle));
                        fflush(xc->handle);
                        zhandle = gzdopen(zfd, "wb4");
                        if(zhandle)
                                {
                                fstWriterFseeko(xc, xc->hier_handle, 0, SEEK_SET);
                                for(hl = 0; hl < xc->hier_file_len; hl += FST_GZIO_LEN)
                                        {
                                        unsigned len = ((xc->hier_file_len - hl) > FST_GZIO_LEN) ? FST_GZIO_LEN : (xc->hier_file_len - hl);
                                        fstFread(mem, len, 1, xc->hier_handle);
                                        gzwrite(zhandle, mem, len);
                                        }
                                gzclose(zhandle);
                                }
                                else
                                {
                                close(zfd);
                                }
                        free(mem);
                        }
                        else
                        {
                        int lz4_maxlen;
                        unsigned char *mem;
                        unsigned char *hmem = NULL;
                        int packed_len;

                        fflush(xc->handle);

                        lz4_maxlen = LZ4_compressBound(xc->hier_file_len);
                        mem = (unsigned char *)malloc(lz4_maxlen);
			errno = 0;
			if(xc->hier_file_len)
				{
	                        fstWriterMmapSanity(hmem = (unsigned char *)fstMmap(NULL, xc->hier_file_len, PROT_READ|PROT_WRITE, MAP_SHARED, fileno(xc->hier_handle), 0), __FILE__, __LINE__, "hmem");
				}
                        packed_len = LZ4_compress_default((char *)hmem, (char *)mem, xc->hier_file_len, lz4_maxlen);
                        fstMunmap(hmem, xc->hier_file_len);

                        fourpack_duo = (!xc->repack_on_close) && (xc->hier_file_len > FST_HDR_FOURPACK_DUO_SIZE); /* double pack when hierarchy is large */

                        if(fourpack_duo)        /* double packing with LZ4 is faster than gzip */
                                {
                                unsigned char *mem_duo;
                                int lz4_maxlen_duo;
                                int packed_len_duo;

                                lz4_maxlen_duo = LZ4_compressBound(packed_len);
                                mem_duo = (unsigned char *)malloc(lz4_maxlen_duo);
                                packed_len_duo = LZ4_compress_default((char *)mem, (char *)mem_duo, packed_len, lz4_maxlen_duo);

                                fstWriterVarint(xc->handle, packed_len); /* 1st round compressed length */
                                fstFwrite(mem_duo, packed_len_duo, 1, xc->handle);
                                free(mem_duo);
                                }
                                else
                                {
                                fstFwrite(mem, packed_len, 1, xc->handle);
                                }

                        free(mem);
                        }

                fstWriterFseeko(xc, xc->handle, 0, SEEK_END);
                eos = ftello(xc->handle);
                fstWriterFseeko(xc, xc->handle, hlen, SEEK_SET);
                fstWriterUint64(xc->handle, eos - hlen);
                fflush(xc->handle);

                fstWriterFseeko(xc, xc->handle, fixup_offs, SEEK_SET);
                fputc(xc->fourpack ?
                        ( fourpack_duo ? FST_BL_HIER_LZ4DUO : FST_BL_HIER_LZ4) :
                        FST_BL_HIER, xc->handle); /* actual tag now also == compression type */

                fstWriterFseeko(xc, xc->handle, 0, SEEK_END);   /* move file pointer to end for any section adds */
                fflush(xc->handle);

#ifndef __MINGW32__
                snprintf(fnam, fnam_len, "%s.hier", xc->filename);
                unlink(fnam);
                free(fnam);
#endif
                }

        /* finalize out header */
        fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_START_TIME, SEEK_SET);
        fstWriterUint64(xc->handle, xc->firsttime);
        fstWriterUint64(xc->handle, xc->curtime);
        fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_NUM_SCOPES, SEEK_SET);
        fstWriterUint64(xc->handle, xc->numscopes);
        fstWriterUint64(xc->handle, xc->numsigs);
        fstWriterUint64(xc->handle, xc->maxhandle);
        fstWriterUint64(xc->handle, xc->secnum);
        fflush(xc->handle);

        tmpfile_close(&xc->tchn_handle, &xc->tchn_handle_nam);
        free(xc->vchg_mem); xc->vchg_mem = NULL;
        tmpfile_close(&xc->curval_handle, &xc->curval_handle_nam);
        tmpfile_close(&xc->valpos_handle, &xc->valpos_handle_nam);
        tmpfile_close(&xc->geom_handle, &xc->geom_handle_nam);
        if(xc->hier_handle) { fclose(xc->hier_handle); xc->hier_handle = NULL; }
        if(xc->handle)
                {
                if(xc->repack_on_close)
                        {
                        FILE *fp;
                        fst_off_t offpnt, uclen;
                        int flen = strlen(xc->filename);
                        char *hf = (char *)calloc(1, flen + 5);

                        strcpy(hf, xc->filename);
                        strcpy(hf+flen, ".pak");
                        fp = fopen(hf, "wb");

                        if(fp)
                                {
                                gzFile dsth;
                                int zfd;
                                char gz_membuf[FST_GZIO_LEN];

                                fstWriterFseeko(xc, xc->handle, 0, SEEK_END);
                                uclen = ftello(xc->handle);

                                fputc(FST_BL_ZWRAPPER, fp);
                                fstWriterUint64(fp, 0);
                                fstWriterUint64(fp, uclen);
                                fflush(fp);

                                fstWriterFseeko(xc, xc->handle, 0, SEEK_SET);
                                zfd = dup(fileno(fp));
                                dsth = gzdopen(zfd, "wb4");
                                if(dsth)
                                        {
                                        for(offpnt = 0; offpnt < uclen; offpnt += FST_GZIO_LEN)
                                                {
                                                size_t this_len = ((uclen - offpnt) > FST_GZIO_LEN) ? FST_GZIO_LEN : (uclen - offpnt);
                                                fstFread(gz_membuf, this_len, 1, xc->handle);
                                                gzwrite(dsth, gz_membuf, this_len);
                                                }
                                        gzclose(dsth);
                                        }
                                        else
                                        {
                                        close(zfd);
                                        }
                                fstWriterFseeko(xc, fp, 0, SEEK_END);
                                offpnt = ftello(fp);
                                fstWriterFseeko(xc, fp, 1, SEEK_SET);
                                fstWriterUint64(fp, offpnt - 1);
                                fclose(fp);
                                fclose(xc->handle); xc->handle = NULL;

                                unlink(xc->filename);
                                rename(hf, xc->filename);
                                }
                                else
                                {
                                xc->repack_on_close = 0;
                                fclose(xc->handle); xc->handle = NULL;
                                }

                        free(hf);
                        }
                        else
                        {
                        fclose(xc->handle); xc->handle = NULL;
                        }
                }

#ifdef __MINGW32__
        {
        int flen = strlen(xc->filename);
        char *hf = (char *)calloc(1, flen + 6);
        strcpy(hf, xc->filename);

        if(xc->compress_hier)
                {
                strcpy(hf + flen, ".hier");
                unlink(hf); /* no longer needed as a section now exists for this */
                }

        free(hf);
        }
#endif

#ifdef FST_WRITER_PARALLEL
        pthread_mutex_destroy(&xc->mutex);
        pthread_attr_destroy(&xc->thread_attr);
#endif

        if(xc->path_array)
                {
#ifndef _WAVE_HAVE_JUDY
                const uint32_t hashmask = FST_PATH_HASHMASK;
#endif
                JudyHSFreeArray(&(xc->path_array), NULL);
                }

        free(xc->filename); xc->filename = NULL;
        free(xc);
        }
}


/*
 * functions to set miscellaneous header/block information
 */
void fstWriterSetDate(void *ctx, const char *dat)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        char s[FST_HDR_DATE_SIZE];
        fst_off_t fpos = ftello(xc->handle);
        int len = strlen(dat);

        fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_DATE, SEEK_SET);
        memset(s, 0, FST_HDR_DATE_SIZE);
        memcpy(s, dat, (len < FST_HDR_DATE_SIZE) ? len : FST_HDR_DATE_SIZE);
        fstFwrite(s, FST_HDR_DATE_SIZE, 1, xc->handle);
        fflush(xc->handle);
        fstWriterFseeko(xc, xc->handle, fpos, SEEK_SET);
        }
}


void fstWriterSetVersion(void *ctx, const char *vers)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc && vers)
        {
        char s[FST_HDR_SIM_VERSION_SIZE];
        fst_off_t fpos = ftello(xc->handle);
        int len = strlen(vers);

        fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_SIM_VERSION, SEEK_SET);
        memset(s, 0, FST_HDR_SIM_VERSION_SIZE);
        memcpy(s, vers, (len < FST_HDR_SIM_VERSION_SIZE) ? len : FST_HDR_SIM_VERSION_SIZE);
        fstFwrite(s, FST_HDR_SIM_VERSION_SIZE, 1, xc->handle);
        fflush(xc->handle);
        fstWriterFseeko(xc, xc->handle, fpos, SEEK_SET);
        }
}


void fstWriterSetFileType(void *ctx, enum fstFileType filetype)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        if(/*(filetype >= FST_FT_MIN) &&*/ (filetype <= FST_FT_MAX))
                {
                fst_off_t fpos = ftello(xc->handle);

                xc->filetype = filetype;

                fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_FILETYPE, SEEK_SET);
                fputc(xc->filetype, xc->handle);
                fflush(xc->handle);
                fstWriterFseeko(xc, xc->handle, fpos, SEEK_SET);
                }
        }
}


static void fstWriterSetAttrDoubleArgGeneric(void *ctx, int typ, uint64_t arg1, uint64_t arg2)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        unsigned char buf[11]; /* ceil(64/7) = 10 + null term */
        unsigned char *pnt = fstCopyVarint64ToRight(buf, arg1);
        if(arg1)
                {
                *pnt = 0; /* this converts any *nonzero* arg1 when made a varint into a null-term string */
                }

        fstWriterSetAttrBegin(xc, FST_AT_MISC, typ, (char *)buf, arg2);
        }
}


static void fstWriterSetAttrGeneric(void *ctx, const char *comm, int typ, uint64_t arg)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc && comm)
        {
        char *s = strdup(comm);
        char *sf = s;

        while(*s)
                {
                if((*s == '\n') || (*s == '\r')) *s = ' ';
                s++;
                }

        fstWriterSetAttrBegin(xc, FST_AT_MISC, typ, sf, arg);
        free(sf);
        }
}


static void fstWriterSetSourceStem_2(void *ctx, const char *path, unsigned int line, unsigned int use_realpath, int typ)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

if(xc && path && path[0])
        {
        uint64_t sidx = 0;
        int slen = strlen(path);
#ifndef _WAVE_HAVE_JUDY
        const uint32_t hashmask = FST_PATH_HASHMASK;
        const unsigned char *path2 = (const unsigned char *)path;
	PPvoid_t pv;
#else
        char *path2 = (char *)alloca(slen + 1); /* judy lacks const qualifier in its JudyHSIns definition */
	PPvoid_t pv;
        strcpy(path2, path);
#endif

        pv = JudyHSIns(&(xc->path_array), path2, slen, NULL);
        if(*pv)
                {
                sidx = (intptr_t)(*pv);
                }
                else
                {
                char *rp = NULL;

                sidx = ++xc->path_array_count;
                *pv = (void *)(intptr_t)(xc->path_array_count);

                if(use_realpath)
                        {
                        rp = fstRealpath(
#ifndef _WAVE_HAVE_JUDY
                                (const char *)
#endif
                                path2, NULL);
                        }

                fstWriterSetAttrGeneric(xc, rp ? rp :
#ifndef _WAVE_HAVE_JUDY
                        (const char *)
#endif
                        path2, FST_MT_PATHNAME, sidx);

                if(rp)
                        {
                        free(rp);
                        }
                }

        fstWriterSetAttrDoubleArgGeneric(xc, typ, sidx, line);
        }
}


void fstWriterSetSourceStem(void *ctx, const char *path, unsigned int line, unsigned int use_realpath)
{
fstWriterSetSourceStem_2(ctx, path, line, use_realpath, FST_MT_SOURCESTEM);
}


void fstWriterSetSourceInstantiationStem(void *ctx, const char *path, unsigned int line, unsigned int use_realpath)
{
fstWriterSetSourceStem_2(ctx, path, line, use_realpath, FST_MT_SOURCEISTEM);
}


void fstWriterSetComment(void *ctx, const char *comm)
{
fstWriterSetAttrGeneric(ctx, comm, FST_MT_COMMENT, 0);
}


void fstWriterSetValueList(void *ctx, const char *vl)
{
fstWriterSetAttrGeneric(ctx, vl, FST_MT_VALUELIST, 0);
}


void fstWriterSetEnvVar(void *ctx, const char *envvar)
{
fstWriterSetAttrGeneric(ctx, envvar, FST_MT_ENVVAR, 0);
}


void fstWriterSetTimescale(void *ctx, int ts)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        fst_off_t fpos = ftello(xc->handle);
        fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_TIMESCALE, SEEK_SET);
        fputc(ts & 255, xc->handle);
        fflush(xc->handle);
        fstWriterFseeko(xc, xc->handle, fpos, SEEK_SET);
        }
}


void fstWriterSetTimescaleFromString(void *ctx, const char *s)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc && s)
        {
        int mat = 0;
        int seconds_exp = -9;
        int tv = atoi(s);
        const char *pnt = s;

        while(*pnt)
                {
                switch(*pnt)
                        {
                        case 'm': seconds_exp =  -3; mat = 1; break;
                        case 'u': seconds_exp =  -6; mat = 1; break;
                        case 'n': seconds_exp =  -9; mat = 1; break;
                        case 'p': seconds_exp = -12; mat = 1; break;
                        case 'f': seconds_exp = -15; mat = 1; break;
                        case 'a': seconds_exp = -18; mat = 1; break;
                        case 'z': seconds_exp = -21; mat = 1; break;
                        case 's': seconds_exp =   0; mat = 1; break;
                        default: break;
                        }

                if(mat) break;
                pnt++;
                }

        if(tv == 10)
                {
                seconds_exp++;
                }
        else
        if(tv == 100)
                {
                seconds_exp+=2;
                }

        fstWriterSetTimescale(ctx, seconds_exp);
        }
}


void fstWriterSetTimezero(void *ctx, int64_t tim)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        fst_off_t fpos = ftello(xc->handle);
        fstWriterFseeko(xc, xc->handle, FST_HDR_OFFS_TIMEZERO, SEEK_SET);
        fstWriterUint64(xc->handle, (xc->timezero = tim));
        fflush(xc->handle);
        fstWriterFseeko(xc, xc->handle, fpos, SEEK_SET);
        }
}


void fstWriterSetPackType(void *ctx, enum fstWriterPackType typ)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        xc->fastpack     = (typ != FST_WR_PT_ZLIB);
        xc->fourpack     = (typ == FST_WR_PT_LZ4);
        }
}


void fstWriterSetRepackOnClose(void *ctx, int enable)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        xc->repack_on_close = (enable != 0);
        }
}


void fstWriterSetParallelMode(void *ctx, int enable)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        xc->parallel_was_enabled |= xc->parallel_enabled; /* make sticky */
        xc->parallel_enabled = (enable != 0);
#ifndef FST_WRITER_PARALLEL
        if(xc->parallel_enabled)
                {
                fprintf(stderr, FST_APIMESS "fstWriterSetParallelMode(), FST_WRITER_PARALLEL not enabled during compile, exiting.\n");
                exit(255);
                }
#endif
        }
}


void fstWriterSetDumpSizeLimit(void *ctx, uint64_t numbytes)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        xc->dump_size_limit = numbytes;
        }
}


int fstWriterGetDumpSizeLimitReached(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        return(xc->size_limit_locked != 0);
        }

return(0);
}


int fstWriterGetFseekFailed(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc)
        {
        return(xc->fseek_failed != 0);
        }

return(0);
}


/*
 * writer attr/scope/var creation:
 * fstWriterCreateVar2() is used to dump VHDL or other languages, but the
 * underlying variable needs to map to Verilog/SV via the proper fstVarType vt
 */
fstHandle fstWriterCreateVar2(void *ctx, enum fstVarType vt, enum fstVarDir vd,
        uint32_t len, const char *nam, fstHandle aliasHandle,
        const char *type, enum fstSupplementalVarType svt, enum fstSupplementalDataType sdt)
{
fstWriterSetAttrGeneric(ctx, type ? type : "", FST_MT_SUPVAR, (svt<<FST_SDT_SVT_SHIFT_COUNT) | (sdt & FST_SDT_ABS_MAX));
return(fstWriterCreateVar(ctx, vt, vd, len, nam, aliasHandle));
}


fstHandle fstWriterCreateVar(void *ctx, enum fstVarType vt, enum fstVarDir vd,
        uint32_t len, const char *nam, fstHandle aliasHandle)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
unsigned int i;
int nlen, is_real;

if(xc && nam)
        {
        if(xc->valpos_mem)
                {
                fstDestroyMmaps(xc, 0);
                }

        fputc(vt, xc->hier_handle);
        fputc(vd, xc->hier_handle);
        nlen = strlen(nam);
        fstFwrite(nam, nlen, 1, xc->hier_handle);
        fputc(0, xc->hier_handle);
        xc->hier_file_len += (nlen+3);

        if((vt == FST_VT_VCD_REAL) || (vt == FST_VT_VCD_REAL_PARAMETER) || (vt == FST_VT_VCD_REALTIME) || (vt == FST_VT_SV_SHORTREAL))
                {
                is_real = 1;
                len = 8; /* recast number of bytes to that of what a double is */
                }
                else
                {
                is_real = 0;
                if(vt == FST_VT_GEN_STRING)
                        {
                        len = 0;
                        }
                }

        xc->hier_file_len += fstWriterVarint(xc->hier_handle, len);

        if(aliasHandle > xc->maxhandle) aliasHandle = 0;
        xc->hier_file_len += fstWriterVarint(xc->hier_handle, aliasHandle);
        xc->numsigs++;
        if(xc->numsigs == xc->next_huge_break)
                {
                if(xc->fst_break_size < xc->fst_huge_break_size)
                        {
                        xc->next_huge_break += FST_ACTIVATE_HUGE_INC;
                        xc->fst_break_size += xc->fst_orig_break_size;
                        xc->fst_break_add_size += xc->fst_orig_break_add_size;

                        xc->vchg_alloc_siz = xc->fst_break_size + xc->fst_break_add_size;
                        if(xc->vchg_mem)
                                {
                                xc->vchg_mem = (unsigned char *)realloc(xc->vchg_mem, xc->vchg_alloc_siz);
                                }
                        }
                }

        if(!aliasHandle)
                {
                uint32_t zero = 0;

                if(len)
                        {
                        fstWriterVarint(xc->geom_handle, !is_real ? len : 0); /* geom section encodes reals as zero byte */
                        }
                        else
                        {
                        fstWriterVarint(xc->geom_handle, 0xFFFFFFFF);         /* geom section encodes zero len as 32b -1 */
                        }

                fstFwrite(&xc->maxvalpos, sizeof(uint32_t), 1, xc->valpos_handle);
                fstFwrite(&len, sizeof(uint32_t), 1, xc->valpos_handle);
                fstFwrite(&zero, sizeof(uint32_t), 1, xc->valpos_handle);
                fstFwrite(&zero, sizeof(uint32_t), 1, xc->valpos_handle);

                if(!is_real)
                        {
                        for(i=0;i<len;i++)
                                {
                                fputc('x', xc->curval_handle);
                                }
                        }
                        else
                        {
                        fstFwrite(&xc->nan, 8, 1, xc->curval_handle); /* initialize doubles to NaN rather than x */
                        }

                xc->maxvalpos+=len;
                xc->maxhandle++;
                return(xc->maxhandle);
                }
                else
                {
                return(aliasHandle);
                }
        }

return(0);
}


void fstWriterSetScope(void *ctx, enum fstScopeType scopetype,
                const char *scopename, const char *scopecomp)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

if(xc)
        {
        fputc(FST_ST_VCD_SCOPE, xc->hier_handle);
        if(/*(scopetype < FST_ST_VCD_MODULE) ||*/ (scopetype > FST_ST_MAX)) { scopetype = FST_ST_VCD_MODULE; }
        fputc(scopetype, xc->hier_handle);
        fprintf(xc->hier_handle, "%s%c%s%c",
                scopename ? scopename : "", 0,
                scopecomp ? scopecomp : "", 0);

        if(scopename)
                {
                xc->hier_file_len += strlen(scopename);
                }
        if(scopecomp)
                {
                xc->hier_file_len += strlen(scopecomp);
                }

        xc->hier_file_len += 4; /* FST_ST_VCD_SCOPE + scopetype + two string terminating zeros */
        xc->numscopes++;
        }
}


void fstWriterSetUpscope(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

if(xc)
        {
        fputc(FST_ST_VCD_UPSCOPE, xc->hier_handle);
        xc->hier_file_len++;
        }
}


void fstWriterSetAttrBegin(void *ctx, enum fstAttrType attrtype, int subtype,
                const char *attrname, uint64_t arg)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

if(xc)
        {
        fputc(FST_ST_GEN_ATTRBEGIN, xc->hier_handle);
        if(/*(attrtype < FST_AT_MISC) ||*/ (attrtype > FST_AT_MAX)) { attrtype = FST_AT_MISC; subtype = FST_MT_UNKNOWN; }
        fputc(attrtype, xc->hier_handle);

        switch(attrtype)
                {
                case FST_AT_ARRAY:      if((subtype < FST_AR_NONE) || (subtype > FST_AR_MAX)) subtype = FST_AR_NONE; break;
                case FST_AT_ENUM:       if((subtype < FST_EV_SV_INTEGER) || (subtype > FST_EV_MAX)) subtype = FST_EV_SV_INTEGER; break;
                case FST_AT_PACK:       if((subtype < FST_PT_NONE) || (subtype > FST_PT_MAX)) subtype = FST_PT_NONE; break;

                case FST_AT_MISC:
                default:                break;
                }

        fputc(subtype, xc->hier_handle);
        fprintf(xc->hier_handle, "%s%c",
                attrname ? attrname : "", 0);

        if(attrname)
                {
                xc->hier_file_len += strlen(attrname);
                }

        xc->hier_file_len += 4; /* FST_ST_GEN_ATTRBEGIN + type + subtype + string terminating zero */
        xc->hier_file_len += fstWriterVarint(xc->hier_handle, arg);
        }
}


void fstWriterSetAttrEnd(void *ctx)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

if(xc)
        {
        fputc(FST_ST_GEN_ATTREND, xc->hier_handle);
        xc->hier_file_len++;
        }
}


fstEnumHandle fstWriterCreateEnumTable(void *ctx, const char *name, uint32_t elem_count, unsigned int min_valbits, const char **literal_arr, const char **val_arr)
{
fstEnumHandle handle = 0;
unsigned int *literal_lens = NULL;
unsigned int *val_lens = NULL;
int lit_len_tot = 0;
int val_len_tot = 0;
int name_len;
char elem_count_buf[16];
int elem_count_len;
int total_len;
int pos = 0;
char *attr_str = NULL;

if(ctx && name && literal_arr && val_arr && (elem_count != 0))
	{
	struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

	uint32_t i;

	name_len = strlen(name);
	elem_count_len = snprintf(elem_count_buf, 16, "%" PRIu32, elem_count);

	literal_lens = (unsigned int *)calloc(elem_count, sizeof(unsigned int));
	val_lens = (unsigned int *)calloc(elem_count, sizeof(unsigned int));

	for(i=0;i<elem_count;i++)
		{
		literal_lens[i] = strlen(literal_arr[i]);
		lit_len_tot += fstUtilityBinToEscConvertedLen((unsigned char*)literal_arr[i], literal_lens[i]);

		val_lens[i] =  strlen(val_arr[i]);
		val_len_tot += fstUtilityBinToEscConvertedLen((unsigned char*)val_arr[i], val_lens[i]);

		if(min_valbits > 0)
			{
			if(val_lens[i] < min_valbits)
				{
				val_len_tot += (min_valbits - val_lens[i]); /* additional converted len is same for '0' character */
				}
			}
		}

	total_len = name_len + 1 + elem_count_len + 1 + lit_len_tot + elem_count + val_len_tot + elem_count;

	attr_str = (char*)malloc(total_len);
	pos = 0;

	memcpy(attr_str+pos, name, name_len);
	pos += name_len;
	attr_str[pos++] = ' ';

	memcpy(attr_str+pos, elem_count_buf, elem_count_len);
	pos += elem_count_len;
	attr_str[pos++] = ' ';

	for(i=0;i<elem_count;i++)
		{
		pos += fstUtilityBinToEsc((unsigned char*)attr_str+pos, (unsigned char*)literal_arr[i], literal_lens[i]);
		attr_str[pos++] = ' ';
		}

	for(i=0;i<elem_count;i++)
		{
		if(min_valbits > 0)
			{
			if(val_lens[i] < min_valbits)
				{
				memset(attr_str+pos, '0', min_valbits - val_lens[i]);
				pos += (min_valbits - val_lens[i]);
				}
			}

		pos += fstUtilityBinToEsc((unsigned char*)attr_str+pos, (unsigned char*)val_arr[i], val_lens[i]);
		attr_str[pos++] = ' ';
		}

	attr_str[pos-1] = 0;

#ifdef FST_DEBUG
	fprintf(stderr, FST_APIMESS "fstWriterCreateEnumTable() total_len: %d, pos: %d\n", total_len, pos);
	fprintf(stderr, FST_APIMESS "*%s*\n", attr_str);
#endif

	fstWriterSetAttrBegin(xc, FST_AT_MISC, FST_MT_ENUMTABLE, attr_str, handle = ++xc->max_enumhandle);

	free(attr_str);
	free(val_lens);
	free(literal_lens);
	}

return(handle);
}


void fstWriterEmitEnumTableRef(void *ctx, fstEnumHandle handle)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
if(xc && handle)
	{
	fstWriterSetAttrBegin(xc, FST_AT_MISC, FST_MT_ENUMTABLE, NULL, handle);
        }
}


/*
 * value and time change emission
 */
void fstWriterEmitValueChange(void *ctx, fstHandle handle, const void *val)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
const unsigned char *buf = (const unsigned char *)val;
uint32_t offs;
int len;

if(FST_LIKELY((xc) && (handle <= xc->maxhandle)))
        {
        uint32_t fpos;
        uint32_t *vm4ip;

        if(FST_UNLIKELY(!xc->valpos_mem))
                {
                xc->vc_emitted = 1;
                fstWriterCreateMmaps(xc);
                }

        handle--; /* move starting at 1 index to starting at 0 */
        vm4ip = &(xc->valpos_mem[4*handle]);

        len  = vm4ip[1];
        if(FST_LIKELY(len)) /* len of zero = variable length, use fstWriterEmitVariableLengthValueChange */
                {
                if(FST_LIKELY(!xc->is_initial_time))
                        {
                        fpos = xc->vchg_siz;

                        if(FST_UNLIKELY((fpos + len + 10) > xc->vchg_alloc_siz))
                                {
                                xc->vchg_alloc_siz += (xc->fst_break_add_size + len); /* +len added in the case of extremely long vectors and small break add sizes */
                                xc->vchg_mem = (unsigned char *)realloc(xc->vchg_mem, xc->vchg_alloc_siz);
                                if(FST_UNLIKELY(!xc->vchg_mem))
                                        {
                                        fprintf(stderr, FST_APIMESS "Could not realloc() in fstWriterEmitValueChange, exiting.\n");
                                        exit(255);
                                        }
                                }
#ifdef FST_REMOVE_DUPLICATE_VC
                        offs = vm4ip[0];

                        if(len != 1)
                                {
                                if((vm4ip[3]==xc->tchn_idx)&&(vm4ip[2]))
                                        {
                                        unsigned char *old_value = xc->vchg_mem + vm4ip[2] + 4; /* the +4 skips old vm4ip[2] value */
                                        while(*(old_value++) & 0x80) { /* skips over varint encoded "xc->tchn_idx - vm4ip[3]" */ }
                                        memcpy(old_value, buf, len); /* overlay new value */

                                        memcpy(xc->curval_mem + offs, buf, len);
                                        return;
                                        }
                                else
                                        {
                                        if(!memcmp(xc->curval_mem + offs, buf, len))
                                                {
                                                if(!xc->curtime)
                                                        {
                                                        int i;
                                                        for(i=0;i<len;i++)
                                                                {
                                                                if(buf[i]!='x') break;
                                                                }

                                                        if(i<len) return;
                                                        }
                                                        else
                                                        {
                                                        return;
                                                        }
                                                }
                                        }

                                memcpy(xc->curval_mem + offs, buf, len);
                                }
                                else
                                {
                                if((vm4ip[3]==xc->tchn_idx)&&(vm4ip[2]))
                                        {
                                        unsigned char *old_value = xc->vchg_mem + vm4ip[2] + 4; /* the +4 skips old vm4ip[2] value */
                                        while(*(old_value++) & 0x80) { /* skips over varint encoded "xc->tchn_idx - vm4ip[3]" */ }
                                        *old_value = *buf; /* overlay new value */

                                        *(xc->curval_mem + offs) = *buf;
                                        return;
                                        }
                                else
                                        {
                                        if((*(xc->curval_mem + offs)) == (*buf))
                                                {
                                                if(!xc->curtime)
                                                        {
                                                        if(*buf != 'x') return;
                                                        }
                                                        else
                                                        {
                                                        return;
                                                        }
                                                }
                                        }

                                *(xc->curval_mem + offs) = *buf;
                                }
#endif
                        xc->vchg_siz += fstWriterUint32WithVarint32(xc, &vm4ip[2], xc->tchn_idx - vm4ip[3], buf, len); /* do one fwrite op only */
                        vm4ip[3] = xc->tchn_idx;
                        vm4ip[2] = fpos;
                        }
                        else
                        {
                        offs = vm4ip[0];
                        memcpy(xc->curval_mem + offs, buf, len);
                        }
                }
        }
}

void fstWriterEmitValueChange32(void *ctx, fstHandle handle,
                                uint32_t bits, uint32_t val) {
        char buf[32];
        char *s = buf;
        uint32_t i;
        for (i = 0; i < bits; ++i)
        {
                *s++ = '0' + ((val >> (bits - i - 1)) & 1);
        }
        fstWriterEmitValueChange(ctx, handle, buf);
}
void fstWriterEmitValueChange64(void *ctx, fstHandle handle,
                                uint32_t bits, uint64_t val) {
        char buf[64];
        char *s = buf;
        uint32_t i;
        for (i = 0; i < bits; ++i)
        {
                *s++ = '0' + ((val >> (bits - i - 1)) & 1);
        }
        fstWriterEmitValueChange(ctx, handle, buf);
}
void fstWriterEmitValueChangeVec32(void *ctx, fstHandle handle,
                                   uint32_t bits, const uint32_t *val) {
        struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
        if (FST_UNLIKELY(bits <= 32))
        {
                fstWriterEmitValueChange32(ctx, handle, bits, val[0]);
        }
        else if(FST_LIKELY(xc))
        {
                int bq = bits / 32;
                int br = bits & 31;
                int i;
                int w;
                uint32_t v;
                unsigned char* s;
                if (FST_UNLIKELY(bits > xc->outval_alloc_siz))
                {
                        xc->outval_alloc_siz = bits*2 + 1;
                        xc->outval_mem = (unsigned char*)realloc(xc->outval_mem, xc->outval_alloc_siz);
                        if (FST_UNLIKELY(!xc->outval_mem))
                        {
                                fprintf(stderr,
                                        FST_APIMESS "Could not realloc() in fstWriterEmitValueChangeVec32, exiting.\n");
                                exit(255);
                        }
                }
                s = xc->outval_mem;
                {
                        w = bq;
                        v = val[w];
                        for (i = 0; i < br; ++i)
                        {
                                *s++ = '0' + ((v >> (br - i - 1)) & 1);
                        }
                }
                for (w = bq - 1; w >= 0; --w)
                {
                        v = val[w];
                        for (i = (32 - 4); i >= 0; i -= 4) {
                                s[0] = '0' + ((v >> (i + 3)) & 1);
                                s[1] = '0' + ((v >> (i + 2)) & 1);
                                s[2] = '0' + ((v >> (i + 1)) & 1);
                                s[3] = '0' + ((v >> (i + 0)) & 1);
                                s += 4;
                        }
                }
                fstWriterEmitValueChange(ctx, handle, xc->outval_mem);
        }
}
void fstWriterEmitValueChangeVec64(void *ctx, fstHandle handle,
                                   uint32_t bits, const uint64_t *val) {
        struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
        if (FST_UNLIKELY(bits <= 64))
        {
                fstWriterEmitValueChange64(ctx, handle, bits, val[0]);
        }
        else if(FST_LIKELY(xc))
        {
                int bq = bits / 64;
                int br = bits & 63;
                int i;
                int w;
                uint32_t v;
                unsigned char* s;
                if (FST_UNLIKELY(bits > xc->outval_alloc_siz))
                {
                        xc->outval_alloc_siz = bits*2 + 1;
                        xc->outval_mem = (unsigned char*)realloc(xc->outval_mem, xc->outval_alloc_siz);
                        if (FST_UNLIKELY(!xc->outval_mem))
                        {
                                fprintf(stderr,
                                        FST_APIMESS "Could not realloc() in fstWriterEmitValueChangeVec64, exiting.\n");
                                exit(255);
                        }
                }
                s = xc->outval_mem;
                {
                        w = bq;
                        v = val[w];
                        for (i = 0; i < br; ++i)
                        {
                                *s++ = '0' + ((v >> (br - i - 1)) & 1);
                        }
                }
                for (w = bq - 1; w >= 0; --w) {
                        v = val[w];
                        for (i = (64 - 4); i >= 0; i -= 4)
                        {
                                s[0] = '0' + ((v >> (i + 3)) & 1);
                                s[1] = '0' + ((v >> (i + 2)) & 1);
                                s[2] = '0' + ((v >> (i + 1)) & 1);
                                s[3] = '0' + ((v >> (i + 0)) & 1);
                                s += 4;
                        }
                }
                fstWriterEmitValueChange(ctx, handle, xc->outval_mem);
        }
}


void fstWriterEmitVariableLengthValueChange(void *ctx, fstHandle handle, const void *val, uint32_t len)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
const unsigned char *buf = (const unsigned char *)val;

if(FST_LIKELY((xc) && (handle <= xc->maxhandle)))
        {
        uint32_t fpos;
        uint32_t *vm4ip;

        if(FST_UNLIKELY(!xc->valpos_mem))
                {
                xc->vc_emitted = 1;
                fstWriterCreateMmaps(xc);
                }

        handle--; /* move starting at 1 index to starting at 0 */
        vm4ip = &(xc->valpos_mem[4*handle]);

        /* there is no initial time dump for variable length value changes */
        if(FST_LIKELY(!vm4ip[1])) /* len of zero = variable length */
                {
                fpos = xc->vchg_siz;

                if(FST_UNLIKELY((fpos + len + 10 + 5) > xc->vchg_alloc_siz))
                        {
                        xc->vchg_alloc_siz += (xc->fst_break_add_size + len + 5); /* +len added in the case of extremely long vectors and small break add sizes */
                        xc->vchg_mem = (unsigned char *)realloc(xc->vchg_mem, xc->vchg_alloc_siz);
                        if(FST_UNLIKELY(!xc->vchg_mem))
                                {
                                fprintf(stderr, FST_APIMESS "Could not realloc() in fstWriterEmitVariableLengthValueChange, exiting.\n");
                                exit(255);
                                }
                        }

                xc->vchg_siz += fstWriterUint32WithVarint32AndLength(xc, &vm4ip[2], xc->tchn_idx - vm4ip[3], buf, len); /* do one fwrite op only */
                vm4ip[3] = xc->tchn_idx;
                vm4ip[2] = fpos;
                }
        }
}


void fstWriterEmitTimeChange(void *ctx, uint64_t tim)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;
unsigned int i;
int skip = 0;
if(xc)
        {
        if(FST_UNLIKELY(xc->is_initial_time))
                {
                if(xc->size_limit_locked)       /* this resets xc->is_initial_time to one */
                        {
                        return;
                        }

                if(!xc->valpos_mem)
                        {
                        fstWriterCreateMmaps(xc);
                        }

                skip = 1;

                xc->firsttime = (xc->vc_emitted) ? 0: tim;
                xc->curtime = 0;
                xc->vchg_mem[0] = '!';
                xc->vchg_siz = 1;
                fstWriterEmitSectionHeader(xc);
                for(i=0;i<xc->maxhandle;i++)
                        {
                        xc->valpos_mem[4*i+2] = 0; /* zero out offset val */
                        xc->valpos_mem[4*i+3] = 0; /* zero out last time change val */
                        }
                xc->is_initial_time = 0;
                }
                else
                {
                if((xc->vchg_siz >= xc->fst_break_size) || (xc->flush_context_pending))
                        {
                        xc->flush_context_pending = 0;
                        fstWriterFlushContextPrivate(xc);
                        xc->tchn_cnt++;
                        fstWriterVarint(xc->tchn_handle, xc->curtime);
                        }
                }

        if(!skip)
                {
                xc->tchn_idx++;
                }
        fstWriterVarint(xc->tchn_handle, tim - xc->curtime);
        xc->tchn_cnt++;
        xc->curtime = tim;
        }
}


void fstWriterEmitDumpActive(void *ctx, int enable)
{
struct fstWriterContext *xc = (struct fstWriterContext *)ctx;

if(xc)
        {
        struct fstBlackoutChain *b = (struct fstBlackoutChain *)calloc(1, sizeof(struct fstBlackoutChain));

        b->tim = xc->curtime;
        b->active = (enable != 0);

        xc->num_blackouts++;
        if(xc->blackout_curr)
                {
                xc->blackout_curr->next = b;
                xc->blackout_curr = b;
                }
                else
                {
                xc->blackout_head = b;
                xc->blackout_curr = b;
                }
        }
}


/***********************/
/***                 ***/
/*** reader function ***/
/***                 ***/
/***********************/

/*
 * private structs
 */
static const char *vartypes[] = {
        "event", "integer", "parameter", "real", "real_parameter",
        "reg", "supply0", "supply1", "time", "tri",
        "triand", "trior", "trireg", "tri0", "tri1",
        "wand", "wire", "wor", "port", "sparray", "realtime",
        "string",
        "bit", "logic", "int", "shortint", "longint", "byte", "enum", "shortreal"
        };

static const char *modtypes[] = {
        "module", "task", "function", "begin", "fork", "generate", "struct", "union", "class", "interface", "package", "program",
        "vhdl_architecture", "vhdl_procedure", "vhdl_function", "vhdl_record", "vhdl_process", "vhdl_block", "vhdl_for_generate", "vhdl_if_generate", "vhdl_generate", "vhdl_package"
        };

static const char *attrtypes[] = {
        "misc", "array", "enum", "class"
        };

static const char *arraytypes[] = {
        "none", "unpacked", "packed", "sparse"
        };

static const char *enumvaluetypes[] = {
        "integer", "bit", "logic", "int", "shortint", "longint", "byte",
        "unsigned_integer", "unsigned_bit", "unsigned_logic", "unsigned_int", "unsigned_shortint", "unsigned_longint", "unsigned_byte"
        };

static const char *packtypes[] = {
        "none", "unpacked", "packed", "tagged_packed"
        };


struct fstCurrHier
{
struct fstCurrHier *prev;
void *user_info;
int len;
};


struct fstReaderContext
{
/* common entries */

FILE *f, *fh;

uint64_t start_time, end_time;
uint64_t mem_used_by_writer;
uint64_t scope_count;
uint64_t var_count;
fstHandle maxhandle;
uint64_t num_alias;
uint64_t vc_section_count;

uint32_t *signal_lens;                  /* maxhandle sized */
unsigned char *signal_typs;             /* maxhandle sized */
unsigned char *process_mask;            /* maxhandle-based, bitwise sized */
uint32_t longest_signal_value_len;      /* longest len value encountered */
unsigned char *temp_signal_value_buf;   /* malloced for len in longest_signal_value_len */

signed char timescale;
unsigned char filetype;

unsigned use_vcd_extensions : 1;
unsigned double_endian_match : 1;
unsigned native_doubles_for_cb : 1;
unsigned contains_geom_section : 1;
unsigned contains_hier_section : 1;        /* valid for hier_pos */
unsigned contains_hier_section_lz4duo : 1; /* valid for hier_pos (contains_hier_section_lz4 always also set) */
unsigned contains_hier_section_lz4 : 1;    /* valid for hier_pos */
unsigned limit_range_valid : 1;            /* valid for limit_range_start, limit_range_end */

char version[FST_HDR_SIM_VERSION_SIZE + 1];
char date[FST_HDR_DATE_SIZE + 1];
int64_t timezero;

char *filename, *filename_unpacked;
fst_off_t hier_pos;

uint32_t num_blackouts;
uint64_t *blackout_times;
unsigned char *blackout_activity;

uint64_t limit_range_start, limit_range_end;

/* entries specific to read value at time functions */

unsigned rvat_data_valid : 1;
uint64_t *rvat_time_table;
uint64_t rvat_beg_tim, rvat_end_tim;
unsigned char *rvat_frame_data;
uint64_t rvat_frame_maxhandle;
fst_off_t *rvat_chain_table;
uint32_t *rvat_chain_table_lengths;
uint64_t rvat_vc_maxhandle;
fst_off_t rvat_vc_start;
uint32_t *rvat_sig_offs;
int rvat_packtype;

uint32_t rvat_chain_len;
unsigned char *rvat_chain_mem;
fstHandle rvat_chain_facidx;

uint32_t rvat_chain_pos_tidx;
uint32_t rvat_chain_pos_idx;
uint64_t rvat_chain_pos_time;
unsigned rvat_chain_pos_valid : 1;

/* entries specific to hierarchy traversal */

struct fstHier hier;
struct fstCurrHier *curr_hier;
fstHandle current_handle;
char *curr_flat_hier_nam;
int flat_hier_alloc_len;
unsigned do_rewind : 1;
char str_scope_nam[FST_ID_NAM_SIZ+1];
char str_scope_comp[FST_ID_NAM_SIZ+1];
char *str_scope_attr;

unsigned fseek_failed : 1;

/* self-buffered I/O for writes */

#ifndef FST_WRITEX_DISABLE
int writex_pos;
int writex_fd;
unsigned char writex_buf[FST_WRITEX_MAX];
#endif

char *f_nam;
char *fh_nam;
};


int fstReaderFseeko(struct fstReaderContext *xc, FILE *stream, fst_off_t offset, int whence)
{
int rc = fseeko(stream, offset, whence);

if(rc<0)
        {
        xc->fseek_failed = 1;
#ifdef FST_DEBUG
        fprintf(stderr, FST_APIMESS "Seek to #%" PRId64 " (whence = %d) failed!\n", offset, whence);
        perror("Why");
#endif
        }

return(rc);
}


#ifndef FST_WRITEX_DISABLE
static void fstWritex(struct fstReaderContext *xc, void *v, uint32_t len) /* TALOS-2023-1793: change len to unsigned */
{
unsigned char *s = (unsigned char *)v;

if(len)
        {
        if(len < FST_WRITEX_MAX)
                {
                if(xc->writex_pos + len >= FST_WRITEX_MAX)
                        {
                        fstWritex(xc, NULL, 0);
                        }

                memcpy(xc->writex_buf + xc->writex_pos, s, len);
                xc->writex_pos += len;
                }
                else
                {
                fstWritex(xc, NULL, 0);
                if (write(xc->writex_fd, s, len)) { };
                }
        }
        else
        {
        if(xc->writex_pos)
                {
                if(write(xc->writex_fd, xc->writex_buf, xc->writex_pos)) { };
                xc->writex_pos = 0;
                }
        }
}
#endif


/*
 * scope -> flat name handling
 */
static void fstReaderDeallocateScopeData(struct fstReaderContext *xc)
{
struct fstCurrHier *chp;

free(xc->curr_flat_hier_nam); xc->curr_flat_hier_nam = NULL;
while(xc->curr_hier)
        {
        chp = xc->curr_hier->prev;
        free(xc->curr_hier);
        xc->curr_hier = chp;
        }
}


const char *fstReaderGetCurrentFlatScope(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
if(xc)
        {
        return(xc->curr_flat_hier_nam ? xc->curr_flat_hier_nam : "");
        }
        else
        {
        return(NULL);
        }
}


void *fstReaderGetCurrentScopeUserInfo(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
if(xc)
        {
        return(xc->curr_hier ? xc->curr_hier->user_info : NULL);
        }
        else
        {
        return(NULL);
        }
}


const char *fstReaderPopScope(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
if(xc && xc->curr_hier)
        {
        struct fstCurrHier *ch = xc->curr_hier;
        if(xc->curr_hier->prev)
                {
                xc->curr_flat_hier_nam[xc->curr_hier->prev->len] = 0;
                }
                else
                {
                *xc->curr_flat_hier_nam = 0;
                }
        xc->curr_hier = xc->curr_hier->prev;
        free(ch);
        return(xc->curr_flat_hier_nam ? xc->curr_flat_hier_nam : "");
        }

return(NULL);
}


void fstReaderResetScope(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        while(fstReaderPopScope(xc)); /* remove any already-built scoping info */
        }
}


const char *fstReaderPushScope(void *ctx, const char *nam, void *user_info)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
if(xc)
        {
        struct fstCurrHier *ch = (struct fstCurrHier *)malloc(sizeof(struct fstCurrHier));
        int chl = xc->curr_hier ? xc->curr_hier->len : 0;
        int len = chl + 1 + strlen(nam);
        if(len >= xc->flat_hier_alloc_len)
                {
                xc->curr_flat_hier_nam = xc->curr_flat_hier_nam ? (char *)realloc(xc->curr_flat_hier_nam, len+1) : (char *)malloc(len+1);
                }

        if(chl)
                {
                xc->curr_flat_hier_nam[chl] = '.';
                strcpy(xc->curr_flat_hier_nam + chl + 1, nam);
                }
                else
                {
                strcpy(xc->curr_flat_hier_nam, nam);
                len--;
                }

        ch->len = len;
        ch->prev = xc->curr_hier;
        ch->user_info = user_info;
        xc->curr_hier = ch;
        return(xc->curr_flat_hier_nam);
        }

return(NULL);
}


int fstReaderGetCurrentScopeLen(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc && xc->curr_hier)
        {
        return(xc->curr_hier->len);
        }

return(0);
}


int fstReaderGetFseekFailed(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
if(xc)
        {
        return(xc->fseek_failed != 0);
        }

return(0);
}


/*
 * iter mask manipulation util functions
 */
int fstReaderGetFacProcessMask(void *ctx, fstHandle facidx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        facidx--;
        if(facidx<xc->maxhandle)
                {
                int process_idx = facidx/8;
                int process_bit = facidx&7;

                return( (xc->process_mask[process_idx]&(1<<process_bit)) != 0 );
                }
        }
return(0);
}


void fstReaderSetFacProcessMask(void *ctx, fstHandle facidx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        facidx--;
        if(facidx<xc->maxhandle)
                {
                int idx = facidx/8;
                int bitpos = facidx&7;

                xc->process_mask[idx] |= (1<<bitpos);
                }
        }
}


void fstReaderClrFacProcessMask(void *ctx, fstHandle facidx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        facidx--;
        if(facidx<xc->maxhandle)
                {
                int idx = facidx/8;
                int bitpos = facidx&7;

                xc->process_mask[idx] &= (~(1<<bitpos));
                }
        }
}


void fstReaderSetFacProcessMaskAll(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        memset(xc->process_mask, 0xff, (xc->maxhandle+7)/8);
        }
}


void fstReaderClrFacProcessMaskAll(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        memset(xc->process_mask, 0x00, (xc->maxhandle+7)/8);
        }
}


/*
 * various utility read/write functions
 */
signed char fstReaderGetTimescale(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->timescale : 0);
}


uint64_t fstReaderGetStartTime(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->start_time : 0);
}


uint64_t fstReaderGetEndTime(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->end_time : 0);
}


uint64_t fstReaderGetMemoryUsedByWriter(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->mem_used_by_writer : 0);
}


uint64_t fstReaderGetScopeCount(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->scope_count : 0);
}


uint64_t fstReaderGetVarCount(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->var_count : 0);
}


fstHandle fstReaderGetMaxHandle(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->maxhandle : 0);
}


uint64_t fstReaderGetAliasCount(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->num_alias : 0);
}


uint64_t fstReaderGetValueChangeSectionCount(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->vc_section_count : 0);
}


int fstReaderGetDoubleEndianMatchState(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->double_endian_match : 0);
}


const char *fstReaderGetVersionString(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->version : NULL);
}


const char *fstReaderGetDateString(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->date : NULL);
}


int fstReaderGetFileType(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? (int)xc->filetype : (int)FST_FT_VERILOG);
}


int64_t fstReaderGetTimezero(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->timezero : 0);
}


uint32_t fstReaderGetNumberDumpActivityChanges(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
return(xc ? xc->num_blackouts : 0);
}


uint64_t fstReaderGetDumpActivityChangeTime(void *ctx, uint32_t idx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc && (idx < xc->num_blackouts) && (xc->blackout_times))
        {
        return(xc->blackout_times[idx]);
        }
        else
        {
        return(0);
        }
}


unsigned char fstReaderGetDumpActivityChangeValue(void *ctx, uint32_t idx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc && (idx < xc->num_blackouts) && (xc->blackout_activity))
        {
        return(xc->blackout_activity[idx]);
        }
        else
        {
        return(0);
        }
}


void fstReaderSetLimitTimeRange(void *ctx, uint64_t start_time, uint64_t end_time)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        xc->limit_range_valid = 1;
        xc->limit_range_start = start_time;
        xc->limit_range_end = end_time;
        }
}


void fstReaderSetUnlimitedTimeRange(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        xc->limit_range_valid = 0;
        }
}


void fstReaderSetVcdExtensions(void *ctx, int enable)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        xc->use_vcd_extensions = (enable != 0);
        }
}


void fstReaderIterBlocksSetNativeDoublesOnCallback(void *ctx, int enable)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
if(xc)
        {
        xc->native_doubles_for_cb = (enable != 0);
        }
}

/*
 * hierarchy processing
 */
static void fstVcdID(char *buf, unsigned int value)
{
char *pnt = buf;

/* zero is illegal for a value...it is assumed they start at one */
while (value)
        {
        value--;
        *(pnt++) = (char)('!' + value % 94);
        value = value / 94;
        }

*pnt = 0;
}

static int fstVcdIDForFwrite(char *buf, unsigned int value)
{
char *pnt = buf;
 int len = 0;

/* zero is illegal for a value...it is assumed they start at one */
while (value && len < 14)
        {
        value--;
	++len;
        *(pnt++) = (char)('!' + value % 94);
        value = value / 94;
        }

return len;
}


static int fstReaderRecreateHierFile(struct fstReaderContext *xc)
{
int pass_status = 1;

if(!xc->fh)
        {
        fst_off_t offs_cache = ftello(xc->f);
	int fnam_len = strlen(xc->filename) + 6 + 16 + 32 + 1;
        char *fnam = (char *)malloc(fnam_len);
        unsigned char *mem = (unsigned char *)malloc(FST_GZIO_LEN);
        fst_off_t hl, uclen;
        fst_off_t clen = 0;
        gzFile zhandle = NULL;
        int zfd;
        int htyp = FST_BL_SKIP;

        /* can't handle both set at once should never happen in a real file */
        if(!xc->contains_hier_section_lz4 && xc->contains_hier_section)
                {
                htyp = FST_BL_HIER;
                }
        else
        if(xc->contains_hier_section_lz4 && !xc->contains_hier_section)
                {
                htyp = xc->contains_hier_section_lz4duo ? FST_BL_HIER_LZ4DUO : FST_BL_HIER_LZ4;
                }

        snprintf(fnam, fnam_len, "%s.hier_%d_%p", xc->filename, getpid(), (void *)xc);
        fstReaderFseeko(xc, xc->f, xc->hier_pos, SEEK_SET);
        uclen = fstReaderUint64(xc->f);
#ifndef __MINGW32__
        fflush(xc->f);
#endif
        if(htyp == FST_BL_HIER)
                {
                fstReaderFseeko(xc, xc->f, xc->hier_pos, SEEK_SET);
                uclen = fstReaderUint64(xc->f);
#ifndef __MINGW32__
                fflush(xc->f);
#endif
                zfd = dup(fileno(xc->f));
                zhandle = gzdopen(zfd, "rb");
                if(!zhandle)
                        {
                        close(zfd);
                        free(mem);
                        free(fnam);
                        return(0);
                        }
                }
        else
        if((htyp == FST_BL_HIER_LZ4) || (htyp == FST_BL_HIER_LZ4DUO))
                {
                fstReaderFseeko(xc, xc->f, xc->hier_pos - 8, SEEK_SET); /* get section len */
                clen =  fstReaderUint64(xc->f) - 16;
                uclen = fstReaderUint64(xc->f);
#ifndef __MINGW32__
                fflush(xc->f);
#endif
                }

#ifndef __MINGW32__
        xc->fh = fopen(fnam, "w+b");
        if(!xc->fh)
#endif
                {
                xc->fh = tmpfile_open(&xc->fh_nam);
                free(fnam); fnam = NULL;
                if(!xc->fh)
                        {
                        tmpfile_close(&xc->fh, &xc->fh_nam);
                        free(mem);
                        return(0);
                        }
                }

#ifndef __MINGW32__
        if(fnam) unlink(fnam);
#endif

        if(htyp == FST_BL_HIER)
                {
                for(hl = 0; hl < uclen; hl += FST_GZIO_LEN)
                        {
                        size_t len = ((uclen - hl) > FST_GZIO_LEN) ? FST_GZIO_LEN : (uclen - hl);
                        size_t gzreadlen = gzread(zhandle, mem, len); /* rc should equal len... */
                        size_t fwlen;

                        if(gzreadlen != len)
                                {
                                pass_status = 0;
                                break;
                                }

                        fwlen = fstFwrite(mem, len, 1, xc->fh);
                        if(fwlen != 1)
                                {
                                pass_status = 0;
                                break;
                                }
                        }
                gzclose(zhandle);
                }
        else
        if(htyp == FST_BL_HIER_LZ4DUO)
                {
                unsigned char *lz4_cmem  = (unsigned char *)malloc(clen);
                unsigned char *lz4_ucmem = (unsigned char *)malloc(uclen);
                unsigned char *lz4_ucmem2;
                uint64_t uclen2;
                int skiplen2 = 0;

                fstFread(lz4_cmem, clen, 1, xc->f);

                uclen2 = fstGetVarint64(lz4_cmem, &skiplen2);
                lz4_ucmem2 = (unsigned char *)malloc(uclen2);
                pass_status = (uclen2 == (uint64_t)LZ4_decompress_safe_partial ((char *)lz4_cmem + skiplen2, (char *)lz4_ucmem2, clen - skiplen2, uclen2, uclen2));
                if(pass_status)
                        {
                        pass_status = (uclen == LZ4_decompress_safe_partial ((char *)lz4_ucmem2, (char *)lz4_ucmem, uclen2, uclen, uclen));

                        if(fstFwrite(lz4_ucmem, uclen, 1, xc->fh) != 1)
                                {
                                pass_status = 0;
                                }
                        }

                free(lz4_ucmem2);
                free(lz4_ucmem);
                free(lz4_cmem);
                }
        else
        if(htyp == FST_BL_HIER_LZ4)
                {
                unsigned char *lz4_cmem  = (unsigned char *)malloc(clen);
                unsigned char *lz4_ucmem = (unsigned char *)malloc(uclen);

                fstFread(lz4_cmem, clen, 1, xc->f);
                pass_status = (uclen == LZ4_decompress_safe_partial ((char *)lz4_cmem, (char *)lz4_ucmem, clen, uclen, uclen));

                if(fstFwrite(lz4_ucmem, uclen, 1, xc->fh) != 1)
                        {
                        pass_status = 0;
                        }

                free(lz4_ucmem);
                free(lz4_cmem);
                }
        else /* FST_BL_SKIP */
                {
                pass_status = 0;
                if(xc->fh)
                        {
                        fclose(xc->fh); xc->fh = NULL; /* needed in case .hier file is missing and there are no hier sections */
                        }
                }

        free(mem);
        free(fnam);

        fstReaderFseeko(xc, xc->f, offs_cache, SEEK_SET);
        }

return(pass_status);
}


int fstReaderIterateHierRewind(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
int pass_status = 0;

if(xc)
        {
        pass_status = 1;
        if(!xc->fh)
                {
                pass_status = fstReaderRecreateHierFile(xc);
                }

        xc->do_rewind = 1;
        }

return(pass_status);
}


struct fstHier *fstReaderIterateHier(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
int isfeof;
fstHandle alias;
char *pnt;
int ch;

if(!xc) return(NULL);

if(!xc->fh)
        {
        if(!fstReaderRecreateHierFile(xc))
                {
                return(NULL);
                }
        }

if(xc->do_rewind)
        {
        xc->do_rewind = 0;
        xc->current_handle = 0;
        fstReaderFseeko(xc, xc->fh, 0, SEEK_SET);
        clearerr(xc->fh);
        }

if(!(isfeof=feof(xc->fh)))
        {
        int tag = fgetc(xc->fh);
	int cl;
        switch(tag)
                {
                case FST_ST_VCD_SCOPE:
                        xc->hier.htyp = FST_HT_SCOPE;
                        xc->hier.u.scope.typ = fgetc(xc->fh);
                        xc->hier.u.scope.name = pnt = xc->str_scope_nam;
			cl = 0;
                        while((ch = fgetc(xc->fh)))
                                {
				if(cl < FST_ID_NAM_SIZ)
					{
					pnt[cl++] = ch;
					}
                                }; /* scopename */
                        pnt[cl] = 0;
                        xc->hier.u.scope.name_length = cl;

                        xc->hier.u.scope.component = pnt = xc->str_scope_comp;
			cl = 0;
                        while((ch = fgetc(xc->fh)))
                                {
				if(cl < FST_ID_NAM_SIZ)
					{
					pnt[cl++] = ch;
					}
                                }; /* scopecomp */
                        pnt[cl] = 0;
                        xc->hier.u.scope.component_length = cl;
                        break;

                case FST_ST_VCD_UPSCOPE:
                        xc->hier.htyp = FST_HT_UPSCOPE;
                        break;

                case FST_ST_GEN_ATTRBEGIN:
                        xc->hier.htyp = FST_HT_ATTRBEGIN;
                        xc->hier.u.attr.typ = fgetc(xc->fh);
                        xc->hier.u.attr.subtype = fgetc(xc->fh);
			if(!xc->str_scope_attr)
				{
				xc->str_scope_attr = (char *)calloc(1, FST_ID_NAM_ATTR_SIZ+1);
				}
                        xc->hier.u.attr.name = pnt = xc->str_scope_attr;
			cl = 0;
                        while((ch = fgetc(xc->fh)))
                                {
				if(cl < FST_ID_NAM_ATTR_SIZ)
					{
					pnt[cl++] = ch;
					}
                                }; /* attrname */
			pnt[cl] = 0;
                        xc->hier.u.attr.name_length = cl;

                        xc->hier.u.attr.arg = fstReaderVarint64(xc->fh);

                        if(xc->hier.u.attr.typ == FST_AT_MISC)
                                {
                                if((xc->hier.u.attr.subtype == FST_MT_SOURCESTEM)||(xc->hier.u.attr.subtype == FST_MT_SOURCEISTEM))
                                        {
                                        int sidx_skiplen_dummy = 0;
                                        xc->hier.u.attr.arg_from_name = fstGetVarint64((unsigned char *)xc->str_scope_attr, &sidx_skiplen_dummy);
                                        }
                                }
                        break;

                case FST_ST_GEN_ATTREND:
                        xc->hier.htyp = FST_HT_ATTREND;
                        break;

                case FST_VT_VCD_EVENT:
                case FST_VT_VCD_INTEGER:
                case FST_VT_VCD_PARAMETER:
                case FST_VT_VCD_REAL:
                case FST_VT_VCD_REAL_PARAMETER:
                case FST_VT_VCD_REG:
                case FST_VT_VCD_SUPPLY0:
                case FST_VT_VCD_SUPPLY1:
                case FST_VT_VCD_TIME:
                case FST_VT_VCD_TRI:
                case FST_VT_VCD_TRIAND:
                case FST_VT_VCD_TRIOR:
                case FST_VT_VCD_TRIREG:
                case FST_VT_VCD_TRI0:
                case FST_VT_VCD_TRI1:
                case FST_VT_VCD_WAND:
                case FST_VT_VCD_WIRE:
                case FST_VT_VCD_WOR:
                case FST_VT_VCD_PORT:
                case FST_VT_VCD_SPARRAY:
                case FST_VT_VCD_REALTIME:
                case FST_VT_GEN_STRING:
                case FST_VT_SV_BIT:
                case FST_VT_SV_LOGIC:
                case FST_VT_SV_INT:
                case FST_VT_SV_SHORTINT:
                case FST_VT_SV_LONGINT:
                case FST_VT_SV_BYTE:
                case FST_VT_SV_ENUM:
                case FST_VT_SV_SHORTREAL:
                        xc->hier.htyp = FST_HT_VAR;
                        xc->hier.u.var.svt_workspace = FST_SVT_NONE;
                        xc->hier.u.var.sdt_workspace = FST_SDT_NONE;
                        xc->hier.u.var.sxt_workspace = 0;
                        xc->hier.u.var.typ = tag;
                        xc->hier.u.var.direction = fgetc(xc->fh);
                        xc->hier.u.var.name = pnt = xc->str_scope_nam;
			cl = 0;
                        while((ch = fgetc(xc->fh)))
                                {
				if(cl < FST_ID_NAM_SIZ)
					{
					pnt[cl++] = ch;
					}
                                }; /* varname */
                        pnt[cl] = 0;
                        xc->hier.u.var.name_length = cl;
                        xc->hier.u.var.length = fstReaderVarint32(xc->fh);
                        if(tag == FST_VT_VCD_PORT)
                                {
                                xc->hier.u.var.length -= 2; /* removal of delimiting spaces */
                                xc->hier.u.var.length /= 3; /* port -> signal size adjust */
                                }

                        alias = fstReaderVarint32(xc->fh);

                        if(!alias)
                                {
                                xc->current_handle++;
                                xc->hier.u.var.handle = xc->current_handle;
                                xc->hier.u.var.is_alias = 0;
                                }
                                else
                                {
                                xc->hier.u.var.handle = alias;
                                xc->hier.u.var.is_alias = 1;
                                }

                        break;

                default:
                        isfeof = 1;
                        break;
                }
        }

return(!isfeof ? &xc->hier : NULL);
}


int fstReaderProcessHier(void *ctx, FILE *fv)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
char *str;
char *pnt;
int ch, scopetype;
int vartype;
uint32_t len, alias;
/* uint32_t maxvalpos=0; */
unsigned int num_signal_dyn = 65536;
int attrtype, subtype;
uint64_t attrarg;
fstHandle maxhandle_scanbuild;
int cl;

if(!xc) return(0);

xc->longest_signal_value_len = 32; /* arbitrarily set at 32...this is much longer than an expanded double */

if(!xc->fh)
        {
        if(!fstReaderRecreateHierFile(xc))
                {
                return(0);
                }
        }

str = (char *)malloc(FST_ID_NAM_ATTR_SIZ+1);

if(fv)
        {
        char time_dimension[2] = {0, 0};
        int time_scale = 1;

        fprintf(fv, "$date\n\t%s\n$end\n", xc->date);
        fprintf(fv, "$version\n\t%s\n$end\n", xc->version);
        if(xc->timezero) fprintf(fv, "$timezero\n\t%" PRId64 "\n$end\n", xc->timezero);

        switch(xc->timescale)
                {
                case  2:        time_scale = 100;               time_dimension[0] = 0;   break;
                case  1:        time_scale = 10; /* fallthrough */
                case  0:                                        time_dimension[0] = 0;   break;

                case -1:        time_scale = 100;               time_dimension[0] = 'm'; break;
                case -2:        time_scale = 10; /* fallthrough */
                case -3:                                        time_dimension[0] = 'm'; break;

                case -4:        time_scale = 100;               time_dimension[0] = 'u'; break;
                case -5:        time_scale = 10; /* fallthrough */
                case -6:                                        time_dimension[0] = 'u'; break;

                case -10:       time_scale = 100;               time_dimension[0] = 'p'; break;
                case -11:       time_scale = 10; /* fallthrough */
                case -12:                                       time_dimension[0] = 'p'; break;

                case -13:       time_scale = 100;               time_dimension[0] = 'f'; break;
                case -14:       time_scale = 10; /* fallthrough */
                case -15:                                       time_dimension[0] = 'f'; break;

                case -16:       time_scale = 100;               time_dimension[0] = 'a'; break;
                case -17:       time_scale = 10; /* fallthrough */
                case -18:                                       time_dimension[0] = 'a'; break;

                case -19:       time_scale = 100;               time_dimension[0] = 'z'; break;
                case -20:       time_scale = 10; /* fallthrough */
                case -21:                                       time_dimension[0] = 'z'; break;

                case -7:        time_scale = 100;               time_dimension[0] = 'n'; break;
                case -8:        time_scale = 10; /* fallthrough */
                case -9:
                default:                                        time_dimension[0] = 'n'; break;
                }

        if(fv) fprintf(fv, "$timescale\n\t%d%ss\n$end\n", time_scale, time_dimension);
        }

xc->maxhandle = 0;
xc->num_alias = 0;

free(xc->signal_lens);
xc->signal_lens = (uint32_t *)malloc(num_signal_dyn*sizeof(uint32_t));

free(xc->signal_typs);
xc->signal_typs = (unsigned char *)malloc(num_signal_dyn*sizeof(unsigned char));

fstReaderFseeko(xc, xc->fh, 0, SEEK_SET);
while(!feof(xc->fh))
        {
        int tag = fgetc(xc->fh);
        switch(tag)
                {
                case FST_ST_VCD_SCOPE:
                        scopetype = fgetc(xc->fh);
                        if((scopetype < FST_ST_MIN) || (scopetype > FST_ST_MAX)) scopetype = FST_ST_VCD_MODULE;
                        pnt = str;
			cl = 0;
                        while((ch = fgetc(xc->fh)))
                                {
				if(cl < FST_ID_NAM_ATTR_SIZ)
					{
					pnt[cl++] = ch;
					}
                                }; /* scopename */
                        pnt[cl] = 0;
                        while(fgetc(xc->fh)) { }; /* scopecomp */

                        if(fv) fprintf(fv, "$scope %s %s $end\n", modtypes[scopetype], str);
                        break;

                case FST_ST_VCD_UPSCOPE:
                        if(fv) fprintf(fv, "$upscope $end\n");
                        break;

                case FST_ST_GEN_ATTRBEGIN:
                        attrtype = fgetc(xc->fh);
                        subtype = fgetc(xc->fh);
                        pnt = str;
			cl = 0;
                        while((ch = fgetc(xc->fh)))
                                {
				if(cl < FST_ID_NAM_ATTR_SIZ)
					{
					pnt[cl++] = ch;
					}
                                }; /* attrname */
                        pnt[cl] = 0;

                        if(!str[0]) { strcpy(str, "\"\""); }

                        attrarg = fstReaderVarint64(xc->fh);

                        if(fv && xc->use_vcd_extensions)
                                {
                                switch(attrtype)
                                        {
                                        case FST_AT_ARRAY:      if((subtype < FST_AR_NONE) || (subtype > FST_AR_MAX)) subtype = FST_AR_NONE;
                                                                fprintf(fv, "$attrbegin %s %s %s %" PRId64 " $end\n", attrtypes[attrtype], arraytypes[subtype], str, attrarg);
                                                                break;
                                        case FST_AT_ENUM:       if((subtype < FST_EV_SV_INTEGER) || (subtype > FST_EV_MAX)) subtype = FST_EV_SV_INTEGER;
                                                                fprintf(fv, "$attrbegin %s %s %s %" PRId64 " $end\n", attrtypes[attrtype], enumvaluetypes[subtype], str, attrarg);
                                                                break;
                                        case FST_AT_PACK:       if((subtype < FST_PT_NONE) || (subtype > FST_PT_MAX)) subtype = FST_PT_NONE;
                                                                fprintf(fv, "$attrbegin %s %s %s %" PRId64 " $end\n", attrtypes[attrtype], packtypes[subtype], str, attrarg);
                                                                break;
                                        case FST_AT_MISC:
                                        default:                attrtype = FST_AT_MISC;
                                                                if(subtype == FST_MT_COMMENT)
                                                                        {
                                                                        fprintf(fv, "$comment\n\t%s\n$end\n", str);
                                                                        }
                                                                        else
                                                                        {
                                                                        if((subtype == FST_MT_SOURCESTEM)||(subtype == FST_MT_SOURCEISTEM))
                                                                                {
                                                                                int sidx_skiplen_dummy = 0;
                                                                                uint64_t sidx = fstGetVarint64((unsigned char *)str, &sidx_skiplen_dummy);

                                                                                fprintf(fv, "$attrbegin %s %02x %" PRId64 " %" PRId64 " $end\n", attrtypes[attrtype], subtype, sidx, attrarg);
                                                                                }
                                                                                else
                                                                                {
                                                                                fprintf(fv, "$attrbegin %s %02x %s %" PRId64 " $end\n", attrtypes[attrtype], subtype, str, attrarg);
                                                                                }
                                                                        }
                                                                break;
                                        }
                                }
                        break;

                case FST_ST_GEN_ATTREND:
                        if(fv && xc->use_vcd_extensions) fprintf(fv, "$attrend $end\n");
                        break;

                case FST_VT_VCD_EVENT:
                case FST_VT_VCD_INTEGER:
                case FST_VT_VCD_PARAMETER:
                case FST_VT_VCD_REAL:
                case FST_VT_VCD_REAL_PARAMETER:
                case FST_VT_VCD_REG:
                case FST_VT_VCD_SUPPLY0:
                case FST_VT_VCD_SUPPLY1:
                case FST_VT_VCD_TIME:
                case FST_VT_VCD_TRI:
                case FST_VT_VCD_TRIAND:
                case FST_VT_VCD_TRIOR:
                case FST_VT_VCD_TRIREG:
                case FST_VT_VCD_TRI0:
                case FST_VT_VCD_TRI1:
                case FST_VT_VCD_WAND:
                case FST_VT_VCD_WIRE:
                case FST_VT_VCD_WOR:
                case FST_VT_VCD_PORT:
                case FST_VT_VCD_SPARRAY:
                case FST_VT_VCD_REALTIME:
                case FST_VT_GEN_STRING:
                case FST_VT_SV_BIT:
                case FST_VT_SV_LOGIC:
                case FST_VT_SV_INT:
                case FST_VT_SV_SHORTINT:
                case FST_VT_SV_LONGINT:
                case FST_VT_SV_BYTE:
                case FST_VT_SV_ENUM:
                case FST_VT_SV_SHORTREAL:
                        vartype = tag;
                        /* vardir = */ fgetc(xc->fh); /* unused in VCD reader, but need to advance read pointer */
                        pnt = str;
			cl = 0;
                        while((ch = fgetc(xc->fh)))
                                {
				if(cl < FST_ID_NAM_ATTR_SIZ)
					{
					pnt[cl++] = ch;
					}
                                }; /* varname */
                        pnt[cl] = 0;
                        len = fstReaderVarint32(xc->fh);
                        alias = fstReaderVarint32(xc->fh);

                        if(!alias)
                                {
                                if(xc->maxhandle == num_signal_dyn)
                                        {
                                        num_signal_dyn *= 2;
                                        xc->signal_lens = (uint32_t *)realloc(xc->signal_lens, num_signal_dyn*sizeof(uint32_t));
                                        xc->signal_typs = (unsigned char *)realloc(xc->signal_typs, num_signal_dyn*sizeof(unsigned char));
                                        }
                                xc->signal_lens[xc->maxhandle] = len;
                                xc->signal_typs[xc->maxhandle] = vartype;

                                /* maxvalpos+=len; */
                                if(len > xc->longest_signal_value_len)
                                        {
                                        xc->longest_signal_value_len = len;
                                        }

                                if((vartype == FST_VT_VCD_REAL) || (vartype == FST_VT_VCD_REAL_PARAMETER) || (vartype == FST_VT_VCD_REALTIME) || (vartype == FST_VT_SV_SHORTREAL))
                                        {
                                        len = (vartype != FST_VT_SV_SHORTREAL) ? 64 : 32;
                                        xc->signal_typs[xc->maxhandle] = FST_VT_VCD_REAL;
                                        }
                                if(fv)
                                        {
                                        char vcdid_buf[16];
                                        uint32_t modlen = (vartype != FST_VT_VCD_PORT) ? len : ((len - 2) / 3);
                                        fstVcdID(vcdid_buf, xc->maxhandle+1);
                                        fprintf(fv, "$var %s %" PRIu32 " %s %s $end\n", vartypes[vartype], modlen, vcdid_buf, str);
                                        }
                                xc->maxhandle++;
                                }
                                else
                                {
                                if((vartype == FST_VT_VCD_REAL) || (vartype == FST_VT_VCD_REAL_PARAMETER) || (vartype == FST_VT_VCD_REALTIME) || (vartype == FST_VT_SV_SHORTREAL))
                                        {
                                        len = (vartype != FST_VT_SV_SHORTREAL) ? 64 : 32;
                                        xc->signal_typs[xc->maxhandle] = FST_VT_VCD_REAL;
                                        }
                                if(fv)
                                        {
                                        char vcdid_buf[16];
                                        uint32_t modlen = (vartype != FST_VT_VCD_PORT) ? len : ((len - 2) / 3);
                                        fstVcdID(vcdid_buf, alias);
                                        fprintf(fv, "$var %s %" PRIu32 " %s %s $end\n", vartypes[vartype], modlen, vcdid_buf, str);
                                        }
                                xc->num_alias++;
                                }

                        break;

                default:
                        break;
                }
        }
if(fv) fprintf(fv, "$enddefinitions $end\n");

maxhandle_scanbuild = xc->maxhandle ? xc->maxhandle : 1; /*scan-build warning suppression, in reality we have at least one signal */

xc->signal_lens = (uint32_t *)realloc(xc->signal_lens, maxhandle_scanbuild*sizeof(uint32_t));
xc->signal_typs = (unsigned char *)realloc(xc->signal_typs, maxhandle_scanbuild*sizeof(unsigned char));

free(xc->process_mask);
xc->process_mask = (unsigned char *)calloc(1, (maxhandle_scanbuild+7)/8);

free(xc->temp_signal_value_buf);
xc->temp_signal_value_buf = (unsigned char *)malloc(xc->longest_signal_value_len + 1);

xc->var_count = xc->maxhandle + xc->num_alias;

free(str);
return(1);
}


/*
 * reader file open/close functions
 */
int fstReaderInit(struct fstReaderContext *xc)
{
fst_off_t blkpos = 0;
fst_off_t endfile;
uint64_t seclen;
int sectype;
uint64_t vc_section_count_actual = 0;
int hdr_incomplete = 0;
int hdr_seen = 0;
int gzread_pass_status = 1;

sectype = fgetc(xc->f);
if(sectype == FST_BL_ZWRAPPER)
        {
        FILE *fcomp;
        fst_off_t offpnt, uclen;
        char gz_membuf[FST_GZIO_LEN];
        gzFile zhandle;
        int zfd;
        int flen = strlen(xc->filename);
        char *hf;
	int hf_len;

        seclen = fstReaderUint64(xc->f);
        uclen = fstReaderUint64(xc->f);

        if(!seclen) return(0); /* not finished compressing, this is a failed read */

	hf_len = flen + 16 + 32 + 1;
        hf = (char *)calloc(1, hf_len);

        snprintf(hf, hf_len, "%s.upk_%d_%p", xc->filename, getpid(), (void *)xc);
        fcomp = fopen(hf, "w+b");
        if(!fcomp)
                {
                fcomp = tmpfile_open(&xc->f_nam);
                free(hf); hf = NULL;
                if(!fcomp) { tmpfile_close(&fcomp, &xc->f_nam); return(0); }
                }

#if defined(FST_UNBUFFERED_IO)
        setvbuf(fcomp, (char *)NULL, _IONBF, 0);   /* keeps gzip from acting weird in tandem with fopen */
#endif

#ifdef __MINGW32__
        xc->filename_unpacked = hf;
#else
        if(hf)
                {
                unlink(hf);
                free(hf);
                }
#endif

        fstReaderFseeko(xc, xc->f, FST_ZWRAPPER_HDR_SIZE, SEEK_SET);
#ifndef __MINGW32__
        fflush(xc->f);
#else
	/* Windows UCRT runtime library reads one byte ahead in the file
	   even with buffering disabled and does not synchronise the
	   file position after fseek. */
	_lseek(fileno(xc->f), FST_ZWRAPPER_HDR_SIZE, SEEK_SET);
#endif

        zfd = dup(fileno(xc->f));
        zhandle = gzdopen(zfd, "rb");
        if(zhandle)
                {
                for(offpnt = 0; offpnt < uclen; offpnt += FST_GZIO_LEN)
                        {
                        size_t this_len = ((uclen - offpnt) > FST_GZIO_LEN) ? FST_GZIO_LEN : (uclen - offpnt);
                        size_t gzreadlen = gzread(zhandle, gz_membuf, this_len);
                        size_t fwlen;

                        if(gzreadlen != this_len)
                                {
                                gzread_pass_status = 0;
                                break;
                                }
                        fwlen = fstFwrite(gz_membuf, this_len, 1, fcomp);
                        if(fwlen != 1)
                                {
                                gzread_pass_status = 0;
                                break;
                                }
                        }
                gzclose(zhandle);
                }
                else
                {
                close(zfd);
                }
        fflush(fcomp);
        fclose(xc->f);
        xc->f = fcomp;
        }

if(gzread_pass_status)
        {
        fstReaderFseeko(xc, xc->f, 0, SEEK_END);
        endfile = ftello(xc->f);

        while(blkpos < endfile)
                {
                fstReaderFseeko(xc, xc->f, blkpos, SEEK_SET);

                sectype = fgetc(xc->f);
                seclen = fstReaderUint64(xc->f);

                if(sectype == EOF)
                        {
                        break;
                        }

                if((hdr_incomplete) && (!seclen))
                        {
                        break;
                        }

                if(!hdr_seen && (sectype != FST_BL_HDR))
                        {
                        break;
                        }

                blkpos++;
                if(sectype == FST_BL_HDR)
                        {
                        if(!hdr_seen)
                                {
                                int ch;
                                double dcheck;

                                xc->start_time = fstReaderUint64(xc->f);
                                xc->end_time = fstReaderUint64(xc->f);

                                hdr_incomplete = (xc->start_time == 0) && (xc->end_time == 0);

                                fstFread(&dcheck, 8, 1, xc->f);
                                xc->double_endian_match = (dcheck == FST_DOUBLE_ENDTEST);
                                if(!xc->double_endian_match)
                                        {
                                        union   {
                                                unsigned char rvs_buf[8];
                                                double d;
                                                } vu;

                                        unsigned char *dcheck_alias = (unsigned char *)&dcheck;
                                        int rvs_idx;

                                        for(rvs_idx=0;rvs_idx<8;rvs_idx++)
                                                {
                                                vu.rvs_buf[rvs_idx] = dcheck_alias[7-rvs_idx];
                                                }
                                        if(vu.d != FST_DOUBLE_ENDTEST)
                                                {
                                                break; /* either corrupt file or wrong architecture (offset +33 also functions as matchword) */
                                                }
                                        }

                                hdr_seen = 1;

                                xc->mem_used_by_writer = fstReaderUint64(xc->f);
                                xc->scope_count = fstReaderUint64(xc->f);
                                xc->var_count = fstReaderUint64(xc->f);
                                xc->maxhandle = fstReaderUint64(xc->f);
                                xc->num_alias = xc->var_count - xc->maxhandle;
                                xc->vc_section_count = fstReaderUint64(xc->f);
                                ch = fgetc(xc->f);
                                xc->timescale = (signed char)ch;
                                fstFread(xc->version, FST_HDR_SIM_VERSION_SIZE, 1, xc->f);
                                xc->version[FST_HDR_SIM_VERSION_SIZE] = 0;
                                fstFread(xc->date, FST_HDR_DATE_SIZE, 1, xc->f);
                                xc->date[FST_HDR_DATE_SIZE] = 0;
                                ch = fgetc(xc->f);
                                xc->filetype = (unsigned char)ch;
                                xc->timezero = fstReaderUint64(xc->f);
                                }
                        }
                else if((sectype == FST_BL_VCDATA) || (sectype == FST_BL_VCDATA_DYN_ALIAS) || (sectype == FST_BL_VCDATA_DYN_ALIAS2))
                        {
                        if(hdr_incomplete)
                                {
                                uint64_t bt = fstReaderUint64(xc->f);
                                xc->end_time = fstReaderUint64(xc->f);

                                if(!vc_section_count_actual) { xc->start_time = bt; }
                                }

                        vc_section_count_actual++;
                        }
                else if(sectype == FST_BL_GEOM)
                        {
                        if(!hdr_incomplete)
                                {
                                uint64_t clen = seclen - 24;
                                uint64_t uclen = fstReaderUint64(xc->f);
                                unsigned char *ucdata = (unsigned char *)malloc(uclen);
                                unsigned char *pnt = ucdata;
                                unsigned int i;

                                xc->contains_geom_section = 1;
                                xc->maxhandle = fstReaderUint64(xc->f);
                                xc->longest_signal_value_len = 32; /* arbitrarily set at 32...this is much longer than an expanded double */

                                free(xc->process_mask);
                                xc->process_mask = (unsigned char *)calloc(1, (xc->maxhandle+7)/8);

                                if(clen != uclen)
                                        {
                                        unsigned char *cdata = (unsigned char *)malloc(clen);
                                        unsigned long destlen = uclen;
                                        unsigned long sourcelen = clen;
                                        int rc;

                                        fstFread(cdata, clen, 1, xc->f);
                                        rc = uncompress(ucdata, &destlen, cdata, sourcelen);

                                        if(rc != Z_OK)
                                                {
                                                fprintf(stderr, FST_APIMESS "fstReaderInit(), geom uncompress rc = %d, exiting.\n", rc);
                                                exit(255);
                                                }

                                        free(cdata);
                                        }
                                        else
                                        {
                                        fstFread(ucdata, uclen, 1, xc->f);
                                        }

                                free(xc->signal_lens);
                                xc->signal_lens = (uint32_t *)malloc(sizeof(uint32_t) * xc->maxhandle);
                                free(xc->signal_typs);
                                xc->signal_typs = (unsigned char *)malloc(sizeof(unsigned char) * xc->maxhandle);

                                for(i=0;i<xc->maxhandle;i++)
                                        {
                                        int skiplen;
                                        uint64_t val = fstGetVarint32(pnt, &skiplen);

                                        pnt += skiplen;

                                        if(val)
                                                {
                                                xc->signal_lens[i] = (val != 0xFFFFFFFF) ? val : 0;
                                                xc->signal_typs[i] = FST_VT_VCD_WIRE;
                                                if(xc->signal_lens[i] > xc->longest_signal_value_len)
                                                        {
                                                        xc->longest_signal_value_len = xc->signal_lens[i];
                                                        }
                                                }
                                                else
                                                {
                                                xc->signal_lens[i] = 8; /* backpatch in real */
                                                xc->signal_typs[i] = FST_VT_VCD_REAL;
                                                /* xc->longest_signal_value_len handled above by overly large init size */
                                                }
                                        }

                                free(xc->temp_signal_value_buf);
                                xc->temp_signal_value_buf = (unsigned char *)malloc(xc->longest_signal_value_len + 1);

                                free(ucdata);
                                }
                        }
                else if(sectype == FST_BL_HIER)
                        {
                        xc->contains_hier_section = 1;
                        xc->hier_pos = ftello(xc->f);
                        }
                else if(sectype == FST_BL_HIER_LZ4DUO)
                        {
                        xc->contains_hier_section_lz4    = 1;
                        xc->contains_hier_section_lz4duo = 1;
                        xc->hier_pos = ftello(xc->f);
                        }
                else if(sectype == FST_BL_HIER_LZ4)
                        {
                        xc->contains_hier_section_lz4 = 1;
                        xc->hier_pos = ftello(xc->f);
                        }
                else if(sectype == FST_BL_BLACKOUT)
                        {
                        uint32_t i;
                        uint64_t cur_bl = 0;
                        uint64_t delta;

                        xc->num_blackouts = fstReaderVarint32(xc->f);
                        free(xc->blackout_times);
                        xc->blackout_times = (uint64_t *)calloc(xc->num_blackouts, sizeof(uint64_t));
                        free(xc->blackout_activity);
                        xc->blackout_activity = (unsigned char *)calloc(xc->num_blackouts, sizeof(unsigned char));

                        for(i=0;i<xc->num_blackouts;i++)
                                {
                                xc->blackout_activity[i] = fgetc(xc->f) != 0;
                                delta = fstReaderVarint64(xc->f);
                                cur_bl += delta;
                                xc->blackout_times[i] = cur_bl;
                                }
                        }

                blkpos += seclen;
                if(!hdr_seen) break;
                }

        if(hdr_seen)
                {
                if(xc->vc_section_count != vc_section_count_actual)
                        {
                        xc->vc_section_count = vc_section_count_actual;
                        }

                if(!xc->contains_geom_section)
                        {
                        fstReaderProcessHier(xc, NULL); /* recreate signal_lens/signal_typs info */
                        }
                }
        }

return(hdr_seen);
}


void *fstReaderOpenForUtilitiesOnly(void)
{
struct fstReaderContext *xc = (struct fstReaderContext *)calloc(1, sizeof(struct fstReaderContext));

return(xc);
}


void *fstReaderOpen(const char *nam)
{
struct fstReaderContext *xc = (struct fstReaderContext *)calloc(1, sizeof(struct fstReaderContext));

if((!nam)||(!(xc->f=fopen(nam, "rb"))))
        {
        free(xc);
        xc=NULL;
        }
        else
        {
        int flen = strlen(nam);
        char *hf = (char *)calloc(1, flen + 6);
        int rc;

#if defined(FST_UNBUFFERED_IO)
        setvbuf(xc->f, (char *)NULL, _IONBF, 0);   /* keeps gzip from acting weird in tandem with fopen */
#endif

        memcpy(hf, nam, flen);
        strcpy(hf + flen, ".hier");
        xc->fh = fopen(hf, "rb");

        free(hf);
        xc->filename = strdup(nam);
        rc = fstReaderInit(xc);

        if((rc) && (xc->vc_section_count) && (xc->maxhandle) && ((xc->fh)||(xc->contains_hier_section||(xc->contains_hier_section_lz4))))
                {
                /* more init */
                xc->do_rewind = 1;
                }
                else
                {
                fstReaderClose(xc);
                xc = NULL;
                }
        }

return(xc);
}


static void fstReaderDeallocateRvatData(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
if(xc)
        {
        free(xc->rvat_chain_mem); xc->rvat_chain_mem = NULL;
        free(xc->rvat_frame_data); xc->rvat_frame_data = NULL;
        free(xc->rvat_time_table); xc->rvat_time_table = NULL;
        free(xc->rvat_chain_table); xc->rvat_chain_table = NULL;
        free(xc->rvat_chain_table_lengths); xc->rvat_chain_table_lengths = NULL;

        xc->rvat_data_valid = 0;
        }
}


void fstReaderClose(void *ctx)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

if(xc)
        {
        fstReaderDeallocateScopeData(xc);
        fstReaderDeallocateRvatData(xc);
        free(xc->rvat_sig_offs); xc->rvat_sig_offs = NULL;

        free(xc->process_mask); xc->process_mask = NULL;
        free(xc->blackout_times); xc->blackout_times = NULL;
        free(xc->blackout_activity); xc->blackout_activity = NULL;
        free(xc->temp_signal_value_buf); xc->temp_signal_value_buf = NULL;
        free(xc->signal_typs); xc->signal_typs = NULL;
        free(xc->signal_lens); xc->signal_lens = NULL;
        free(xc->filename); xc->filename = NULL;
	free(xc->str_scope_attr); xc->str_scope_attr = NULL;

        if(xc->fh)
                {
                tmpfile_close(&xc->fh, &xc->fh_nam);
                }

        if(xc->f)
                {
                tmpfile_close(&xc->f, &xc->f_nam);
                if(xc->filename_unpacked)
                        {
                        unlink(xc->filename_unpacked);
                        free(xc->filename_unpacked);
                        }
                }

        free(xc);
        }
}


/*
 * read processing
 */

/* normal read which re-interleaves the value change data */
int fstReaderIterBlocks(void *ctx,
        void (*value_change_callback)(void *user_callback_data_pointer, uint64_t time, fstHandle facidx, const unsigned char *value),
        void *user_callback_data_pointer, FILE *fv)
{
return(fstReaderIterBlocks2(ctx, value_change_callback, NULL, user_callback_data_pointer, fv));
}


int fstReaderIterBlocks2(void *ctx,
        void (*value_change_callback)(void *user_callback_data_pointer, uint64_t time, fstHandle facidx, const unsigned char *value),
        void (*value_change_callback_varlen)(void *user_callback_data_pointer, uint64_t time, fstHandle facidx, const unsigned char *value, uint32_t len),
        void *user_callback_data_pointer, FILE *fv)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;

uint64_t previous_time = UINT64_MAX;
uint64_t *time_table = NULL;
uint64_t tsec_nitems;
unsigned int secnum = 0;
int blocks_skipped = 0;
fst_off_t blkpos = 0;
uint64_t seclen, beg_tim;
uint64_t end_tim;
uint64_t frame_uclen, frame_clen, frame_maxhandle, vc_maxhandle;
fst_off_t vc_start;
fst_off_t indx_pntr, indx_pos;
fst_off_t *chain_table = NULL;
uint32_t *chain_table_lengths = NULL;
unsigned char *chain_cmem;
unsigned char *pnt;
long chain_clen;
fstHandle idx, pidx=0, i;
uint64_t pval;
uint64_t vc_maxhandle_largest = 0;
uint64_t tsec_uclen = 0, tsec_clen = 0;
int sectype;
uint64_t mem_required_for_traversal;
unsigned char *mem_for_traversal = NULL;
uint32_t traversal_mem_offs;
uint32_t *scatterptr, *headptr, *length_remaining;
uint32_t cur_blackout = 0;
int packtype;
unsigned char *mc_mem = NULL;
uint32_t mc_mem_len; /* corresponds to largest value encountered in chain_table_lengths[i] */
int dumpvars_state = 0;

if(!xc) return(0);

scatterptr = (uint32_t *)calloc(xc->maxhandle, sizeof(uint32_t));
headptr = (uint32_t *)calloc(xc->maxhandle, sizeof(uint32_t));
length_remaining = (uint32_t *)calloc(xc->maxhandle, sizeof(uint32_t));

if(fv)
        {
#ifndef FST_WRITEX_DISABLE
        fflush(fv);
        setvbuf(fv, (char *) NULL, _IONBF, 0); /* even buffered IO is slow so disable it and use our own routines that don't need seeking */
        xc->writex_fd = fileno(fv);
#endif
        }

for(;;)
        {
        uint32_t *tc_head = NULL;
	uint32_t tc_head_items = 0;
        traversal_mem_offs = 0;

        fstReaderFseeko(xc, xc->f, blkpos, SEEK_SET);

        sectype = fgetc(xc->f);
        seclen = fstReaderUint64(xc->f);

        if((sectype == EOF) || (sectype == FST_BL_SKIP))
                {
#ifdef FST_DEBUG
                fprintf(stderr, FST_APIMESS "<< EOF >>\n");
#endif
                break;
                }

        blkpos++;
        if((sectype != FST_BL_VCDATA) && (sectype != FST_BL_VCDATA_DYN_ALIAS) && (sectype != FST_BL_VCDATA_DYN_ALIAS2))
                {
                blkpos += seclen;
                continue;
                }

        if(!seclen) break;

        beg_tim = fstReaderUint64(xc->f);
        end_tim = fstReaderUint64(xc->f);

        if(xc->limit_range_valid)
                {
                if(end_tim < xc->limit_range_start)
                        {
                        blocks_skipped++;
                        blkpos += seclen;
                        continue;
                        }

                if(beg_tim > xc->limit_range_end) /* likely the compare in for(i=0;i<tsec_nitems;i++) below would do this earlier */
                        {
                        break;
                        }
                }


        mem_required_for_traversal = fstReaderUint64(xc->f) + 66; /* add in potential fastlz overhead */
        mem_for_traversal = (unsigned char *)malloc(mem_required_for_traversal);
#ifdef FST_DEBUG
        fprintf(stderr, FST_APIMESS "sec: %u seclen: %d begtim: %d endtim: %d\n",
                secnum, (int)seclen, (int)beg_tim, (int)end_tim);
        fprintf(stderr, FST_APIMESS "mem_required_for_traversal: %d\n", (int)mem_required_for_traversal-66);
#endif
        /* process time block */
        {
        unsigned char *ucdata;
        unsigned char *cdata;
        unsigned long destlen /* = tsec_uclen */; /* scan-build */
        unsigned long sourcelen /*= tsec_clen */; /* scan-build */
        int rc;
        unsigned char *tpnt;
        uint64_t tpval;
        unsigned int ti;

        if(fstReaderFseeko(xc, xc->f, blkpos + seclen - 24, SEEK_SET) != 0) break;
        tsec_uclen = fstReaderUint64(xc->f);
        tsec_clen = fstReaderUint64(xc->f);
        tsec_nitems = fstReaderUint64(xc->f);
#ifdef FST_DEBUG
        fprintf(stderr, FST_APIMESS "time section unc: %d, com: %d (%d items)\n",
                (int)tsec_uclen, (int)tsec_clen, (int)tsec_nitems);
#endif
        if(tsec_clen > seclen) break; /* corrupted tsec_clen: by definition it can't be larger than size of section */
        ucdata = (unsigned char *)malloc(tsec_uclen);
        if(!ucdata) break; /* malloc fail as tsec_uclen out of range from corrupted file */
        destlen = tsec_uclen;
        sourcelen = tsec_clen;

        fstReaderFseeko(xc, xc->f, -24 - ((fst_off_t)tsec_clen), SEEK_CUR);

        if(tsec_uclen != tsec_clen)
                {
                cdata = (unsigned char *)malloc(tsec_clen);
                fstFread(cdata, tsec_clen, 1, xc->f);

                rc = uncompress(ucdata, &destlen, cdata, sourcelen);

                if(rc != Z_OK)
                        {
                        fprintf(stderr, FST_APIMESS "fstReaderIterBlocks2(), tsec uncompress rc = %d, exiting.\n", rc);
                        exit(255);
                        }

                free(cdata);
                }
                else
                {
                fstFread(ucdata, tsec_uclen, 1, xc->f);
                }

        free(time_table);

	if(sizeof(size_t) < sizeof(uint64_t))
		{
		/* TALOS-2023-1792 for 32b overflow */
		uint64_t chk_64 = tsec_nitems * sizeof(uint64_t);
		size_t   chk_32 = ((size_t)tsec_nitems) * sizeof(uint64_t);
		if(chk_64 != chk_32) chk_report_abort("TALOS-2023-1792");
		}
	else
		{
		uint64_t chk_64 = tsec_nitems * sizeof(uint64_t);
		if((chk_64/sizeof(uint64_t)) != tsec_nitems)
			{
			chk_report_abort("TALOS-2023-1792");
			}
		}
        time_table = (uint64_t *)calloc(tsec_nitems, sizeof(uint64_t));
        tpnt = ucdata;
        tpval = 0;
        for(ti=0;ti<tsec_nitems;ti++)
                {
                int skiplen;
                uint64_t val = fstGetVarint64(tpnt, &skiplen);
                tpval = time_table[ti] = tpval + val;
                tpnt += skiplen;
                }

	tc_head_items = tsec_nitems /* scan-build */ ? tsec_nitems : 1;
	if(sizeof(size_t) < sizeof(uint64_t))
		{
		/* TALOS-2023-1792 for 32b overflow */
		uint64_t chk_64 = tc_head_items * sizeof(uint32_t);
		size_t   chk_32 = ((size_t)tc_head_items) * sizeof(uint32_t);
		if(chk_64 != chk_32) chk_report_abort("TALOS-2023-1792");
		}
	else
		{
		uint64_t chk_64 = tc_head_items * sizeof(uint32_t);
		if((chk_64/sizeof(uint32_t)) != tc_head_items)
			{
			chk_report_abort("TALOS-2023-1792");
			}
		}
        tc_head = (uint32_t *)calloc(tc_head_items, sizeof(uint32_t));
        free(ucdata);
        }

        fstReaderFseeko(xc, xc->f, blkpos+32, SEEK_SET);

        frame_uclen = fstReaderVarint64(xc->f);
        frame_clen = fstReaderVarint64(xc->f);
        frame_maxhandle = fstReaderVarint64(xc->f);

        if(secnum == 0)
                {
                if((beg_tim != time_table[0]) || (blocks_skipped))
                        {
                        unsigned char *mu = (unsigned char *)malloc(frame_uclen);
                        uint32_t sig_offs = 0;

                        if(fv)
                                {
                                char wx_buf[32];
                                int wx_len;

                                if(beg_tim)
                                        {
					if(dumpvars_state == 1) { wx_len = snprintf(wx_buf, 32, "$end\n"); fstWritex(xc, wx_buf, wx_len); dumpvars_state = 2; }
                                        wx_len = snprintf(wx_buf, 32, "#%" PRIu64 "\n", beg_tim);
                                        fstWritex(xc, wx_buf, wx_len);
					if(!dumpvars_state) { wx_len = snprintf(wx_buf, 32, "$dumpvars\n"); fstWritex(xc, wx_buf, wx_len); dumpvars_state = 1; }
                                        }
                                if((xc->num_blackouts)&&(cur_blackout != xc->num_blackouts))
                                        {
                                        if(beg_tim == xc->blackout_times[cur_blackout])
                                                {
                                                wx_len = snprintf(wx_buf, 32, "$dump%s $end\n", (xc->blackout_activity[cur_blackout++]) ? "on" : "off");
                                                fstWritex(xc, wx_buf, wx_len);
                                                }
                                        }
                                }

                        if(frame_uclen == frame_clen)
                                {
                                fstFread(mu, frame_uclen, 1, xc->f);
                                }
                                else
                                {
                                unsigned char *mc = (unsigned char *)malloc(frame_clen);
                                int rc;

                                unsigned long destlen = frame_uclen;
                                unsigned long sourcelen = frame_clen;

                                fstFread(mc, sourcelen, 1, xc->f);
                                rc = uncompress(mu, &destlen, mc, sourcelen);
                                if(rc != Z_OK)
                                        {
                                        fprintf(stderr, FST_APIMESS "fstReaderIterBlocks2(), frame uncompress rc: %d, exiting.\n", rc);
                                        exit(255);
                                        }
                                free(mc);
                                }


                        for(idx=0;idx<frame_maxhandle;idx++)
                                {
                                int process_idx = idx/8;
                                int process_bit = idx&7;

                                if(xc->process_mask[process_idx]&(1<<process_bit))
                                        {
                                        if(xc->signal_lens[idx] <= 1)
                                                {
                                                if(xc->signal_lens[idx] == 1)
                                                        {
                                                        unsigned char val = mu[sig_offs];
                                                        if(value_change_callback)
                                                                {
                                                                xc->temp_signal_value_buf[0] = val;
                                                                xc->temp_signal_value_buf[1] = 0;
                                                                value_change_callback(user_callback_data_pointer, beg_tim, idx+1, xc->temp_signal_value_buf);
                                                                }
                                                                else
                                                                {
                                                                if(fv)
                                                                        {
                                                                        char vcd_id[16];

                                                                        int vcdid_len = fstVcdIDForFwrite(vcd_id+1, idx+1);
                                                                        vcd_id[0] = val; /* collapse 3 writes into one I/O call */
                                                                        vcd_id[vcdid_len + 1] = '\n';
                                                                        fstWritex(xc, vcd_id, vcdid_len + 2);
                                                                        }
                                                                }
                                                        }
                                                        else
                                                        {
                                                        /* variable-length ("0" length) records have no initial state */
                                                        }
                                                }
                                                else
                                                {
                                                if(xc->signal_typs[idx] != FST_VT_VCD_REAL)
                                                        {
                                                        if(value_change_callback)
                                                                {
								if(xc->signal_lens[idx] > xc->longest_signal_value_len)
									{
									chk_report_abort("TALOS-2023-1797");
									}
                                                                memcpy(xc->temp_signal_value_buf, mu+sig_offs, xc->signal_lens[idx]);
                                                                xc->temp_signal_value_buf[xc->signal_lens[idx]] = 0;
                                                                value_change_callback(user_callback_data_pointer, beg_tim, idx+1, xc->temp_signal_value_buf);
                                                                }
                                                                else
                                                                {
                                                                if(fv)
                                                                        {
                                                                        char vcd_id[16];
                                                                        int vcdid_len = fstVcdIDForFwrite(vcd_id+1, idx+1);

                                                                        vcd_id[0] = (xc->signal_typs[idx] != FST_VT_VCD_PORT) ? 'b' : 'p';
                                                                        fstWritex(xc, vcd_id, 1);
									if((sig_offs + xc->signal_lens[idx]) > frame_uclen)
										{
										chk_report_abort("TALOS-2023-1793");
										}
                                                                        fstWritex(xc,mu+sig_offs, xc->signal_lens[idx]);

                                                                        vcd_id[0] = ' '; /* collapse 3 writes into one I/O call */
                                                                        vcd_id[vcdid_len + 1] = '\n';
                                                                        fstWritex(xc, vcd_id, vcdid_len + 2);
                                                                        }
                                                                }
                                                        }
                                                        else
                                                        {
                                                        double d;
                                                        unsigned char *clone_d;
                                                        unsigned char *srcdata = mu+sig_offs;

                                                        if(value_change_callback)
                                                                {
                                                                if(xc->native_doubles_for_cb)
                                                                        {
                                                                        if(xc->double_endian_match)
                                                                                {
                                                                                clone_d = srcdata;
                                                                                }
                                                                                else
                                                                                {
                                                                                int j;

                                                                                clone_d = (unsigned char *)&d;
                                                                                for(j=0;j<8;j++)
                                                                                        {
                                                                                        clone_d[j] = srcdata[7-j];
                                                                                        }
                                                                                }
                                                                        value_change_callback(user_callback_data_pointer, beg_tim, idx+1, clone_d);
                                                                        }
                                                                        else
                                                                        {
                                                                        clone_d = (unsigned char *)&d;
                                                                        if(xc->double_endian_match)
                                                                                {
                                                                                memcpy(clone_d, srcdata, 8);
                                                                                }
                                                                                else
                                                                                {
                                                                                int j;

                                                                                for(j=0;j<8;j++)
                                                                                        {
                                                                                        clone_d[j] = srcdata[7-j];
                                                                                        }
                                                                                }
                                                                        snprintf((char *)xc->temp_signal_value_buf, xc->longest_signal_value_len + 1, "%.16g", d);
                                                                        value_change_callback(user_callback_data_pointer, beg_tim, idx+1, xc->temp_signal_value_buf);
                                                                        }
                                                                }
                                                                else
                                                                {
                                                                if(fv)
                                                                        {
                                                                        char vcdid_buf[16];
                                                                        char wx_buf[64];
                                                                        int wx_len;

                                                                        clone_d = (unsigned char *)&d;
                                                                        if(xc->double_endian_match)
                                                                                {
                                                                                memcpy(clone_d, srcdata, 8);
                                                                                }
                                                                                else
                                                                                {
                                                                                int j;

                                                                                for(j=0;j<8;j++)
                                                                                        {
                                                                                        clone_d[j] = srcdata[7-j];
                                                                                        }
                                                                                }

                                                                        fstVcdID(vcdid_buf, idx+1);
                                                                        wx_len = snprintf(wx_buf, 64, "r%.16g %s\n", d, vcdid_buf);
                                                                        fstWritex(xc, wx_buf, wx_len);
                                                                        }
                                                                }
                                                        }
                                                }
                                        }

                                sig_offs += xc->signal_lens[idx];
                                }

                        free(mu);
                        fstReaderFseeko(xc, xc->f, -((fst_off_t)frame_clen), SEEK_CUR);
                        }
                }

        fstReaderFseeko(xc, xc->f, (fst_off_t)frame_clen, SEEK_CUR); /* skip past compressed data */

        vc_maxhandle = fstReaderVarint64(xc->f);
        vc_start = ftello(xc->f);       /* points to '!' character */
        packtype = fgetc(xc->f);

#ifdef FST_DEBUG
        fprintf(stderr, FST_APIMESS "frame_uclen: %d, frame_clen: %d, frame_maxhandle: %d\n",
                (int)frame_uclen, (int)frame_clen, (int)frame_maxhandle);
        fprintf(stderr, FST_APIMESS "vc_maxhandle: %d, packtype: %c\n", (int)vc_maxhandle, packtype);
#endif

        indx_pntr = blkpos + seclen - 24 -tsec_clen -8;
        fstReaderFseeko(xc, xc->f, indx_pntr, SEEK_SET);
        chain_clen = fstReaderUint64(xc->f);
        indx_pos = indx_pntr - chain_clen;
#ifdef FST_DEBUG
        fprintf(stderr, FST_APIMESS "indx_pos: %d (%d bytes)\n", (int)indx_pos, (int)chain_clen);
#endif
        chain_cmem = (unsigned char *)malloc(chain_clen);
        if(!chain_cmem) goto block_err;
        fstReaderFseeko(xc, xc->f, indx_pos, SEEK_SET);
        fstFread(chain_cmem, chain_clen, 1, xc->f);

        if(vc_maxhandle > vc_maxhandle_largest)
                {
                free(chain_table);
                free(chain_table_lengths);

                vc_maxhandle_largest = vc_maxhandle;

		if(!(vc_maxhandle+1))
			{
			chk_report_abort("TALOS-2023-1798");
			}

		if(sizeof(size_t) < sizeof(uint64_t))
			{
			/* TALOS-2023-1798 for 32b overflow */
			uint64_t chk_64 = (vc_maxhandle+1) * sizeof(fst_off_t);
			size_t   chk_32 = ((size_t)(vc_maxhandle+1)) * sizeof(fst_off_t);
			if(chk_64 != chk_32) chk_report_abort("TALOS-2023-1798");
			}
		else
			{
			uint64_t chk_64 = (vc_maxhandle+1) * sizeof(fst_off_t);
				if((chk_64/sizeof(fst_off_t)) != (vc_maxhandle+1))
				{
				chk_report_abort("TALOS-2023-1798");
				}
			}
                chain_table = (fst_off_t *)calloc((vc_maxhandle+1), sizeof(fst_off_t));

		if(sizeof(size_t) < sizeof(uint64_t))
			{
			/* TALOS-2023-1798 for 32b overflow */
			uint64_t chk_64 = (vc_maxhandle+1) * sizeof(uint32_t);
			size_t   chk_32 = ((size_t)(vc_maxhandle+1)) * sizeof(uint32_t);
			if(chk_64 != chk_32) chk_report_abort("TALOS-2023-1798");
			}
		else
			{
			uint64_t chk_64 = (vc_maxhandle+1) * sizeof(uint32_t);
				if((chk_64/sizeof(uint32_t)) != (vc_maxhandle+1))
				{
				chk_report_abort("TALOS-2023-1798");
				}
			}
                chain_table_lengths = (uint32_t *)calloc((vc_maxhandle+1), sizeof(uint32_t));
                }

        if(!chain_table || !chain_table_lengths) goto block_err;

        pnt = chain_cmem;
        idx = 0;
        pval = 0;

        if(sectype == FST_BL_VCDATA_DYN_ALIAS2)
                {
                uint32_t prev_alias = 0;

                do      {
                        int skiplen;

                        if(*pnt & 0x01)
                                {
                                int64_t shval = fstGetSVarint64(pnt, &skiplen) >> 1;
                                if(shval > 0)
                                        {
                                        pval = chain_table[idx] = pval + shval;
                                        if(idx) { chain_table_lengths[pidx] = pval - chain_table[pidx]; }
                                        pidx = idx++;
                                        }
                                else if(shval < 0)
                                        {
                                        chain_table[idx] = 0;                                   /* need to explicitly zero as calloc above might not run */
                                        chain_table_lengths[idx] = prev_alias = shval;          /* because during this loop iter would give stale data! */
                                        idx++;
                                        }
                                else
                                        {
                                        chain_table[idx] = 0;                                   /* need to explicitly zero as calloc above might not run */
                                        chain_table_lengths[idx] = prev_alias;                  /* because during this loop iter would give stale data! */
                                        idx++;
                                        }
                                }
                                else
                                {
                                uint64_t val = fstGetVarint32(pnt, &skiplen);

                                fstHandle loopcnt = val >> 1;
				if((idx+loopcnt-1) > vc_maxhandle) /* TALOS-2023-1789 */
					{
					chk_report_abort("TALOS-2023-1789");
					}

                                for(i=0;i<loopcnt;i++)
                                        {
                                        chain_table[idx++] = 0;
                                        }
                                }

                        pnt += skiplen;
                        } while (pnt != (chain_cmem + chain_clen));
                }
                else
                {
                do      {
                        int skiplen;
                        uint64_t val = fstGetVarint32(pnt, &skiplen);

                        if(!val)
                                {
                                pnt += skiplen;
                                val = fstGetVarint32(pnt, &skiplen);
                                chain_table[idx] = 0;                   /* need to explicitly zero as calloc above might not run */
                                chain_table_lengths[idx] = -val;        /* because during this loop iter would give stale data! */
                                idx++;
                                }
                        else
                        if(val&1)
                                {
                                pval = chain_table[idx] = pval + (val >> 1);
                                if(idx) { chain_table_lengths[pidx] = pval - chain_table[pidx]; }
                                pidx = idx++;
                                }
                        else
                                {
                                fstHandle loopcnt = val >> 1;

				if((idx+loopcnt-1) > vc_maxhandle) /* TALOS-2023-1789 */
					{
					chk_report_abort("TALOS-2023-1789");
					}

                                for(i=0;i<loopcnt;i++)
                                        {
                                        chain_table[idx++] = 0;
                                        }
                                }

                        pnt += skiplen;
                        } while (pnt != (chain_cmem + chain_clen));
                }

        chain_table[idx] = indx_pos - vc_start;
        chain_table_lengths[pidx] = chain_table[idx] - chain_table[pidx];

        for(i=0;i<idx;i++)
                {
                int32_t v32 = chain_table_lengths[i];
                if((v32 < 0) && (!chain_table[i]))
                        {
                        v32 = -v32;
                        v32--;
                        if(((uint32_t)v32) < i) /* sanity check */
                                {
                                chain_table[i] = chain_table[v32];
                                chain_table_lengths[i] = chain_table_lengths[v32];
                                }
                        }
                }

#ifdef FST_DEBUG
        fprintf(stderr, FST_APIMESS "decompressed chain idx len: %" PRIu32 "\n", idx);
#endif

        mc_mem_len = 16384;
        mc_mem = (unsigned char *)malloc(mc_mem_len); /* buffer for compressed reads */

        /* check compressed VC data */
        if(idx > xc->maxhandle) idx = xc->maxhandle;
        for(i=0;i<idx;i++)
                {
                if(chain_table[i])
                        {
                        int process_idx = i/8;
                        int process_bit = i&7;

                        if(xc->process_mask[process_idx]&(1<<process_bit))
                                {
                                int rc = Z_OK;
                                uint32_t val;
                                uint32_t skiplen;
                                uint32_t tdelta;

                                fstReaderFseeko(xc, xc->f, vc_start + chain_table[i], SEEK_SET);
                                val = fstReaderVarint32WithSkip(xc->f, &skiplen);
                                if(val)
                                        {
                                        unsigned char *mu = mem_for_traversal + traversal_mem_offs; /* uncomp: dst */
                                        unsigned char *mc;                                          /* comp:   src */
                                        unsigned long destlen = val;
                                        unsigned long sourcelen = chain_table_lengths[i];

					if(traversal_mem_offs >= mem_required_for_traversal)
						{
						chk_report_abort("TALOS-2023-1785");
						}

                                        if(mc_mem_len < chain_table_lengths[i])
                                                {
                                                free(mc_mem);
                                                mc_mem = (unsigned char *)malloc(mc_mem_len = chain_table_lengths[i]);
                                                }
                                        mc = mc_mem;

                                        fstFread(mc, chain_table_lengths[i], 1, xc->f);

                                        switch(packtype)
                                                {
                                                case '4': rc = (destlen == (unsigned long)LZ4_decompress_safe_partial((char *)mc, (char *)mu, sourcelen, destlen, destlen)) ? Z_OK : Z_DATA_ERROR;
                                                          break;
                                                case 'F': fastlz_decompress(mc, sourcelen, mu, destlen); /* rc appears unreliable */
                                                          break;
                                                default:  rc = uncompress(mu, &destlen, mc, sourcelen);
                                                          break;
                                                }

                                        /* data to process is for(j=0;j<destlen;j++) in mu[j] */
                                        headptr[i] = traversal_mem_offs;
                                        length_remaining[i] = val;
                                        traversal_mem_offs += val;
                                        }
                                        else
                                        {
                                        int destlen = chain_table_lengths[i] - skiplen;
                                        unsigned char *mu = mem_for_traversal + traversal_mem_offs;

					if(traversal_mem_offs >= mem_required_for_traversal)
						{
						chk_report_abort("TALOS-2023-1785");
						}

                                        fstFread(mu, destlen, 1, xc->f);
                                        /* data to process is for(j=0;j<destlen;j++) in mu[j] */
                                        headptr[i] = traversal_mem_offs;
                                        length_remaining[i] = destlen;
                                        traversal_mem_offs += destlen;
                                        }

                                if(rc != Z_OK)
                                        {
                                        fprintf(stderr, FST_APIMESS "fstReaderIterBlocks2(), fac: %d clen: %d (rc=%d), exiting.\n", (int)i, (int)val, rc);
                                        exit(255);
                                        }

                                if(xc->signal_lens[i] == 1)
                                        {
                                        uint32_t vli = fstGetVarint32NoSkip(mem_for_traversal + headptr[i]);
                                        uint32_t shcnt = 2 << (vli & 1);
                                        tdelta = vli >> shcnt;
                                        }
                                        else
                                        {
                                        uint32_t vli = fstGetVarint32NoSkip(mem_for_traversal + headptr[i]);
                                        tdelta = vli >> 1;
                                        }

				if(tdelta >= tc_head_items)
					{
					chk_report_abort("TALOS-2023-1791");
					}

                                scatterptr[i] = tc_head[tdelta];
                                tc_head[tdelta] = i+1;
                                }
                        }
                }

        free(mc_mem); /* there is no usage below for this, no real need to clear out mc_mem or mc_mem_len */

        for(i=0;i<tsec_nitems;i++)
                {
                uint32_t tdelta;
                int skiplen, skiplen2;
                uint32_t vli;

                if(fv)
                        {
                        char wx_buf[32];
                        int wx_len;

                        if(time_table[i] != previous_time)
                                {
                                if(xc->limit_range_valid)
                                        {
                                        if(time_table[i] > xc->limit_range_end)
                                                {
                                                break;
                                                }
                                        }

				if(dumpvars_state == 1) { wx_len = snprintf(wx_buf, 32, "$end\n"); fstWritex(xc, wx_buf, wx_len); dumpvars_state = 2; }
                                wx_len = snprintf(wx_buf, 32, "#%" PRIu64 "\n", time_table[i]);
                                fstWritex(xc, wx_buf, wx_len);
				if(!dumpvars_state) { wx_len = snprintf(wx_buf, 32, "$dumpvars\n"); fstWritex(xc, wx_buf, wx_len); dumpvars_state = 1; }

                                if((xc->num_blackouts)&&(cur_blackout != xc->num_blackouts))
                                        {
                                        if(time_table[i] == xc->blackout_times[cur_blackout])
                                                {
                                                wx_len = snprintf(wx_buf, 32, "$dump%s $end\n", (xc->blackout_activity[cur_blackout++]) ? "on" : "off");
                                                fstWritex(xc, wx_buf, wx_len);
                                                }
                                        }
                                previous_time = time_table[i];
                                }
                        }

                while(tc_head[i])
                        {
                        idx = tc_head[i] - 1;
                        vli = fstGetVarint32(mem_for_traversal + headptr[idx], &skiplen);

                        if(xc->signal_lens[idx] <= 1)
                                {
                                if(xc->signal_lens[idx] == 1)
                                        {
                                        unsigned char val;
                                        if(!(vli & 1))
                                                {
                                                /* tdelta = vli >> 2; */ /* scan-build */
                                                val = ((vli >> 1) & 1) | '0';
                                                }
                                                else
                                                {
                                                /* tdelta = vli >> 4; */ /* scan-build */
                                                val = FST_RCV_STR[((vli >> 1) & 7)];
                                                }

                                        if(value_change_callback)
                                                {
                                                xc->temp_signal_value_buf[0] = val;
                                                xc->temp_signal_value_buf[1] = 0;
                                                value_change_callback(user_callback_data_pointer, time_table[i], idx+1, xc->temp_signal_value_buf);
                                                }
                                                else
                                                {
                                                if(fv)
                                                        {
                                                        char vcd_id[16];
                                                        int vcdid_len = fstVcdIDForFwrite(vcd_id+1, idx+1);

                                                        vcd_id[0] = val;
                                                        vcd_id[vcdid_len+1] = '\n';
                                                        fstWritex(xc, vcd_id, vcdid_len+2);
                                                        }
                                                }
                                        headptr[idx] += skiplen;
                                        length_remaining[idx] -= skiplen;

                                        tc_head[i] = scatterptr[idx];
                                        scatterptr[idx] = 0;

                                        if(length_remaining[idx])
                                                {
                                                int shamt;
                                                vli = fstGetVarint32NoSkip(mem_for_traversal + headptr[idx]);
                                                shamt = 2 << (vli & 1);
                                                tdelta = vli >> shamt;

						if((tdelta+i) >= tc_head_items)
							{
							chk_report_abort("TALOS-2023-1791");
							}

                                                scatterptr[idx] = tc_head[i+tdelta];
                                                tc_head[i+tdelta] = idx+1;
                                                }
                                        }
                                        else
                                        {
                                        unsigned char *vdata;
                                        uint32_t len;

                                        vli = fstGetVarint32(mem_for_traversal + headptr[idx], &skiplen);
                                        len = fstGetVarint32(mem_for_traversal + headptr[idx] + skiplen, &skiplen2);
                                        /* tdelta = vli >> 1; */ /* scan-build */
                                        skiplen += skiplen2;
                                        vdata = mem_for_traversal + headptr[idx] + skiplen;

                                        if(!(vli & 1))
                                                {
                                                if(value_change_callback_varlen)
                                                        {
                                                        value_change_callback_varlen(user_callback_data_pointer, time_table[i], idx+1, vdata, len);
                                                        }
                                                        else
                                                        {
                                                        if(fv)
                                                                {
                                                                char vcd_id[16];
                                                                int vcdid_len;

                                                                vcd_id[0] = 's';
                                                                fstWritex(xc, vcd_id, 1);

                                                                vcdid_len = fstVcdIDForFwrite(vcd_id+1, idx+1);
                                                                {
								if(sizeof(size_t) < sizeof(uint64_t))
                							{
                							/* TALOS-2023-1790 for 32b overflow */
                							uint64_t chk_64 = len*4 + 1;
                							size_t   chk_32 = len*4 + 1;
                							if(chk_64 != chk_32) chk_report_abort("TALOS-2023-1790");
                							}

                                                                unsigned char *vesc = (unsigned char *)malloc(len*4 + 1);
                                                                int vlen = fstUtilityBinToEsc(vesc, vdata, len);
                                                                fstWritex(xc, vesc, vlen);
                                                                free(vesc);
                                                                }

                                                                vcd_id[0] = ' ';
                                                                vcd_id[vcdid_len + 1] = '\n';
                                                                fstWritex(xc, vcd_id, vcdid_len+2);
                                                                }
                                                        }
                                                }

                                        skiplen += len;
                                        headptr[idx] += skiplen;
                                        length_remaining[idx] -= skiplen;

                                        tc_head[i] = scatterptr[idx];
                                        scatterptr[idx] = 0;

                                        if(length_remaining[idx])
                                                {
                                                vli = fstGetVarint32NoSkip(mem_for_traversal + headptr[idx]);
                                                tdelta = vli >> 1;

						if((tdelta+i) >= tc_head_items)
							{
							chk_report_abort("TALOS-2023-1791");
							}

                                                scatterptr[idx] = tc_head[i+tdelta];
                                                tc_head[i+tdelta] = idx+1;
                                                }
                                        }
                                }
                                else
                                {
                                uint32_t len = xc->signal_lens[idx];
                                unsigned char *vdata;

                                vli = fstGetVarint32(mem_for_traversal + headptr[idx], &skiplen);
                                /* tdelta = vli >> 1; */ /* scan-build */
                                vdata = mem_for_traversal + headptr[idx] + skiplen;

                                if(xc->signal_typs[idx] != FST_VT_VCD_REAL)
                                        {
					if(len > xc->longest_signal_value_len)
						{
						chk_report_abort("TALOS-2023-1797");
						}

                                        if(!(vli & 1))
                                                {
                                                int byte = 0;
                                                int bit;
                                                unsigned int j;

                                                for(j=0;j<len;j++)
                                                        {
                                                        unsigned char ch;
                                                        byte = j/8;
                                                        bit = 7 - (j & 7);
                                                        ch = ((vdata[byte] >> bit) & 1) | '0';
                                                        xc->temp_signal_value_buf[j] = ch;
                                                        }
                                                xc->temp_signal_value_buf[j] = 0;

                                                if(value_change_callback)
                                                        {
                                                        value_change_callback(user_callback_data_pointer, time_table[i], idx+1, xc->temp_signal_value_buf);
                                                        }
                                                        else
                                                        {
                                                        if(fv)  {
                                                                unsigned char ch_bp = (xc->signal_typs[idx] != FST_VT_VCD_PORT) ? 'b' : 'p';

                                                                fstWritex(xc, &ch_bp, 1);
                                                                fstWritex(xc, xc->temp_signal_value_buf, len);
                                                                }
                                                        }

                                                len = byte+1;
                                                }
                                                else
                                                {
                                                if(value_change_callback)
                                                        {
                                                        memcpy(xc->temp_signal_value_buf, vdata, len);
                                                        xc->temp_signal_value_buf[len] = 0;
                                                        value_change_callback(user_callback_data_pointer, time_table[i], idx+1, xc->temp_signal_value_buf);
                                                        }
                                                        else
                                                        {
                                                        if(fv)
                                                                {
                                                                unsigned char ch_bp =  (xc->signal_typs[idx] != FST_VT_VCD_PORT) ? 'b' : 'p';
								uint64_t mem_required_for_traversal_chk = vdata - mem_for_traversal + len;

                                                                fstWritex(xc, &ch_bp, 1);
								if(mem_required_for_traversal_chk > mem_required_for_traversal)
									{
									chk_report_abort("TALOS-2023-1793");
									}
                                                                fstWritex(xc, vdata, len);
                                                                }
                                                        }
                                                }
                                        }
                                        else
                                        {
                                        double d;
                                        unsigned char *clone_d /*= (unsigned char *)&d */; /* scan-build */
                                        unsigned char buf[8];
                                        unsigned char *srcdata;

                                        if(!(vli & 1))  /* very rare case, but possible */
                                                {
                                                int bit;
                                                int j;

                                                for(j=0;j<8;j++)
                                                        {
                                                        unsigned char ch;
                                                        bit = 7 - (j & 7);
                                                        ch = ((vdata[0] >> bit) & 1) | '0';
                                                        buf[j] = ch;
                                                        }

                                                len = 1;
                                                srcdata = buf;
                                                }
                                                else
                                                {
                                                srcdata = vdata;
                                                }

                                        if(value_change_callback)
                                                {
                                                if(xc->native_doubles_for_cb)
                                                        {
                                                        if(xc->double_endian_match)
                                                                {
                                                                clone_d = srcdata;
                                                                }
                                                                else
                                                                {
                                                                int j;

                                                                clone_d = (unsigned char *)&d;
                                                                for(j=0;j<8;j++)
                                                                        {
                                                                        clone_d[j] = srcdata[7-j];
                                                                        }
                                                                }
                                                        value_change_callback(user_callback_data_pointer, time_table[i], idx+1, clone_d);
                                                        }
                                                        else
                                                        {
                                                        clone_d = (unsigned char *)&d;
                                                        if(xc->double_endian_match)
                                                                {
                                                                memcpy(clone_d, srcdata, 8);
                                                                }
                                                                else
                                                                {
                                                                int j;

                                                                for(j=0;j<8;j++)
                                                                        {
                                                                        clone_d[j] = srcdata[7-j];
                                                                        }
                                                                }
                                                        snprintf((char *)xc->temp_signal_value_buf, xc->longest_signal_value_len + 1, "%.16g", d);
                                                        value_change_callback(user_callback_data_pointer, time_table[i], idx+1, xc->temp_signal_value_buf);
                                                        }
                                                }
                                                else
                                                {
                                                if(fv)
                                                        {
                                                        char wx_buf[32];
                                                        int wx_len;

                                                        clone_d = (unsigned char *)&d;
                                                        if(xc->double_endian_match)
                                                                {
                                                                memcpy(clone_d, srcdata, 8);
                                                                }
                                                                else
                                                                {
                                                                int j;

                                                                for(j=0;j<8;j++)
                                                                        {
                                                                        clone_d[j] = srcdata[7-j];
                                                                        }
                                                                }

                                                        wx_len = snprintf(wx_buf, 32, "r%.16g", d);
                                                        fstWritex(xc, wx_buf, wx_len);
                                                        }
                                                }
                                        }

                                if(fv)
                                        {
                                        char vcd_id[16];
                                        int vcdid_len = fstVcdIDForFwrite(vcd_id+1, idx+1);
                                        vcd_id[0] = ' ';
                                        vcd_id[vcdid_len+1] = '\n';
                                        fstWritex(xc, vcd_id, vcdid_len+2);
                                        }

                                skiplen += len;
                                headptr[idx] += skiplen;
                                length_remaining[idx] -= skiplen;

                                tc_head[i] = scatterptr[idx];
                                scatterptr[idx] = 0;

                                if(length_remaining[idx])
                                        {
                                        vli = fstGetVarint32NoSkip(mem_for_traversal + headptr[idx]);
                                        tdelta = vli >> 1;

					if((tdelta+i) >= tc_head_items)
						{
						chk_report_abort("TALOS-2023-1791");
						}

                                        scatterptr[idx] = tc_head[i+tdelta];
                                        tc_head[i+tdelta] = idx+1;
                                        }
                                }
                        }
                }

block_err:
        free(tc_head);
        free(chain_cmem);
        free(mem_for_traversal); mem_for_traversal = NULL;

        secnum++;
        if(secnum == xc->vc_section_count) break; /* in case file is growing, keep with original block count */
        blkpos += seclen;
        }

if(mem_for_traversal) free(mem_for_traversal); /* scan-build */
free(length_remaining);
free(headptr);
free(scatterptr);

if(chain_table) free(chain_table);
if(chain_table_lengths) free(chain_table_lengths);

free(time_table);

#ifndef FST_WRITEX_DISABLE
if(fv)
        {
        fstWritex(xc, NULL, 0);
        }
#endif

return(1);
}


/* rvat functions */

static char *fstExtractRvatDataFromFrame(struct fstReaderContext *xc, fstHandle facidx, char *buf)
{
if(facidx >= xc->rvat_frame_maxhandle)
        {
        return(NULL);
        }

if(xc->signal_lens[facidx] == 1)
        {
        buf[0] = (char)xc->rvat_frame_data[xc->rvat_sig_offs[facidx]];
        buf[1] = 0;
        }
        else
        {
        if(xc->signal_typs[facidx] != FST_VT_VCD_REAL)
                {
                memcpy(buf, xc->rvat_frame_data + xc->rvat_sig_offs[facidx], xc->signal_lens[facidx]);
                buf[xc->signal_lens[facidx]] = 0;
                }
                else
                {
                double d;
                unsigned char *clone_d = (unsigned char *)&d;
                unsigned char *srcdata = xc->rvat_frame_data + xc->rvat_sig_offs[facidx];

                if(xc->double_endian_match)
                        {
                        memcpy(clone_d, srcdata, 8);
                        }
                        else
                        {
                        int j;

                        for(j=0;j<8;j++)
                                {
                                clone_d[j] = srcdata[7-j];
                                }
                        }

                snprintf((char *)buf, 32, "%.16g", d); /* this will write 18 bytes */
                }
        }

return(buf);
}


char *fstReaderGetValueFromHandleAtTime(void *ctx, uint64_t tim, fstHandle facidx, char *buf)
{
struct fstReaderContext *xc = (struct fstReaderContext *)ctx;
fst_off_t blkpos = 0, prev_blkpos;
uint64_t beg_tim, end_tim, beg_tim2, end_tim2;
int sectype;
#ifdef FST_DEBUG
unsigned int secnum = 0;
#endif
uint64_t seclen;
uint64_t tsec_uclen = 0, tsec_clen = 0;
uint64_t tsec_nitems;
uint64_t frame_uclen, frame_clen;
#ifdef FST_DEBUG
uint64_t mem_required_for_traversal;
#endif
fst_off_t indx_pntr, indx_pos;
long chain_clen;
unsigned char *chain_cmem;
unsigned char *pnt;
fstHandle idx, pidx=0, i;
uint64_t pval;

if((!xc) || (!facidx) || (facidx > xc->maxhandle) || (!buf) || (!xc->signal_lens[facidx-1]))
        {
        return(NULL);
        }

if(!xc->rvat_sig_offs)
        {
        uint32_t cur_offs = 0;

        xc->rvat_sig_offs = (uint32_t *)calloc(xc->maxhandle, sizeof(uint32_t));
        for(i=0;i<xc->maxhandle;i++)
                {
                xc->rvat_sig_offs[i] = cur_offs;
                cur_offs += xc->signal_lens[i];
                }
        }

if(xc->rvat_data_valid)
        {
        if((xc->rvat_beg_tim <= tim) && (tim <= xc->rvat_end_tim))
                {
                goto process_value;
                }

        fstReaderDeallocateRvatData(xc);
        }

xc->rvat_chain_pos_valid = 0;

for(;;)
        {
        fstReaderFseeko(xc, xc->f, (prev_blkpos = blkpos), SEEK_SET);

        sectype = fgetc(xc->f);
        seclen = fstReaderUint64(xc->f);

        if((sectype == EOF) || (sectype == FST_BL_SKIP) || (!seclen))
                {
                return(NULL); /* if this loop exits on break, it's successful */
                }

        blkpos++;
        if((sectype != FST_BL_VCDATA) && (sectype != FST_BL_VCDATA_DYN_ALIAS) && (sectype != FST_BL_VCDATA_DYN_ALIAS2))
                {
                blkpos += seclen;
                continue;
                }

        beg_tim = fstReaderUint64(xc->f);
        end_tim = fstReaderUint64(xc->f);

        if((beg_tim <= tim) && (tim <= end_tim))
                {
                if((tim == end_tim) && (tim != xc->end_time))
                        {
                        fst_off_t cached_pos = ftello(xc->f);
                        fstReaderFseeko(xc, xc->f, blkpos, SEEK_SET);

                        sectype = fgetc(xc->f);
                        seclen = fstReaderUint64(xc->f);

                        beg_tim2 = fstReaderUint64(xc->f);
                        end_tim2 = fstReaderUint64(xc->f);

                        if(((sectype != FST_BL_VCDATA)&&(sectype != FST_BL_VCDATA_DYN_ALIAS)&&(sectype != FST_BL_VCDATA_DYN_ALIAS2)) || (!seclen) || (beg_tim2 != tim))
                                {
                                blkpos = prev_blkpos;
                                break;
                                }
                        beg_tim = beg_tim2;
                        end_tim = end_tim2;
                        fstReaderFseeko(xc, xc->f, cached_pos, SEEK_SET);
                        }
                break;
                }

        blkpos += seclen;
#ifdef FST_DEBUG
        secnum++;
#endif
        }

xc->rvat_beg_tim = beg_tim;
xc->rvat_end_tim = end_tim;

#ifdef FST_DEBUG
mem_required_for_traversal =
#endif
        fstReaderUint64(xc->f);

#ifdef FST_DEBUG
fprintf(stderr, FST_APIMESS "rvat sec: %u seclen: %d begtim: %d endtim: %d\n",
        secnum, (int)seclen, (int)beg_tim, (int)end_tim);
fprintf(stderr, FST_APIMESS "mem_required_for_traversal: %d\n", (int)mem_required_for_traversal);
#endif

/* process time block */
{
unsigned char *ucdata;
unsigned char *cdata;
unsigned long destlen /* = tsec_uclen */; /* scan-build */
unsigned long sourcelen /* = tsec_clen */; /* scan-build */
int rc;
unsigned char *tpnt;
uint64_t tpval;
unsigned int ti;

fstReaderFseeko(xc, xc->f, blkpos + seclen - 24, SEEK_SET);
tsec_uclen = fstReaderUint64(xc->f);
tsec_clen = fstReaderUint64(xc->f);
tsec_nitems = fstReaderUint64(xc->f);
#ifdef FST_DEBUG
fprintf(stderr, FST_APIMESS "time section unc: %d, com: %d (%d items)\n",
        (int)tsec_uclen, (int)tsec_clen, (int)tsec_nitems);
#endif
ucdata = (unsigned char *)malloc(tsec_uclen);
destlen = tsec_uclen;
sourcelen = tsec_clen;

fstReaderFseeko(xc, xc->f, -24 - ((fst_off_t)tsec_clen), SEEK_CUR);
if(tsec_uclen != tsec_clen)
        {
        cdata = (unsigned char *)malloc(tsec_clen);
        fstFread(cdata, tsec_clen, 1, xc->f);

        rc = uncompress(ucdata, &destlen, cdata, sourcelen);

        if(rc != Z_OK)
                {
                fprintf(stderr, FST_APIMESS "fstReaderGetValueFromHandleAtTime(), tsec uncompress rc = %d, exiting.\n", rc);
                exit(255);
                }

        free(cdata);
        }
        else
        {
        fstFread(ucdata, tsec_uclen, 1, xc->f);
        }

xc->rvat_time_table = (uint64_t *)calloc(tsec_nitems, sizeof(uint64_t));
tpnt = ucdata;
tpval = 0;
for(ti=0;ti<tsec_nitems;ti++)
        {
        int skiplen;
        uint64_t val = fstGetVarint64(tpnt, &skiplen);
        tpval = xc->rvat_time_table[ti] = tpval + val;
        tpnt += skiplen;
        }

free(ucdata);
}

fstReaderFseeko(xc, xc->f, blkpos+32, SEEK_SET);

frame_uclen = fstReaderVarint64(xc->f);
frame_clen = fstReaderVarint64(xc->f);
xc->rvat_frame_maxhandle = fstReaderVarint64(xc->f);
xc->rvat_frame_data = (unsigned char *)malloc(frame_uclen);

if(frame_uclen == frame_clen)
        {
        fstFread(xc->rvat_frame_data, frame_uclen, 1, xc->f);
        }
        else
        {
        unsigned char *mc = (unsigned char *)malloc(frame_clen);
        int rc;

        unsigned long destlen = frame_uclen;
        unsigned long sourcelen = frame_clen;

        fstFread(mc, sourcelen, 1, xc->f);
        rc = uncompress(xc->rvat_frame_data, &destlen, mc, sourcelen);
        if(rc != Z_OK)
                {
                fprintf(stderr, FST_APIMESS "fstReaderGetValueFromHandleAtTime(), frame decompress rc: %d, exiting.\n", rc);
                exit(255);
                }
        free(mc);
        }

xc->rvat_vc_maxhandle = fstReaderVarint64(xc->f);
xc->rvat_vc_start = ftello(xc->f);      /* points to '!' character */
xc->rvat_packtype = fgetc(xc->f);

#ifdef FST_DEBUG
fprintf(stderr, FST_APIMESS "frame_uclen: %d, frame_clen: %d, frame_maxhandle: %d\n",
        (int)frame_uclen, (int)frame_clen, (int)xc->rvat_frame_maxhandle);
fprintf(stderr, FST_APIMESS "vc_maxhandle: %d\n", (int)xc->rvat_vc_maxhandle);
#endif

indx_pntr = blkpos + seclen - 24 -tsec_clen -8;
fstReaderFseeko(xc, xc->f, indx_pntr, SEEK_SET);
chain_clen = fstReaderUint64(xc->f);
indx_pos = indx_pntr - chain_clen;
#ifdef FST_DEBUG
fprintf(stderr, FST_APIMESS "indx_pos: %d (%d bytes)\n", (int)indx_pos, (int)chain_clen);
#endif
chain_cmem = (unsigned char *)malloc(chain_clen);
fstReaderFseeko(xc, xc->f, indx_pos, SEEK_SET);
fstFread(chain_cmem, chain_clen, 1, xc->f);

xc->rvat_chain_table = (fst_off_t *)calloc((xc->rvat_vc_maxhandle+1), sizeof(fst_off_t));
xc->rvat_chain_table_lengths = (uint32_t *)calloc((xc->rvat_vc_maxhandle+1), sizeof(uint32_t));

pnt = chain_cmem;
idx = 0;
pval = 0;

if(sectype == FST_BL_VCDATA_DYN_ALIAS2)
        {
        uint32_t prev_alias = 0;

        do      {
                int skiplen;

                if(*pnt & 0x01)
                        {
                        int64_t shval = fstGetSVarint64(pnt, &skiplen) >> 1;
                        if(shval > 0)
                                {
                                pval = xc->rvat_chain_table[idx] = pval + shval;
                                if(idx) { xc->rvat_chain_table_lengths[pidx] = pval - xc->rvat_chain_table[pidx]; }
                                pidx = idx++;
                                }
                        else if(shval < 0)
                                {
                                xc->rvat_chain_table[idx] = 0;                                   /* need to explicitly zero as calloc above might not run */
                                xc->rvat_chain_table_lengths[idx] = prev_alias = shval;          /* because during this loop iter would give stale data! */
                                idx++;
                                }
                        else
                                {
                                xc->rvat_chain_table[idx] = 0;                                   /* need to explicitly zero as calloc above might not run */
                                xc->rvat_chain_table_lengths[idx] = prev_alias;                  /* because during this loop iter would give stale data! */
                                idx++;
                                }
                        }
                        else
                        {
                        uint64_t val = fstGetVarint32(pnt, &skiplen);

                        fstHandle loopcnt = val >> 1;
                        for(i=0;i<loopcnt;i++)
                                {
                                xc->rvat_chain_table[idx++] = 0;
                                }
                        }

                pnt += skiplen;
                } while (pnt != (chain_cmem + chain_clen));
        }
        else
	{
	do
	        {
	        int skiplen;
	        uint64_t val = fstGetVarint32(pnt, &skiplen);

	        if(!val)
	                {
	                pnt += skiplen;
	                val = fstGetVarint32(pnt, &skiplen);
	                xc->rvat_chain_table[idx] = 0;
	                xc->rvat_chain_table_lengths[idx] = -val;
	                idx++;
	                }
	        else
	        if(val&1)
	                {
	                pval = xc->rvat_chain_table[idx] = pval + (val >> 1);
	                if(idx) { xc->rvat_chain_table_lengths[pidx] = pval - xc->rvat_chain_table[pidx]; }
	                pidx = idx++;
	                }
	                else
	                {
	                fstHandle loopcnt = val >> 1;
	                for(i=0;i<loopcnt;i++)
	                        {
	                        xc->rvat_chain_table[idx++] = 0;
	                        }
	                }

	        pnt += skiplen;
	        } while (pnt != (chain_cmem + chain_clen));
	}

free(chain_cmem);
xc->rvat_chain_table[idx] = indx_pos - xc->rvat_vc_start;
xc->rvat_chain_table_lengths[pidx] = xc->rvat_chain_table[idx] - xc->rvat_chain_table[pidx];

for(i=0;i<idx;i++)
        {
        int32_t v32 = xc->rvat_chain_table_lengths[i];
        if((v32 < 0) && (!xc->rvat_chain_table[i]))
                {
                v32 = -v32;
                v32--;
                if(((uint32_t)v32) < i) /* sanity check */
                        {
                        xc->rvat_chain_table[i] = xc->rvat_chain_table[v32];
                        xc->rvat_chain_table_lengths[i] = xc->rvat_chain_table_lengths[v32];
                        }
                }
        }

#ifdef FST_DEBUG
fprintf(stderr, FST_APIMESS "decompressed chain idx len: %" PRIu32 "\n", idx);
#endif

xc->rvat_data_valid = 1;

/* all data at this point is loaded or resident in fst cache, process and return appropriate value */
process_value:
if(facidx > xc->rvat_vc_maxhandle)
        {
        return(NULL);
        }

facidx--; /* scale down for array which starts at zero */


if(((tim == xc->rvat_beg_tim)&&(!xc->rvat_chain_table[facidx])) || (!xc->rvat_chain_table[facidx]))
        {
        return(fstExtractRvatDataFromFrame(xc, facidx, buf));
        }

if(facidx != xc->rvat_chain_facidx)
        {
        if(xc->rvat_chain_mem)
                {
                free(xc->rvat_chain_mem);
                xc->rvat_chain_mem = NULL;

                xc->rvat_chain_pos_valid = 0;
                }
        }

if(!xc->rvat_chain_mem)
        {
        uint32_t skiplen;
        fstReaderFseeko(xc, xc->f, xc->rvat_vc_start + xc->rvat_chain_table[facidx], SEEK_SET);
        xc->rvat_chain_len = fstReaderVarint32WithSkip(xc->f, &skiplen);
        if(xc->rvat_chain_len)
                {
                unsigned char *mu = (unsigned char *)malloc(xc->rvat_chain_len);
                unsigned char *mc = (unsigned char *)malloc(xc->rvat_chain_table_lengths[facidx]);
                unsigned long destlen = xc->rvat_chain_len;
                unsigned long sourcelen = xc->rvat_chain_table_lengths[facidx];
                int rc = Z_OK;

                fstFread(mc, xc->rvat_chain_table_lengths[facidx], 1, xc->f);

                switch(xc->rvat_packtype)
			{
                        case '4': rc = (destlen == (unsigned long)LZ4_decompress_safe_partial((char *)mc, (char *)mu, sourcelen, destlen, destlen)) ? Z_OK : Z_DATA_ERROR;
                        	break;
                        case 'F': fastlz_decompress(mc, sourcelen, mu, destlen); /* rc appears unreliable */
                        	break;
                        default:  rc = uncompress(mu, &destlen, mc, sourcelen);
                        	break;
                        }

                free(mc);

                if(rc != Z_OK)
                        {
                        fprintf(stderr, FST_APIMESS "fstReaderGetValueFromHandleAtTime(), rvat decompress clen: %d (rc=%d), exiting.\n", (int)xc->rvat_chain_len, rc);
                        exit(255);
                        }

                /* data to process is for(j=0;j<destlen;j++) in mu[j] */
                xc->rvat_chain_mem = mu;
                }
                else
                {
                int destlen = xc->rvat_chain_table_lengths[facidx] - skiplen;
                unsigned char *mu = (unsigned char *)malloc(xc->rvat_chain_len = destlen);
                fstFread(mu, destlen, 1, xc->f);
                /* data to process is for(j=0;j<destlen;j++) in mu[j] */
                xc->rvat_chain_mem = mu;
                }

        xc->rvat_chain_facidx = facidx;
        }

/* process value chain here */

{
uint32_t tidx = 0, ptidx = 0;
uint32_t tdelta;
int skiplen;
unsigned int iprev = xc->rvat_chain_len;
uint32_t pvli = 0;
int pskip = 0;

if((xc->rvat_chain_pos_valid)&&(tim >= xc->rvat_chain_pos_time))
        {
        i = xc->rvat_chain_pos_idx;
        tidx = xc->rvat_chain_pos_tidx;
        }
        else
        {
        i = 0;
        tidx = 0;
        xc->rvat_chain_pos_time = xc->rvat_beg_tim;
        }

if(xc->signal_lens[facidx] == 1)
        {
        while(i<xc->rvat_chain_len)
                {
                uint32_t vli = fstGetVarint32(xc->rvat_chain_mem + i, &skiplen);
                uint32_t shcnt = 2 << (vli & 1);
                tdelta = vli >> shcnt;

                if(xc->rvat_time_table[tidx + tdelta] <= tim)
                        {
                        iprev = i;
                        pvli = vli;
                        ptidx = tidx;
                        /* pskip = skiplen; */ /* scan-build */

                        tidx += tdelta;
                        i+=skiplen;
                        }
                        else
                        {
                        break;
                        }
                }
        if(iprev != xc->rvat_chain_len)
                {
                xc->rvat_chain_pos_tidx = ptidx;
                xc->rvat_chain_pos_idx = iprev;
                xc->rvat_chain_pos_time = tim;
                xc->rvat_chain_pos_valid = 1;

                if(!(pvli & 1))
                        {
                        buf[0] = ((pvli >> 1) & 1) | '0';
                        }
                        else
                        {
                        buf[0] = FST_RCV_STR[((pvli >> 1) & 7)];
                        }
                buf[1] = 0;
                return(buf);
                }
                else
                {
                return(fstExtractRvatDataFromFrame(xc, facidx, buf));
                }
        }
        else
        {
        while(i<xc->rvat_chain_len)
                {
                uint32_t vli = fstGetVarint32(xc->rvat_chain_mem + i, &skiplen);
                tdelta = vli >> 1;

                if(xc->rvat_time_table[tidx + tdelta] <= tim)
                        {
                        iprev = i;
                        pvli = vli;
                        ptidx = tidx;
                        pskip = skiplen;

                        tidx += tdelta;
                        i+=skiplen;

                        if(!(pvli & 1))
                                {
                                i+=((xc->signal_lens[facidx]+7)/8);
                                }
                                else
                                {
                                i+=xc->signal_lens[facidx];
                                }
                        }
                        else
                        {
                        break;
                        }
                }

        if(iprev != xc->rvat_chain_len)
                {
                unsigned char *vdata = xc->rvat_chain_mem + iprev + pskip;

                xc->rvat_chain_pos_tidx = ptidx;
                xc->rvat_chain_pos_idx = iprev;
                xc->rvat_chain_pos_time = tim;
                xc->rvat_chain_pos_valid = 1;

                if(xc->signal_typs[facidx] != FST_VT_VCD_REAL)
                        {
                        if(!(pvli & 1))
                                {
                                int byte = 0;
                                int bit;
                                unsigned int j;

                                for(j=0;j<xc->signal_lens[facidx];j++)
                                        {
                                        unsigned char ch;
                                        byte = j/8;
                                        bit = 7 - (j & 7);
                                        ch = ((vdata[byte] >> bit) & 1) | '0';
                                        buf[j] = ch;
                                        }
                                buf[j] = 0;

                                return(buf);
                                }
                                else
                                {
                                memcpy(buf, vdata, xc->signal_lens[facidx]);
                                buf[xc->signal_lens[facidx]] = 0;
                                return(buf);
                                }
                        }
                        else
                        {
                        double d;
                        unsigned char *clone_d = (unsigned char *)&d;
                        unsigned char bufd[8];
                        unsigned char *srcdata;

                        if(!(pvli & 1)) /* very rare case, but possible */
                                {
                                int bit;
                                int j;

                                for(j=0;j<8;j++)
                                        {
                                        unsigned char ch;
                                        bit = 7 - (j & 7);
                                        ch = ((vdata[0] >> bit) & 1) | '0';
                                        bufd[j] = ch;
                                        }

                                srcdata = bufd;
                                }
                                else
                                {
                                srcdata = vdata;
                                }

                        if(xc->double_endian_match)
                                {
                                memcpy(clone_d, srcdata, 8);
                                }
                                else
                                {
                                int j;

                                for(j=0;j<8;j++)
                                        {
                                        clone_d[j] = srcdata[7-j];
                                        }
                                }

                        snprintf(buf, 32, "r%.16g", d); /* this will write 19 bytes */
                        return(buf);
                        }
                }
                else
                {
                return(fstExtractRvatDataFromFrame(xc, facidx, buf));
                }
        }
}

/* return(NULL); */
}



/**********************************************************************/
#ifndef _WAVE_HAVE_JUDY

/***********************/
/***                 ***/
/***  jenkins hash   ***/
/***                 ***/
/***********************/

/*
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bits set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a
  structure that could supported 2x parallelism, like so:
      a -= b;
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/*
--------------------------------------------------------------------
j_hash() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  len     : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 6*len+35 instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

static uint32_t j_hash(const uint8_t *k, uint32_t length, uint32_t initval)
{
   uint32_t a,b,c,len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;         /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((uint32_t)k[1]<<8) +((uint32_t)k[2]<<16) +((uint32_t)k[3]<<24));
      b += (k[4] +((uint32_t)k[5]<<8) +((uint32_t)k[6]<<16) +((uint32_t)k[7]<<24));
      c += (k[8] +((uint32_t)k[9]<<8) +((uint32_t)k[10]<<16)+((uint32_t)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((uint32_t)k[10]<<24); /* fallthrough */
   case 10: c+=((uint32_t)k[9]<<16); /* fallthrough */
   case 9 : c+=((uint32_t)k[8]<<8); /* fallthrough */
      /* the first byte of c is reserved for the length */
   case 8 : b+=((uint32_t)k[7]<<24); /* fallthrough */
   case 7 : b+=((uint32_t)k[6]<<16); /* fallthrough */
   case 6 : b+=((uint32_t)k[5]<<8); /* fallthrough */
   case 5 : b+=k[4]; /* fallthrough */
   case 4 : a+=((uint32_t)k[3]<<24); /* fallthrough */
   case 3 : a+=((uint32_t)k[2]<<16); /* fallthrough */
   case 2 : a+=((uint32_t)k[1]<<8); /* fallthrough */
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);
   /*-------------------------------------------- report the result */
   return(c);
}

/********************************************************************/

/***************************/
/***                     ***/
/***  judy HS emulation  ***/
/***                     ***/
/***************************/

struct collchain_t
{
struct collchain_t *next;
void *payload;
uint32_t fullhash, length;
unsigned char mem[1];
};


void **JenkinsIns(void *base_i, const unsigned char *mem, uint32_t length, uint32_t hashmask)
{
struct collchain_t ***base = (struct collchain_t ***)base_i;
uint32_t hf, h;
struct collchain_t **ar;
struct collchain_t *chain, *pchain;

if(!*base)
        {
        *base = (struct collchain_t **)calloc(1, (hashmask + 1) * sizeof(void *));
        }
ar = *base;

h = (hf = j_hash(mem, length, length)) & hashmask;
pchain = chain = ar[h];
while(chain)
        {
        if((chain->fullhash == hf) && (chain->length == length) && !memcmp(chain->mem, mem, length))
                {
                if(pchain != chain) /* move hit to front */
                        {
                        pchain->next = chain->next;
                        chain->next = ar[h];
                        ar[h] = chain;
                        }
                return(&(chain->payload));
                }

        pchain = chain;
        chain = chain->next;
        }

chain = (struct collchain_t *)calloc(1, sizeof(struct collchain_t) + length - 1);
memcpy(chain->mem, mem, length);
chain->fullhash = hf;
chain->length = length;
chain->next = ar[h];
ar[h] = chain;
return(&(chain->payload));
}


void JenkinsFree(void *base_i, uint32_t hashmask)
{
struct collchain_t ***base = (struct collchain_t ***)base_i;
uint32_t h;
struct collchain_t **ar;
struct collchain_t *chain, *chain_next;

if(base && *base)
        {
        ar = *base;
        for(h=0;h<=hashmask;h++)
                {
                chain = ar[h];
                while(chain)
                        {
                        chain_next = chain->next;
                        free(chain);
                        chain = chain_next;
                        }
                }

        free(*base);
        *base = NULL;
        }
}

#endif

/**********************************************************************/

/************************/
/***                  ***/
/*** utility function ***/
/***                  ***/
/************************/

int fstUtilityBinToEscConvertedLen(const unsigned char *s, int len)
{
const unsigned char *src = s;
int dlen = 0;
int i;

for(i=0;i<len;i++)
        {
        switch(src[i])
                {
                case '\a':      /* fallthrough */
                case '\b':	/* fallthrough */
                case '\f':	/* fallthrough */
                case '\n':	/* fallthrough */
                case '\r':	/* fallthrough */
                case '\t':	/* fallthrough */
                case '\v':	/* fallthrough */
                case '\'':	/* fallthrough */
                case '\"':	/* fallthrough */
                case '\\':	/* fallthrough */
                case '\?':      dlen += 2; break;
                default:        if((src[i] > ' ') && (src[i] <= '~')) /* no white spaces in output */
                                        {
					dlen++;
                                        }
                                        else
                                        {
					dlen += 4;
                                        }
                                break;
                }
        }

return(dlen);
}


int fstUtilityBinToEsc(unsigned char *d, const unsigned char *s, int len)
{
const unsigned char *src = s;
unsigned char *dst = d;
unsigned char val;
int i;

for(i=0;i<len;i++)
        {
        switch(src[i])
                {
                case '\a':      *(dst++) = '\\'; *(dst++) = 'a'; break;
                case '\b':      *(dst++) = '\\'; *(dst++) = 'b'; break;
                case '\f':      *(dst++) = '\\'; *(dst++) = 'f'; break;
                case '\n':      *(dst++) = '\\'; *(dst++) = 'n'; break;
                case '\r':      *(dst++) = '\\'; *(dst++) = 'r'; break;
                case '\t':      *(dst++) = '\\'; *(dst++) = 't'; break;
                case '\v':      *(dst++) = '\\'; *(dst++) = 'v'; break;
                case '\'':      *(dst++) = '\\'; *(dst++) = '\''; break;
                case '\"':      *(dst++) = '\\'; *(dst++) = '\"'; break;
                case '\\':      *(dst++) = '\\'; *(dst++) = '\\'; break;
                case '\?':      *(dst++) = '\\'; *(dst++) = '\?'; break;
                default:        if((src[i] > ' ') && (src[i] <= '~')) /* no white spaces in output */
                                        {
                                        *(dst++) = src[i];
                                        }
                                        else
                                        {
                                        val = src[i];
                                        *(dst++) = '\\';
                                        *(dst++) = (val/64) + '0'; val = val & 63;
                                        *(dst++) = (val/8)  + '0'; val = val & 7;
                                        *(dst++) = (val) + '0';
                                        }
                                break;
                }
        }

return(dst - d);
}


/*
 * this overwrites the original string if the destination pointer is NULL
 */
int fstUtilityEscToBin(unsigned char *d, unsigned char *s, int len)
{
unsigned char *src = s;
unsigned char *dst = (!d) ? s : (s = d);
unsigned char val[3];
int i;

for(i=0;i<len;i++)
        {
        if(src[i] != '\\')
                {
                *(dst++) = src[i];
                }
                else
                {
                switch(src[++i])
                        {
                        case 'a':       *(dst++) = '\a'; break;
                        case 'b':       *(dst++) = '\b'; break;
                        case 'f':       *(dst++) = '\f'; break;
                        case 'n':       *(dst++) = '\n'; break;
                        case 'r':       *(dst++) = '\r'; break;
                        case 't':       *(dst++) = '\t'; break;
                        case 'v':       *(dst++) = '\v'; break;
                        case '\'':      *(dst++) = '\''; break;
                        case '\"':      *(dst++) = '\"'; break;
                        case '\\':      *(dst++) = '\\'; break;
                        case '\?':      *(dst++) = '\?'; break;

                        case 'x':       val[0] = toupper(src[++i]);
                                        val[1] = toupper(src[++i]);
                                        val[0] = ((val[0]>='A')&&(val[0]<='F')) ? (val[0] - 'A' + 10) : (val[0] - '0');
                                        val[1] = ((val[1]>='A')&&(val[1]<='F')) ? (val[1] - 'A' + 10) : (val[1] - '0');
                                        *(dst++) = val[0] * 16 + val[1];
                                        break;

                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':       val[0] = src[  i] - '0';
                                        val[1] = src[++i] - '0';
                                        val[2] = src[++i] - '0';
                                        *(dst++) = val[0] * 64 + val[1] * 8 + val[2];
                                        break;

                        default:        *(dst++) = src[i]; break;
                        }
                }
        }

return(dst - s);
}


struct fstETab *fstUtilityExtractEnumTableFromString(const char *s)
{
struct fstETab *et = NULL;
int num_spaces = 0;
int i;
int newlen;

if(s)
	{
	const char *csp = strchr(s, ' ');
	int cnt = atoi(csp+1);

	for(;;)
		{
		csp = strchr(csp+1, ' ');
		if(csp) { num_spaces++; } else { break; }
		}

	if(num_spaces == (2*cnt))
		{
		char *sp, *sp2;

		et = (struct fstETab*)calloc(1, sizeof(struct fstETab));
		et->elem_count = cnt;
		et->name = strdup(s);
		et->literal_arr = (char**)calloc(cnt, sizeof(char *));
		et->val_arr = (char**)calloc(cnt, sizeof(char *));

		sp = strchr(et->name, ' ');
		*sp = 0;

		sp = strchr(sp+1, ' ');

		for(i=0;i<cnt;i++)
			{
			sp2 = strchr(sp+1, ' ');
			*(char*)sp2 = 0;
			et->literal_arr[i] = sp+1;
			sp = sp2;

			newlen = fstUtilityEscToBin(NULL, (unsigned char*)et->literal_arr[i], strlen(et->literal_arr[i]));
			et->literal_arr[i][newlen] = 0;
			}

		for(i=0;i<cnt;i++)
			{
			sp2 = strchr(sp+1, ' ');
			if(sp2) { *sp2 = 0; }
			et->val_arr[i] = sp+1;
			sp = sp2;

			newlen = fstUtilityEscToBin(NULL, (unsigned char*)et->val_arr[i], strlen(et->val_arr[i]));
			et->val_arr[i][newlen] = 0;
			}
		}
	}

return(et);
}

void fstUtilityFreeEnumTable(struct fstETab *etab)
{
if(etab)
	{
	free(etab->literal_arr);
	free(etab->val_arr);
	free(etab->name);
	free(etab);
	}
}
#ifdef __cplusplus
}
#endif
#endif /* GSIM_FST_IMPL */

#endif /* GSIM_FST_H */
