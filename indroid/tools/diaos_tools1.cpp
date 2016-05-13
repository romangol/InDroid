#include "indroid/tools/diaos_tools.h"

/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * The "dexdump" tool is intended to mimic "objdump".  When possible, use
 * similar command-line arguments.
 *
 * TODO: rework the "plain" output format to be more regexp-friendly
 *
 * Differences between XML output and the "current.xml" file:
 * - classes in same package are not all grouped together; generally speaking
 *   nothing is sorted
 * - no "deprecated" on fields and methods
 * - no "value" on fields
 * - no parameter names
 * - no generic signatures on parameters, e.g. type="java.lang.Class&lt;?&gt;"
 * - class shows declared fields and methods; does not show inherited fields
 */

#include "libdex/DexFile.h"

#include "libdex/CmdUtils.h"
#include "libdex/DexCatch.h"
#include "libdex/DexClass.h"
#include "libdex/DexDebugInfo.h"
#include "libdex/DexOpcodes.h"
#include "libdex/DexProto.h"
#include "libdex/InstrUtils.h"
#include "libdex/SysUtil.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>

static const char* gProgName = "dexdump";
FILE * dexFile;

enum OutputFormat {
    OUTPUT_PLAIN = 0,               /* default */
    OUTPUT_XML,                     /* fancy */
};

/* command-line options */
struct Options {
    bool checksumOnly;
    bool disassemble;
    bool showFileHeaders;
    bool showSectionHeaders;
    bool ignoreBadChecksum;
    bool dumpRegisterMaps;
    OutputFormat outputFormat;
    const char* tempFileName;
    bool exportsOnly;
    bool verbose;
};

struct Options gOptions;

/* basic info about a field or method */
struct FieldMethodInfo {
    const char* classDescriptor;
    const char* name;
    const char* signature;
};

/*
 * Converts a single-character primitive type into its human-readable
 * equivalent.
 */
static const char* primitiveTypeLabel(char typeChar)
{
    switch (typeChar) {
    case 'B':   return "byte";
    case 'C':   return "char";
    case 'D':   return "double";
    case 'F':   return "float";
    case 'I':   return "int";
    case 'J':   return "long";
    case 'S':   return "short";
    case 'V':   return "void";
    case 'Z':   return "boolean";
    default:
                return "UNKNOWN";
    }
}

/*
 * Converts a type descriptor to human-readable "dotted" form.  For
 * example, "Ljava/lang/String;" becomes "java.lang.String", and
 * "[I" becomes "int[]".  Also converts '$' to '.', which means this
 * form can't be converted back to a descriptor.
 */
static char* descriptorToDot(const char* str)
{
    int targetLen = strlen(str);
    int offset = 0;
    int arrayDepth = 0;
    char* newStr;

    /* strip leading [s; will be added to end */
    while (targetLen > 1 && str[offset] == '[') {
        offset++;
        targetLen--;
    }
    arrayDepth = offset;

    if (targetLen == 1) {
        /* primitive type */
        str = primitiveTypeLabel(str[offset]);
        offset = 0;
        targetLen = strlen(str);
    } else {
        /* account for leading 'L' and trailing ';' */
        if (targetLen >= 2 && str[offset] == 'L' &&
            str[offset+targetLen-1] == ';')
        {
            targetLen -= 2;
            offset++;
        }
    }

    newStr = (char*)malloc(targetLen + arrayDepth * 2 +1);

    /* copy class name over */
    int i;
    for (i = 0; i < targetLen; i++) {
        char ch = str[offset + i];
        newStr[i] = (ch == '/' || ch == '$') ? '.' : ch;
    }

    /* add the appropriate number of brackets for arrays */
    while (arrayDepth-- > 0) {
        newStr[i++] = '[';
        newStr[i++] = ']';
    }
    newStr[i] = '\0';
    assert(i == targetLen + arrayDepth * 2);

    return newStr;
}

/*
 * Converts the class name portion of a type descriptor to human-readable
 * "dotted" form.
 *
 * Returns a newly-allocated string.
 */
static char* descriptorClassToDot(const char* str)
{
    const char* lastSlash;
    char* newStr;
    char* cp;

    /* reduce to just the class name, trimming trailing ';' */
    lastSlash = strrchr(str, '/');
    if (lastSlash == NULL)
        lastSlash = str + 1;        /* start past 'L' */
    else
        lastSlash++;                /* start past '/' */

    newStr = strdup(lastSlash);
    newStr[strlen(lastSlash)-1] = '\0';
    for (cp = newStr; *cp != '\0'; cp++) {
        if (*cp == '$')
            *cp = '.';
    }

    return newStr;
}

/*
 * Returns a quoted string representing the boolean value.
 */
static const char* quotedBool(bool val)
{
    if (val)
        return "\"true\"";
    else
        return "\"false\"";
}

static const char* quotedVisibility(u4 accessFlags)
{
    if ((accessFlags & ACC_PUBLIC) != 0)
        return "\"public\"";
    else if ((accessFlags & ACC_PROTECTED) != 0)
        return "\"protected\"";
    else if ((accessFlags & ACC_PRIVATE) != 0)
        return "\"private\"";
    else
        return "\"package\"";
}

/*
 * Count the number of '1' bits in a word.
 */
static int countOnes(u4 val)
{
    int count = 0;

    val = val - ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    count = (((val + (val >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;

    return count;
}

/*
 * Flag for use with createAccessFlagStr().
 */
enum AccessFor {
    kAccessForClass = 0, kAccessForMethod = 1, kAccessForField = 2,
    kAccessForMAX
};

/*
 * Create a new string with human-readable access flags.
 *
 * In the base language the access_flags fields are type u2; in Dalvik
 * they're u4.
 */
static char* createAccessFlagStr(u4 flags, AccessFor forWhat)
{
#define NUM_FLAGS   18
    static const char* kAccessStrings[kAccessForMAX][NUM_FLAGS] = {
        {
            /* class, inner class */
            "PUBLIC",           /* 0x0001 */
            "PRIVATE",          /* 0x0002 */
            "PROTECTED",        /* 0x0004 */
            "STATIC",           /* 0x0008 */
            "FINAL",            /* 0x0010 */
            "?",                /* 0x0020 */
            "?",                /* 0x0040 */
            "?",                /* 0x0080 */
            "?",                /* 0x0100 */
            "INTERFACE",        /* 0x0200 */
            "ABSTRACT",         /* 0x0400 */
            "?",                /* 0x0800 */
            "SYNTHETIC",        /* 0x1000 */
            "ANNOTATION",       /* 0x2000 */
            "ENUM",             /* 0x4000 */
            "?",                /* 0x8000 */
            "VERIFIED",         /* 0x10000 */
            "OPTIMIZED",        /* 0x20000 */
        },
        {
            /* method */
            "PUBLIC",           /* 0x0001 */
            "PRIVATE",          /* 0x0002 */
            "PROTECTED",        /* 0x0004 */
            "STATIC",           /* 0x0008 */
            "FINAL",            /* 0x0010 */
            "SYNCHRONIZED",     /* 0x0020 */
            "BRIDGE",           /* 0x0040 */
            "VARARGS",          /* 0x0080 */
            "NATIVE",           /* 0x0100 */
            "?",                /* 0x0200 */
            "ABSTRACT",         /* 0x0400 */
            "STRICT",           /* 0x0800 */
            "SYNTHETIC",        /* 0x1000 */
            "?",                /* 0x2000 */
            "?",                /* 0x4000 */
            "MIRANDA",          /* 0x8000 */
            "CONSTRUCTOR",      /* 0x10000 */
            "DECLARED_SYNCHRONIZED", /* 0x20000 */
        },
        {
            /* field */
            "PUBLIC",           /* 0x0001 */
            "PRIVATE",          /* 0x0002 */
            "PROTECTED",        /* 0x0004 */
            "STATIC",           /* 0x0008 */
            "FINAL",            /* 0x0010 */
            "?",                /* 0x0020 */
            "VOLATILE",         /* 0x0040 */
            "TRANSIENT",        /* 0x0080 */
            "?",                /* 0x0100 */
            "?",                /* 0x0200 */
            "?",                /* 0x0400 */
            "?",                /* 0x0800 */
            "SYNTHETIC",        /* 0x1000 */
            "?",                /* 0x2000 */
            "ENUM",             /* 0x4000 */
            "?",                /* 0x8000 */
            "?",                /* 0x10000 */
            "?",                /* 0x20000 */
        },
    };
    const int kLongest = 21;        /* strlen of longest string above */
    int i, count;
    char* str;
    char* cp;

    /*
     * Allocate enough storage to hold the expected number of strings,
     * plus a space between each.  We over-allocate, using the longest
     * string above as the base metric.
     */
    count = countOnes(flags);
    cp = str = (char*) malloc(count * (kLongest+1) +1);

    for (i = 0; i < NUM_FLAGS; i++) {
        if (flags & 0x01) {
            const char* accessStr = kAccessStrings[forWhat][i];
            int len = strlen(accessStr);
            if (cp != str)
                *cp++ = ' ';

            memcpy(cp, accessStr, len);
            cp += len;
        }
        flags >>= 1;
    }
    *cp = '\0';

    return str;
}


/*
 * Copy character data from "data" to "out", converting non-ASCII values
 * to printf format chars or an ASCII filler ('.' or '?').
 *
 * The output buffer must be able to hold (2*len)+1 bytes.  The result is
 * NUL-terminated.
 */
static void asciify(char* out, const unsigned char* data, size_t len)
{
    while (len--) {
        if (*data < 0x20) {
            /* could do more here, but we don't need them yet */
            switch (*data) {
            case '\0':
                *out++ = '\\';
                *out++ = '0';
                break;
            case '\n':
                *out++ = '\\';
                *out++ = 'n';
                break;
            default:
                *out++ = '.';
                break;
            }
        } else if (*data >= 0x80) {
            *out++ = '?';
        } else {
            *out++ = *data;
        }
        data++;
    }
    *out = '\0';
}

/*
 * Dump the file header.
 */
void dumpFileHeader(const DexFile* pDexFile)
{
    const DexOptHeader* pOptHeader = pDexFile->pOptHeader;
    const DexHeader* pHeader = pDexFile->pHeader;
    char sanitized[sizeof(pHeader->magic)*2 +1];

    assert(sizeof(pHeader->magic) == sizeof(pOptHeader->magic));

    if (pOptHeader != NULL) {
        fprintf(dexFile, "Optimized DEX file header:\n");

        asciify(sanitized, pOptHeader->magic, sizeof(pOptHeader->magic));
        fprintf(dexFile, "magic               : '%s'\n", sanitized);
        fprintf(dexFile, "dex_offset          : %d (0x%06x)\n",
            pOptHeader->dexOffset, pOptHeader->dexOffset);
        fprintf(dexFile, "dex_length          : %d\n", pOptHeader->dexLength);
        fprintf(dexFile, "deps_offset         : %d (0x%06x)\n",
            pOptHeader->depsOffset, pOptHeader->depsOffset);
        fprintf(dexFile, "deps_length         : %d\n", pOptHeader->depsLength);
        fprintf(dexFile, "opt_offset          : %d (0x%06x)\n",
            pOptHeader->optOffset, pOptHeader->optOffset);
        fprintf(dexFile, "opt_length          : %d\n", pOptHeader->optLength);
        fprintf(dexFile, "flags               : %08x\n", pOptHeader->flags);
        fprintf(dexFile, "checksum            : %08x\n", pOptHeader->checksum);
        fprintf(dexFile, "\n");
    }

    fprintf(dexFile, "DEX file header:\n");
    asciify(sanitized, pHeader->magic, sizeof(pHeader->magic));
    fprintf(dexFile, "magic               : '%s'\n", sanitized);
    fprintf(dexFile, "checksum            : %08x\n", pHeader->checksum);
    fprintf(dexFile, "signature           : %02x%02x...%02x%02x\n",
        pHeader->signature[0], pHeader->signature[1],
        pHeader->signature[kSHA1DigestLen-2],
        pHeader->signature[kSHA1DigestLen-1]);
    fprintf(dexFile, "file_size           : %d\n", pHeader->fileSize);
    fprintf(dexFile, "header_size         : %d\n", pHeader->headerSize);
    fprintf(dexFile, "link_size           : %d\n", pHeader->linkSize);
    fprintf(dexFile, "link_off            : %d (0x%06x)\n",
        pHeader->linkOff, pHeader->linkOff);
    fprintf(dexFile, "string_ids_size     : %d\n", pHeader->stringIdsSize);
    fprintf(dexFile, "string_ids_off      : %d (0x%06x)\n",
        pHeader->stringIdsOff, pHeader->stringIdsOff);
    fprintf(dexFile, "type_ids_size       : %d\n", pHeader->typeIdsSize);
    fprintf(dexFile, "type_ids_off        : %d (0x%06x)\n",
        pHeader->typeIdsOff, pHeader->typeIdsOff);
    fprintf(dexFile, "field_ids_size      : %d\n", pHeader->fieldIdsSize);
    fprintf(dexFile, "field_ids_off       : %d (0x%06x)\n",
        pHeader->fieldIdsOff, pHeader->fieldIdsOff);
    fprintf(dexFile, "method_ids_size     : %d\n", pHeader->methodIdsSize);
    fprintf(dexFile, "method_ids_off      : %d (0x%06x)\n",
        pHeader->methodIdsOff, pHeader->methodIdsOff);
    fprintf(dexFile, "class_defs_size     : %d\n", pHeader->classDefsSize);
    fprintf(dexFile, "class_defs_off      : %d (0x%06x)\n",
        pHeader->classDefsOff, pHeader->classDefsOff);
    fprintf(dexFile, "data_size           : %d\n", pHeader->dataSize);
    fprintf(dexFile, "data_off            : %d (0x%06x)\n",
        pHeader->dataOff, pHeader->dataOff);
    fprintf(dexFile, "\n");
}

/*
 * Dump the "table of contents" for the opt area.
 */
void dumpOptDirectory(const DexFile* pDexFile)
{
    const DexOptHeader* pOptHeader = pDexFile->pOptHeader;
    if (pOptHeader == NULL)
        return;

    fprintf(dexFile, "OPT section contents:\n");

    const u4* pOpt = (const u4*) ((u1*) pOptHeader + pOptHeader->optOffset);

    if (*pOpt == 0) {
        fprintf(dexFile, "(1.0 format, only class lookup table is present)\n\n");
        return;
    }

    /*
     * The "opt" section is in "chunk" format: a 32-bit identifier, a 32-bit
     * length, then the data.  Chunks start on 64-bit boundaries.
     */
    while (*pOpt != kDexChunkEnd) {
        const char* verboseStr;

        u4 size = *(pOpt+1);

        switch (*pOpt) {
        case kDexChunkClassLookup:
            verboseStr = "class lookup hash table";
            break;
        case kDexChunkRegisterMaps:
            verboseStr = "register maps";
            break;
        default:
            verboseStr = "(unknown chunk type)";
            break;
        }

        fprintf(dexFile, "Chunk %08x (%c%c%c%c) - %s (%d bytes)\n", *pOpt,
            *pOpt >> 24, (char)(*pOpt >> 16), (char)(*pOpt >> 8), (char)*pOpt,
            verboseStr, size);

        size = (size + 8 + 7) & ~7;
        pOpt += size / sizeof(u4);
    }
    fprintf(dexFile, "\n");
}

/*
 * Dump a class_def_item.
 */
void dumpClassDef(DexFile* pDexFile, int idx)
{
    const DexClassDef* pClassDef;
    const u1* pEncodedData;
    DexClassData* pClassData;

    pClassDef = dexGetClassDef(pDexFile, idx);
    pEncodedData = dexGetClassData(pDexFile, pClassDef);
    pClassData = dexReadAndVerifyClassData(&pEncodedData, NULL);

    if (pClassData == NULL) {
        fprintf(stderr, "Trouble reading class data\n");
        return;
    }

    fprintf(dexFile, "Class #%d header:\n", idx);
    fprintf(dexFile, "class_idx           : %d\n", pClassDef->classIdx);
    fprintf(dexFile, "access_flags        : %d (0x%04x)\n",
        pClassDef->accessFlags, pClassDef->accessFlags);
    fprintf(dexFile, "superclass_idx      : %d\n", pClassDef->superclassIdx);
    fprintf(dexFile, "interfaces_off      : %d (0x%06x)\n",
        pClassDef->interfacesOff, pClassDef->interfacesOff);
    fprintf(dexFile, "source_file_idx     : %d\n", pClassDef->sourceFileIdx);
    fprintf(dexFile, "annotations_off     : %d (0x%06x)\n",
        pClassDef->annotationsOff, pClassDef->annotationsOff);
    fprintf(dexFile, "class_data_off      : %d (0x%06x)\n",
        pClassDef->classDataOff, pClassDef->classDataOff);
    fprintf(dexFile, "static_fields_size  : %d\n", pClassData->header.staticFieldsSize);
    fprintf(dexFile, "instance_fields_size: %d\n",
            pClassData->header.instanceFieldsSize);
    fprintf(dexFile, "direct_methods_size : %d\n", pClassData->header.directMethodsSize);
    fprintf(dexFile, "virtual_methods_size: %d\n",
            pClassData->header.virtualMethodsSize);
    fprintf(dexFile, "\n");

    free(pClassData);
}

/*
 * Dump an interface that a class declares to implement.
 */
void dumpInterface(const DexFile* pDexFile, const DexTypeItem* pTypeItem,
    int i)
{
    const char* interfaceName =
        dexStringByTypeIdx(pDexFile, pTypeItem->typeIdx);

    if (gOptions.outputFormat == OUTPUT_PLAIN) {
        fprintf(dexFile, "    #%d              : '%s'\n", i, interfaceName);
    } else {
        char* dotted = descriptorToDot(interfaceName);
        fprintf(dexFile, "<implements name=\"%s\">\n</implements>\n", dotted);
        free(dotted);
    }
}

/*
 * Dump the catches table associated with the code.
 */
void dumpCatches(DexFile* pDexFile, const DexCode* pCode)
{
    u4 triesSize = pCode->triesSize;

    if (triesSize == 0) {
        fprintf(dexFile, "      catches       : (none)\n");
        return;
    }

    fprintf(dexFile, "      catches       : %d\n", triesSize);

    const DexTry* pTries = dexGetTries(pCode);
    u4 i;

    for (i = 0; i < triesSize; i++) {
        const DexTry* pTry = &pTries[i];
        u4 start = pTry->startAddr;
        u4 end = start + pTry->insnCount;
        DexCatchIterator iterator;

        fprintf(dexFile, "        0x%04x - 0x%04x\n", start, end);

        dexCatchIteratorInit(&iterator, pCode, pTry->handlerOff);

        for (;;) {
            DexCatchHandler* handler = dexCatchIteratorNext(&iterator);
            const char* descriptor;

            if (handler == NULL) {
                break;
            }

            descriptor = (handler->typeIdx == kDexNoIndex) ? "<any>" :
                dexStringByTypeIdx(pDexFile, handler->typeIdx);

            fprintf(dexFile, "          %s -> 0x%04x\n", descriptor,
                    handler->address);
        }
    }
}

static int dumpPositionsCb(void *cnxt, u4 address, u4 lineNum)
{
    fprintf(dexFile, "        0x%04x line=%d\n", address, lineNum);
    return 0;
}

/*
 * Dump the positions list.
 */
void dumpPositions(DexFile* pDexFile, const DexCode* pCode,
        const DexMethod *pDexMethod)
{
    fprintf(dexFile, "      positions     : \n");
    const DexMethodId *pMethodId
            = dexGetMethodId(pDexFile, pDexMethod->methodIdx);
    const char *classDescriptor
            = dexStringByTypeIdx(pDexFile, pMethodId->classIdx);

    dexDecodeDebugInfo(pDexFile, pCode, classDescriptor, pMethodId->protoIdx,
            pDexMethod->accessFlags, dumpPositionsCb, NULL, NULL);
}

static void dumpLocalsCb(void *cnxt, u2 reg, u4 startAddress,
        u4 endAddress, const char *name, const char *descriptor,
        const char *signature)
{
    fprintf(dexFile, "        0x%04x - 0x%04x reg=%d %s %s %s\n",
            startAddress, endAddress, reg, name, descriptor,
            signature);
}

/*
 * Dump the locals list.
 */
void dumpLocals(DexFile* pDexFile, const DexCode* pCode,
        const DexMethod *pDexMethod)
{
    fprintf(dexFile, "      locals        : \n");

    const DexMethodId *pMethodId
            = dexGetMethodId(pDexFile, pDexMethod->methodIdx);
    const char *classDescriptor
            = dexStringByTypeIdx(pDexFile, pMethodId->classIdx);

    dexDecodeDebugInfo(pDexFile, pCode, classDescriptor, pMethodId->protoIdx,
            pDexMethod->accessFlags, NULL, dumpLocalsCb, NULL);
}

/*
 * Get information about a method.
 */
bool getMethodInfo(DexFile* pDexFile, u4 methodIdx, FieldMethodInfo* pMethInfo)
{
    const DexMethodId* pMethodId;

    if (methodIdx >= pDexFile->pHeader->methodIdsSize)
        return false;

    pMethodId = dexGetMethodId(pDexFile, methodIdx);
    pMethInfo->name = dexStringById(pDexFile, pMethodId->nameIdx);
    pMethInfo->signature = dexCopyDescriptorFromMethodId(pDexFile, pMethodId);

    pMethInfo->classDescriptor =
            dexStringByTypeIdx(pDexFile, pMethodId->classIdx);
    return true;
}

/*
 * Get information about a field.
 */
bool getFieldInfo(DexFile* pDexFile, u4 fieldIdx, FieldMethodInfo* pFieldInfo)
{
    const DexFieldId* pFieldId;

    if (fieldIdx >= pDexFile->pHeader->fieldIdsSize)
        return false;

    pFieldId = dexGetFieldId(pDexFile, fieldIdx);
    pFieldInfo->name = dexStringById(pDexFile, pFieldId->nameIdx);
    pFieldInfo->signature = dexStringByTypeIdx(pDexFile, pFieldId->typeIdx);
    pFieldInfo->classDescriptor =
        dexStringByTypeIdx(pDexFile, pFieldId->classIdx);
    return true;
}


/*
 * Look up a class' descriptor.
 */
const char* getClassDescriptor(DexFile* pDexFile, u4 classIdx)
{
    return dexStringByTypeIdx(pDexFile, classIdx);
}

/*
 * Helper for dumpInstruction(), which builds the string
 * representation for the index in the given instruction. This will
 * first try to use the given buffer, but if the result won't fit,
 * then this will allocate a new buffer to hold the result. A pointer
 * to the buffer which holds the full result is always returned, and
 * this can be compared with the one passed in, to see if the result
 * needs to be free()d.
 */
static char* indexString(DexFile* pDexFile,
    const DecodedInstruction* pDecInsn, char* buf, size_t bufSize)
{
    int outSize;
    u4 index;
    u4 width;

    /* TODO: Make the index *always* be in field B, to simplify this code. */
    switch (dexGetFormatFromOpcode(pDecInsn->opcode)) {
    case kFmt20bc:
    case kFmt21c:
    case kFmt35c:
    case kFmt35ms:
    case kFmt3rc:
    case kFmt3rms:
    case kFmt35mi:
    case kFmt3rmi:
        index = pDecInsn->vB;
        width = 4;
        break;
    case kFmt31c:
        index = pDecInsn->vB;
        width = 8;
        break;
    case kFmt22c:
    case kFmt22cs:
        index = pDecInsn->vC;
        width = 4;
        break;
    default:
        index = 0;
        width = 4;
        break;
    }

    switch (pDecInsn->indexType) {
    case kIndexUnknown:
        /*
         * This function shouldn't ever get called for this type, but do
         * something sensible here, just to help with debugging.
         */
        outSize = snprintf(buf, bufSize, "<unknown-index>");
        break;
    case kIndexNone:
        /*
         * This function shouldn't ever get called for this type, but do
         * something sensible here, just to help with debugging.
         */
        outSize = snprintf(buf, bufSize, "<no-index>");
        break;
    case kIndexVaries:
        /*
         * This one should never show up in a dexdump, so no need to try
         * to get fancy here.
         */
        outSize = snprintf(buf, bufSize, "<index-varies> // thing@%0*x",
                width, index);
        break;
    case kIndexTypeRef:
        if (index < pDexFile->pHeader->typeIdsSize) {
            outSize = snprintf(buf, bufSize, "%s // type@%0*x",
                               getClassDescriptor(pDexFile, index), width, index);
        } else {
            outSize = snprintf(buf, bufSize, "<type?> // type@%0*x", width, index);
        }
        break;
    case kIndexStringRef:
        if (index < pDexFile->pHeader->stringIdsSize) {
            outSize = snprintf(buf, bufSize, "\"%s\" // string@%0*x",
                               dexStringById(pDexFile, index), width, index);
        } else {
            outSize = snprintf(buf, bufSize, "<string?> // string@%0*x",
                               width, index);
        }
        break;
    case kIndexMethodRef:
        {
            FieldMethodInfo methInfo;
            if (getMethodInfo(pDexFile, index, &methInfo)) {
                outSize = snprintf(buf, bufSize, "%s.%s:%s // method@%0*x",
                        methInfo.classDescriptor, methInfo.name,
                        methInfo.signature, width, index);
            } else {
                outSize = snprintf(buf, bufSize, "<method?> // method@%0*x",
                        width, index);
            }
        }
        break;
    case kIndexFieldRef:
        {
            FieldMethodInfo fieldInfo;
            if (getFieldInfo(pDexFile, index, &fieldInfo)) {
                outSize = snprintf(buf, bufSize, "%s.%s:%s // field@%0*x",
                        fieldInfo.classDescriptor, fieldInfo.name,
                        fieldInfo.signature, width, index);
            } else {
                outSize = snprintf(buf, bufSize, "<field?> // field@%0*x",
                        width, index);
            }
        }
        break;
    case kIndexInlineMethod:
        outSize = snprintf(buf, bufSize, "[%0*x] // inline #%0*x",
                width, index, width, index);
        break;
    case kIndexVtableOffset:
        outSize = snprintf(buf, bufSize, "[%0*x] // vtable #%0*x",
                width, index, width, index);
        break;
    case kIndexFieldOffset:
        outSize = snprintf(buf, bufSize, "[obj+%0*x]", width, index);
        break;
    default:
        outSize = snprintf(buf, bufSize, "<?>");
        break;
    }

    if (outSize >= (int) bufSize) {
        /*
         * The buffer wasn't big enough; allocate and retry. Note:
         * snprintf( ) doesn't count the '\0' as part of its returned
         * size, so we add explicit space for it here.
         */
        outSize++;
        buf = (char*)malloc(outSize);
        if (buf == NULL) {
            return NULL;
        }
        return indexString(pDexFile, pDecInsn, buf, outSize);
    } else {
        return buf;
    }
}

/*
 * Dump a single instruction.
 */
void dumpInstruction(DexFile* pDexFile, const DexCode* pCode, int insnIdx,
    int insnWidth, const DecodedInstruction* pDecInsn)
{
    char indexBufChars[200];
    char *indexBuf = indexBufChars;
    const u2* insns = pCode->insns;
    int i;

    fprintf(dexFile, "%06x:", ((u1*)insns - pDexFile->baseAddr) + insnIdx*2);
    for (i = 0; i < 8; i++) {
        if (i < insnWidth) {
            if (i == 7) {
                fprintf(dexFile, " ... ");
            } else {
                /* print 16-bit value in little-endian order */
                const u1* bytePtr = (const u1*) &insns[insnIdx+i];
                fprintf(dexFile, " %02x%02x", bytePtr[0], bytePtr[1]);
            }
        } else {
            fprintf(dexFile, "     ");
        }
    }

    if (pDecInsn->opcode == OP_NOP) {
        u2 instr = get2LE((const u1*) &insns[insnIdx]);
        if (instr == kPackedSwitchSignature) {
            fprintf(dexFile, "|%04x: packed-switch-data (%d units)",
                insnIdx, insnWidth);
        } else if (instr == kSparseSwitchSignature) {
            fprintf(dexFile, "|%04x: sparse-switch-data (%d units)",
                insnIdx, insnWidth);
        } else if (instr == kArrayDataSignature) {
            fprintf(dexFile, "|%04x: array-data (%d units)",
                insnIdx, insnWidth);
        } else {
            fprintf(dexFile, "|%04x: nop // spacer", insnIdx);
        }
    } else {
        fprintf(dexFile, "|%04x: %s", insnIdx, dexGetOpcodeName(pDecInsn->opcode));
    }

    if (pDecInsn->indexType != kIndexNone) {
        indexBuf = indexString(pDexFile, pDecInsn,
                indexBufChars, sizeof(indexBufChars));
    }

    switch (dexGetFormatFromOpcode(pDecInsn->opcode)) {
    case kFmt10x:        // op
        break;
    case kFmt12x:        // op vA, vB
        fprintf(dexFile, " v%d, v%d", pDecInsn->vA, pDecInsn->vB);
        break;
    case kFmt11n:        // op vA, #+B
        fprintf(dexFile, " v%d, #int %d // #%x",
            pDecInsn->vA, (s4)pDecInsn->vB, (u1)pDecInsn->vB);
        break;
    case kFmt11x:        // op vAA
        fprintf(dexFile, " v%d", pDecInsn->vA);
        break;
    case kFmt10t:        // op +AA
    case kFmt20t:        // op +AAAA
        {
            s4 targ = (s4) pDecInsn->vA;
            fprintf(dexFile, " %04x // %c%04x",
                insnIdx + targ,
                (targ < 0) ? '-' : '+',
                (targ < 0) ? -targ : targ);
        }
        break;
    case kFmt22x:        // op vAA, vBBBB
        fprintf(dexFile, " v%d, v%d", pDecInsn->vA, pDecInsn->vB);
        break;
    case kFmt21t:        // op vAA, +BBBB
        {
            s4 targ = (s4) pDecInsn->vB;
            fprintf(dexFile, " v%d, %04x // %c%04x", pDecInsn->vA,
                insnIdx + targ,
                (targ < 0) ? '-' : '+',
                (targ < 0) ? -targ : targ);
        }
        break;
    case kFmt21s:        // op vAA, #+BBBB
        fprintf(dexFile, " v%d, #int %d // #%x",
            pDecInsn->vA, (s4)pDecInsn->vB, (u2)pDecInsn->vB);
        break;
    case kFmt21h:        // op vAA, #+BBBB0000[00000000]
        // The printed format varies a bit based on the actual opcode.
        if (pDecInsn->opcode == OP_CONST_HIGH16) {
            s4 value = pDecInsn->vB << 16;
            fprintf(dexFile, " v%d, #int %d // #%x",
                pDecInsn->vA, value, (u2)pDecInsn->vB);
        } else {
            s8 value = ((s8) pDecInsn->vB) << 48;
            fprintf(dexFile, " v%d, #long %lld // #%x",
                pDecInsn->vA, value, (u2)pDecInsn->vB);
        }
        break;
    case kFmt21c:        // op vAA, thing@BBBB
    case kFmt31c:        // op vAA, thing@BBBBBBBB
        fprintf(dexFile, " v%d, %s", pDecInsn->vA, indexBuf);
        break;
    case kFmt23x:        // op vAA, vBB, vCC
        fprintf(dexFile, " v%d, v%d, v%d", pDecInsn->vA, pDecInsn->vB, pDecInsn->vC);
        break;
    case kFmt22b:        // op vAA, vBB, #+CC
        fprintf(dexFile, " v%d, v%d, #int %d // #%02x",
            pDecInsn->vA, pDecInsn->vB, (s4)pDecInsn->vC, (u1)pDecInsn->vC);
        break;
    case kFmt22t:        // op vA, vB, +CCCC
        {
            s4 targ = (s4) pDecInsn->vC;
            fprintf(dexFile, " v%d, v%d, %04x // %c%04x", pDecInsn->vA, pDecInsn->vB,
                insnIdx + targ,
                (targ < 0) ? '-' : '+',
                (targ < 0) ? -targ : targ);
        }
        break;
    case kFmt22s:        // op vA, vB, #+CCCC
        fprintf(dexFile, " v%d, v%d, #int %d // #%04x",
            pDecInsn->vA, pDecInsn->vB, (s4)pDecInsn->vC, (u2)pDecInsn->vC);
        break;
    case kFmt22c:        // op vA, vB, thing@CCCC
    case kFmt22cs:       // [opt] op vA, vB, field offset CCCC
        fprintf(dexFile, " v%d, v%d, %s", pDecInsn->vA, pDecInsn->vB, indexBuf);
        break;
    case kFmt30t:
        fprintf(dexFile, " #%08x", pDecInsn->vA);
        break;
    case kFmt31i:        // op vAA, #+BBBBBBBB
        {
            /* this is often, but not always, a float */
            union {
                float f;
                u4 i;
            } conv;
            conv.i = pDecInsn->vB;
            fprintf(dexFile, " v%d, #float %f // #%08x",
                pDecInsn->vA, conv.f, pDecInsn->vB);
        }
        break;
    case kFmt31t:       // op vAA, offset +BBBBBBBB
        fprintf(dexFile, " v%d, %08x // +%08x",
            pDecInsn->vA, insnIdx + pDecInsn->vB, pDecInsn->vB);
        break;
    case kFmt32x:        // op vAAAA, vBBBB
        fprintf(dexFile, " v%d, v%d", pDecInsn->vA, pDecInsn->vB);
        break;
    case kFmt35c:        // op {vC, vD, vE, vF, vG}, thing@BBBB
    case kFmt35ms:       // [opt] invoke-virtual+super
    case kFmt35mi:       // [opt] inline invoke
        {
            fprintf(dexFile, " {");
            for (i = 0; i < (int) pDecInsn->vA; i++) {
                if (i == 0)
                    fprintf(dexFile, "v%d", pDecInsn->arg[i]);
                else
                    fprintf(dexFile, ", v%d", pDecInsn->arg[i]);
            }
            fprintf(dexFile, "}, %s", indexBuf);
        }
        break;
    case kFmt3rc:        // op {vCCCC .. v(CCCC+AA-1)}, thing@BBBB
    case kFmt3rms:       // [opt] invoke-virtual+super/range
    case kFmt3rmi:       // [opt] execute-inline/range
        {
            /*
             * This doesn't match the "dx" output when some of the args are
             * 64-bit values -- dx only shows the first register.
             */
            fprintf(dexFile, " {");
            for (i = 0; i < (int) pDecInsn->vA; i++) {
                if (i == 0)
                    fprintf(dexFile, "v%d", pDecInsn->vC + i);
                else
                    fprintf(dexFile, ", v%d", pDecInsn->vC + i);
            }
            fprintf(dexFile, "}, %s", indexBuf);
        }
        break;
    case kFmt51l:        // op vAA, #+BBBBBBBBBBBBBBBB
        {
            /* this is often, but not always, a double */
            union {
                double d;
                u8 j;
            } conv;
            conv.j = pDecInsn->vB_wide;
            fprintf(dexFile, " v%d, #double %f // #%016llx",
                pDecInsn->vA, conv.d, pDecInsn->vB_wide);
        }
        break;
    case kFmt00x:        // unknown op or breakpoint
        break;
    default:
        fprintf(dexFile, " ???");
        break;
    }

    fprintf(dexFile, "\n");

    if (indexBuf != indexBufChars) {
        free(indexBuf);
    }
}

/*
 * Dump a bytecode disassembly.
 */
void dumpBytecodes(DexFile* pDexFile, const DexMethod* pDexMethod)
{
    const DexCode* pCode = dexGetCode(pDexFile, pDexMethod);
    const u2* insns;
    int insnIdx;
    FieldMethodInfo methInfo;
    int startAddr;
    char* className = NULL;

    assert(pCode->insnsSize > 0);
    insns = pCode->insns;

    getMethodInfo(pDexFile, pDexMethod->methodIdx, &methInfo);
    startAddr = ((u1*)pCode - pDexFile->baseAddr);
    className = descriptorToDot(methInfo.classDescriptor);

    fprintf(dexFile, "%06x:                                        |[%06x] %s.%s:%s\n",
        startAddr, startAddr,
        className, methInfo.name, methInfo.signature);

    insnIdx = 0;
    while (insnIdx < (int) pCode->insnsSize) {
        int insnWidth;
        DecodedInstruction decInsn;
        u2 instr;

        /*
         * Note: This code parallels the function
         * dexGetWidthFromInstruction() in InstrUtils.c, but this version
         * can deal with data in either endianness.
         *
         * TODO: Figure out if this really matters, and possibly change
         * this to just use dexGetWidthFromInstruction().
         */
        instr = get2LE((const u1*)insns);
        if (instr == kPackedSwitchSignature) {
            insnWidth = 4 + get2LE((const u1*)(insns+1)) * 2;
        } else if (instr == kSparseSwitchSignature) {
            insnWidth = 2 + get2LE((const u1*)(insns+1)) * 4;
        } else if (instr == kArrayDataSignature) {
            int width = get2LE((const u1*)(insns+1));
            int size = get2LE((const u1*)(insns+2)) |
                       (get2LE((const u1*)(insns+3))<<16);
            // The plus 1 is to round up for odd size and width.
            insnWidth = 4 + ((size * width) + 1) / 2;
        } else {
            Opcode opcode = dexOpcodeFromCodeUnit(instr);
            insnWidth = dexGetWidthFromOpcode(opcode);
            if (insnWidth == 0) {
                fprintf(stderr,
                    "GLITCH: zero-width instruction at idx=0x%04x\n", insnIdx);
                break;
            }
        }

        dexDecodeInstruction(insns, &decInsn);
        dumpInstruction(pDexFile, pCode, insnIdx, insnWidth, &decInsn);

        insns += insnWidth;
        insnIdx += insnWidth;
    }

    free(className);
}

/*
 * Dump a "code" struct.
 */
void dumpCode(DexFile* pDexFile, const DexMethod* pDexMethod)
{
    const DexCode* pCode = dexGetCode(pDexFile, pDexMethod);

    fprintf(dexFile, "      registers     : %d\n", pCode->registersSize);
    fprintf(dexFile, "      ins           : %d\n", pCode->insSize);
    fprintf(dexFile, "      outs          : %d\n", pCode->outsSize);
    fprintf(dexFile, "      insns size    : %d 16-bit code units\n", pCode->insnsSize);

    if (gOptions.disassemble)
        dumpBytecodes(pDexFile, pDexMethod);

    dumpCatches(pDexFile, pCode);
    /* both of these are encoded in debug info */
    dumpPositions(pDexFile, pCode, pDexMethod);
    dumpLocals(pDexFile, pCode, pDexMethod);
}

/*
 * Dump a method.
 */
void dumpMethod(DexFile* pDexFile, const DexMethod* pDexMethod, int i)
{
    const DexMethodId* pMethodId;
    const char* backDescriptor;
    const char* name;
    char* typeDescriptor = NULL;
    char* accessStr = NULL;

    if (gOptions.exportsOnly &&
        (pDexMethod->accessFlags & (ACC_PUBLIC | ACC_PROTECTED)) == 0)
    {
        return;
    }

    pMethodId = dexGetMethodId(pDexFile, pDexMethod->methodIdx);
    name = dexStringById(pDexFile, pMethodId->nameIdx);
    typeDescriptor = dexCopyDescriptorFromMethodId(pDexFile, pMethodId);

    backDescriptor = dexStringByTypeIdx(pDexFile, pMethodId->classIdx);

    accessStr = createAccessFlagStr(pDexMethod->accessFlags,
                    kAccessForMethod);

    if (gOptions.outputFormat == OUTPUT_PLAIN) {
        fprintf(dexFile, "    #%d              : (in %s)\n", i, backDescriptor);
        fprintf(dexFile, "      name          : '%s'\n", name);
        fprintf(dexFile, "      type          : '%s'\n", typeDescriptor);
        fprintf(dexFile, "      access        : 0x%04x (%s)\n",
            pDexMethod->accessFlags, accessStr);

        if (pDexMethod->codeOff == 0) {
            fprintf(dexFile, "      code          : (none)\n");
        } else {
            fprintf(dexFile, "      code          -\n");
            dumpCode(pDexFile, pDexMethod);
        }

        if (gOptions.disassemble)
            fprintf(dexFile, "\n");
    } else if (gOptions.outputFormat == OUTPUT_XML) {
        bool constructor = (name[0] == '<');

        if (constructor) {
            char* tmp;

            tmp = descriptorClassToDot(backDescriptor);
            fprintf(dexFile, "<constructor name=\"%s\"\n", tmp);
            free(tmp);

            tmp = descriptorToDot(backDescriptor);
            fprintf(dexFile, " type=\"%s\"\n", tmp);
            free(tmp);
        } else {
            fprintf(dexFile, "<method name=\"%s\"\n", name);

            const char* returnType = strrchr(typeDescriptor, ')');
            if (returnType == NULL) {
                fprintf(stderr, "bad method type descriptor '%s'\n",
                    typeDescriptor);
                goto bail;
            }

            char* tmp = descriptorToDot(returnType+1);
            fprintf(dexFile, " return=\"%s\"\n", tmp);
            free(tmp);

            fprintf(dexFile, " abstract=%s\n",
                quotedBool((pDexMethod->accessFlags & ACC_ABSTRACT) != 0));
            fprintf(dexFile, " native=%s\n",
                quotedBool((pDexMethod->accessFlags & ACC_NATIVE) != 0));

            bool isSync =
                (pDexMethod->accessFlags & ACC_SYNCHRONIZED) != 0 ||
                (pDexMethod->accessFlags & ACC_DECLARED_SYNCHRONIZED) != 0;
            fprintf(dexFile, " synchronized=%s\n", quotedBool(isSync));
        }

        fprintf(dexFile, " static=%s\n",
            quotedBool((pDexMethod->accessFlags & ACC_STATIC) != 0));
        fprintf(dexFile, " final=%s\n",
            quotedBool((pDexMethod->accessFlags & ACC_FINAL) != 0));
        // "deprecated=" not knowable w/o parsing annotations
        fprintf(dexFile, " visibility=%s\n",
            quotedVisibility(pDexMethod->accessFlags));

        fprintf(dexFile, ">\n");

        /*
         * Parameters.
         */
        if (typeDescriptor[0] != '(') {
            fprintf(stderr, "ERROR: bad descriptor '%s'\n", typeDescriptor);
            goto bail;
        }

        char tmpBuf[strlen(typeDescriptor)+1];      /* more than big enough */
        int argNum = 0;

        const char* base = typeDescriptor+1;

        while (*base != ')') {
            char* cp = tmpBuf;

            while (*base == '[')
                *cp++ = *base++;

            if (*base == 'L') {
                /* copy through ';' */
                do {
                    *cp = *base++;
                } while (*cp++ != ';');
            } else {
                /* primitive char, copy it */
                if (strchr("ZBCSIFJD", *base) == NULL) {
                    fprintf(stderr, "ERROR: bad method signature '%s'\n", base);
                    goto bail;
                }
                *cp++ = *base++;
            }

            /* null terminate and display */
            *cp++ = '\0';

            char* tmp = descriptorToDot(tmpBuf);
            fprintf(dexFile, "<parameter name=\"arg%d\" type=\"%s\">\n</parameter>\n",
                argNum++, tmp);
            free(tmp);
        }

        if (constructor)
            fprintf(dexFile, "</constructor>\n");
        else
            fprintf(dexFile, "</method>\n");
    }

bail:
    free(typeDescriptor);
    free(accessStr);
}

/*
 * Dump a static (class) field.
 */
void dumpSField(const DexFile* pDexFile, const DexField* pSField, int i)
{
    const DexFieldId* pFieldId;
    const char* backDescriptor;
    const char* name;
    const char* typeDescriptor;
    char* accessStr;

    if (gOptions.exportsOnly &&
        (pSField->accessFlags & (ACC_PUBLIC | ACC_PROTECTED)) == 0)
    {
        return;
    }

    pFieldId = dexGetFieldId(pDexFile, pSField->fieldIdx);
    name = dexStringById(pDexFile, pFieldId->nameIdx);
    typeDescriptor = dexStringByTypeIdx(pDexFile, pFieldId->typeIdx);
    backDescriptor = dexStringByTypeIdx(pDexFile, pFieldId->classIdx);

    accessStr = createAccessFlagStr(pSField->accessFlags, kAccessForField);

    if (gOptions.outputFormat == OUTPUT_PLAIN) {
        fprintf(dexFile, "    #%d              : (in %s)\n", i, backDescriptor);
        fprintf(dexFile, "      name          : '%s'\n", name);
        fprintf(dexFile, "      type          : '%s'\n", typeDescriptor);
        fprintf(dexFile, "      access        : 0x%04x (%s)\n",
            pSField->accessFlags, accessStr);
    } else if (gOptions.outputFormat == OUTPUT_XML) {
        char* tmp;

        fprintf(dexFile, "<field name=\"%s\"\n", name);

        tmp = descriptorToDot(typeDescriptor);
        fprintf(dexFile, " type=\"%s\"\n", tmp);
        free(tmp);

        fprintf(dexFile, " transient=%s\n",
            quotedBool((pSField->accessFlags & ACC_TRANSIENT) != 0));
        fprintf(dexFile, " volatile=%s\n",
            quotedBool((pSField->accessFlags & ACC_VOLATILE) != 0));
        // "value=" not knowable w/o parsing annotations
        fprintf(dexFile, " static=%s\n",
            quotedBool((pSField->accessFlags & ACC_STATIC) != 0));
        fprintf(dexFile, " final=%s\n",
            quotedBool((pSField->accessFlags & ACC_FINAL) != 0));
        // "deprecated=" not knowable w/o parsing annotations
        fprintf(dexFile, " visibility=%s\n",
            quotedVisibility(pSField->accessFlags));
        fprintf(dexFile, ">\n</field>\n");
    }

    free(accessStr);
}

/*
 * Dump an instance field.
 */
void dumpIField(const DexFile* pDexFile, const DexField* pIField, int i)
{
    dumpSField(pDexFile, pIField, i);
}

/*
 * Dump the class.
 *
 * Note "idx" is a DexClassDef index, not a DexTypeId index.
 *
 * If "*pLastPackage" is NULL or does not match the current class' package,
 * the value will be replaced with a newly-allocated string.
 */
void dumpClass(DexFile* pDexFile, int idx, char** pLastPackage)
{
    const DexTypeList* pInterfaces;
    const DexClassDef* pClassDef;
    DexClassData* pClassData = NULL;
    const u1* pEncodedData;
    const char* fileName;
    const char* classDescriptor;
    const char* superclassDescriptor;
    char* accessStr = NULL;
    int i;

    pClassDef = dexGetClassDef(pDexFile, idx);

    if (gOptions.exportsOnly && (pClassDef->accessFlags & ACC_PUBLIC) == 0) {
        //fprintf(dexFile, "<!-- omitting non-public class %s -->\n",
        //    classDescriptor);
        goto bail;
    }

    pEncodedData = dexGetClassData(pDexFile, pClassDef);
    pClassData = dexReadAndVerifyClassData(&pEncodedData, NULL);

    if (pClassData == NULL) {
        fprintf(dexFile, "Trouble reading class data (#%d)\n", idx);
        goto bail;
    }

    classDescriptor = dexStringByTypeIdx(pDexFile, pClassDef->classIdx);

    /*
     * For the XML output, show the package name.  Ideally we'd gather
     * up the classes, sort them, and dump them alphabetically so the
     * package name wouldn't jump around, but that's not a great plan
     * for something that needs to run on the device.
     */
    if (!(classDescriptor[0] == 'L' &&
          classDescriptor[strlen(classDescriptor)-1] == ';'))
    {
        /* arrays and primitives should not be defined explicitly */
        fprintf(stderr, "Malformed class name '%s'\n", classDescriptor);
        /* keep going? */
    } else if (gOptions.outputFormat == OUTPUT_XML) {
        char* mangle;
        char* lastSlash;
        char* cp;

        mangle = strdup(classDescriptor + 1);
        mangle[strlen(mangle)-1] = '\0';

        /* reduce to just the package name */
        lastSlash = strrchr(mangle, '/');
        if (lastSlash != NULL) {
            *lastSlash = '\0';
        } else {
            *mangle = '\0';
        }

        for (cp = mangle; *cp != '\0'; cp++) {
            if (*cp == '/')
                *cp = '.';
        }

        if (*pLastPackage == NULL || strcmp(mangle, *pLastPackage) != 0) {
            /* start of a new package */
            if (*pLastPackage != NULL)
                fprintf(dexFile, "</package>\n");
            fprintf(dexFile, "<package name=\"%s\"\n>\n", mangle);
            free(*pLastPackage);
            *pLastPackage = mangle;
        } else {
            free(mangle);
        }
    }

    accessStr = createAccessFlagStr(pClassDef->accessFlags, kAccessForClass);

    if (pClassDef->superclassIdx == kDexNoIndex) {
        superclassDescriptor = NULL;
    } else {
        superclassDescriptor =
            dexStringByTypeIdx(pDexFile, pClassDef->superclassIdx);
    }

    if (gOptions.outputFormat == OUTPUT_PLAIN) {
        fprintf(dexFile, "Class #%d            -\n", idx);
        fprintf(dexFile, "  Class descriptor  : '%s'\n", classDescriptor);
        fprintf(dexFile, "  Access flags      : 0x%04x (%s)\n",
            pClassDef->accessFlags, accessStr);

        if (superclassDescriptor != NULL)
            fprintf(dexFile, "  Superclass        : '%s'\n", superclassDescriptor);

        fprintf(dexFile, "  Interfaces        -\n");
    } else {
        char* tmp;

        tmp = descriptorClassToDot(classDescriptor);
        fprintf(dexFile, "<class name=\"%s\"\n", tmp);
        free(tmp);

        if (superclassDescriptor != NULL) {
            tmp = descriptorToDot(superclassDescriptor);
            fprintf(dexFile, " extends=\"%s\"\n", tmp);
            free(tmp);
        }
        fprintf(dexFile, " abstract=%s\n",
            quotedBool((pClassDef->accessFlags & ACC_ABSTRACT) != 0));
        fprintf(dexFile, " static=%s\n",
            quotedBool((pClassDef->accessFlags & ACC_STATIC) != 0));
        fprintf(dexFile, " final=%s\n",
            quotedBool((pClassDef->accessFlags & ACC_FINAL) != 0));
        // "deprecated=" not knowable w/o parsing annotations
        fprintf(dexFile, " visibility=%s\n",
            quotedVisibility(pClassDef->accessFlags));
        fprintf(dexFile, ">\n");
    }
    pInterfaces = dexGetInterfacesList(pDexFile, pClassDef);
    if (pInterfaces != NULL) {
        for (i = 0; i < (int) pInterfaces->size; i++)
            dumpInterface(pDexFile, dexGetTypeItem(pInterfaces, i), i);
    }

    if (gOptions.outputFormat == OUTPUT_PLAIN)
        fprintf(dexFile, "  Static fields     -\n");
    for (i = 0; i < (int) pClassData->header.staticFieldsSize; i++) {
        dumpSField(pDexFile, &pClassData->staticFields[i], i);
    }

    if (gOptions.outputFormat == OUTPUT_PLAIN)
        fprintf(dexFile, "  Instance fields   -\n");
    for (i = 0; i < (int) pClassData->header.instanceFieldsSize; i++) {
        dumpIField(pDexFile, &pClassData->instanceFields[i], i);
    }

    if (gOptions.outputFormat == OUTPUT_PLAIN)
        fprintf(dexFile, "  Direct methods    -\n");
    for (i = 0; i < (int) pClassData->header.directMethodsSize; i++) {
        dumpMethod(pDexFile, &pClassData->directMethods[i], i);
    }

    if (gOptions.outputFormat == OUTPUT_PLAIN)
        fprintf(dexFile, "  Virtual methods   -\n");
    for (i = 0; i < (int) pClassData->header.virtualMethodsSize; i++) {
        dumpMethod(pDexFile, &pClassData->virtualMethods[i], i);
    }

    // TODO: Annotations.

    if (pClassDef->sourceFileIdx != kDexNoIndex)
        fileName = dexStringById(pDexFile, pClassDef->sourceFileIdx);
    else
        fileName = "unknown";

    if (gOptions.outputFormat == OUTPUT_PLAIN) {
        fprintf(dexFile, "  source_file_idx   : %d (%s)\n",
            pClassDef->sourceFileIdx, fileName);
        fprintf(dexFile, "\n");
    }

    if (gOptions.outputFormat == OUTPUT_XML) {
        fprintf(dexFile, "</class>\n");
    }

bail:
    free(pClassData);
    free(accessStr);
}


/*
 * Advance "ptr" to ensure 32-bit alignment.
 */
static inline const u1* align32(const u1* ptr)
{
    return (u1*) (((uintptr_t) ptr + 3) & ~0x03);
}


/*
 * Dump a map in the "differential" format.
 *
 * TODO: show a hex dump of the compressed data.  (We can show the
 * uncompressed data if we move the compression code to libdex; otherwise
 * it's too complex to merit a fast & fragile implementation here.)
 */
void dumpDifferentialCompressedMap(const u1** pData)
{
    const u1* data = *pData;
    const u1* dataStart = data -1;      // format byte already removed
    u1 regWidth;
    u2 numEntries;

    /* standard header */
    regWidth = *data++;
    numEntries = *data++;
    numEntries |= (*data++) << 8;

    /* compressed data begins with the compressed data length */
    int compressedLen = readUnsignedLeb128(&data);
    int addrWidth = 1;
    if ((*data & 0x80) != 0)
        addrWidth++;

    int origLen = 4 + (addrWidth + regWidth) * numEntries;
    int compLen = (data - dataStart) + compressedLen;

    fprintf(dexFile, "        (differential compression %d -> %d [%d -> %d])\n",
        origLen, compLen,
        (addrWidth + regWidth) * numEntries, compressedLen);

    /* skip past end of entry */
    data += compressedLen;

    *pData = data;
}

/*
 * Dump register map contents of the current method.
 *
 * "*pData" should point to the start of the register map data.  Advances
 * "*pData" to the start of the next map.
 */
void dumpMethodMap(DexFile* pDexFile, const DexMethod* pDexMethod, int idx,
    const u1** pData)
{
    const u1* data = *pData;
    const DexMethodId* pMethodId;
    const char* name;
    int offset = data - (u1*) pDexFile->pOptHeader;

    pMethodId = dexGetMethodId(pDexFile, pDexMethod->methodIdx);
    name = dexStringById(pDexFile, pMethodId->nameIdx);
    fprintf(dexFile, "      #%d: 0x%08x %s\n", idx, offset, name);

    u1 format;
    int addrWidth;

    format = *data++;
    if (format == 1) {              /* kRegMapFormatNone */
        /* no map */
        fprintf(dexFile, "        (no map)\n");
        addrWidth = 0;
    } else if (format == 2) {       /* kRegMapFormatCompact8 */
        addrWidth = 1;
    } else if (format == 3) {       /* kRegMapFormatCompact16 */
        addrWidth = 2;
    } else if (format == 4) {       /* kRegMapFormatDifferential */
        dumpDifferentialCompressedMap(&data);
        goto bail;
    } else {
        fprintf(dexFile, "        (unknown format %d!)\n", format);
        /* don't know how to skip data; failure will cascade to end of class */
        goto bail;
    }

    if (addrWidth > 0) {
        u1 regWidth;
        u2 numEntries;
        int idx, addr, byte;

        regWidth = *data++;
        numEntries = *data++;
        numEntries |= (*data++) << 8;

        for (idx = 0; idx < numEntries; idx++) {
            addr = *data++;
            if (addrWidth > 1)
                addr |= (*data++) << 8;

            fprintf(dexFile, "        %4x:", addr);
            for (byte = 0; byte < regWidth; byte++) {
                fprintf(dexFile, " %02x", *data++);
            }
            fprintf(dexFile, "\n");
        }
    }

bail:
    //if (addrWidth >= 0)
    //    *pData = align32(data);
    *pData = data;
}

/*
 * Dump the contents of the register map area.
 *
 * These are only present in optimized DEX files, and the structure is
 * not really exposed to other parts of the VM itself.  We're going to
 * dig through them here, but this is pretty fragile.  DO NOT rely on
 * this or derive other code from it.
 */
void dumpRegisterMaps(DexFile* pDexFile)
{
    const u1* pClassPool = (const u1*)pDexFile->pRegisterMapPool;
    const u4* classOffsets;
    const u1* ptr;
    u4 numClasses;
    int baseFileOffset = (u1*) pClassPool - (u1*) pDexFile->pOptHeader;
    int idx;

    if (pClassPool == NULL) {
        fprintf(dexFile, "No register maps found\n");
        return;
    }

    ptr = pClassPool;
    numClasses = get4LE(ptr);
    ptr += sizeof(u4);
    classOffsets = (const u4*) ptr;

    fprintf(dexFile, "RMAP begins at offset 0x%07x\n", baseFileOffset);
    fprintf(dexFile, "Maps for %d classes\n", numClasses);
    for (idx = 0; idx < (int) numClasses; idx++) {
        const DexClassDef* pClassDef;
        const char* classDescriptor;

        pClassDef = dexGetClassDef(pDexFile, idx);
        classDescriptor = dexStringByTypeIdx(pDexFile, pClassDef->classIdx);

        fprintf(dexFile, "%4d: +%d (0x%08x) %s\n", idx, classOffsets[idx],
            baseFileOffset + classOffsets[idx], classDescriptor);

        if (classOffsets[idx] == 0)
            continue;

        /*
         * What follows is a series of RegisterMap entries, one for every
         * direct method, then one for every virtual method.
         */
        DexClassData* pClassData;
        const u1* pEncodedData;
        const u1* data = (u1*) pClassPool + classOffsets[idx];
        u2 methodCount;
        int i;

        pEncodedData = dexGetClassData(pDexFile, pClassDef);
        pClassData = dexReadAndVerifyClassData(&pEncodedData, NULL);
        if (pClassData == NULL) {
            fprintf(stderr, "Trouble reading class data\n");
            continue;
        }

        methodCount = *data++;
        methodCount |= (*data++) << 8;
        data += 2;      /* two pad bytes follow methodCount */
        if (methodCount != pClassData->header.directMethodsSize
                            + pClassData->header.virtualMethodsSize)
        {
            fprintf(dexFile, "NOTE: method count discrepancy (%d != %d + %d)\n",
                methodCount, pClassData->header.directMethodsSize,
                pClassData->header.virtualMethodsSize);
            /* this is bad, but keep going anyway */
        }

        fprintf(dexFile, "    direct methods: %d\n",
            pClassData->header.directMethodsSize);
        for (i = 0; i < (int) pClassData->header.directMethodsSize; i++) {
            dumpMethodMap(pDexFile, &pClassData->directMethods[i], i, &data);
        }

        fprintf(dexFile, "    virtual methods: %d\n",
            pClassData->header.virtualMethodsSize);
        for (i = 0; i < (int) pClassData->header.virtualMethodsSize; i++) {
            dumpMethodMap(pDexFile, &pClassData->virtualMethods[i], i, &data);
        }

        free(pClassData);
    }
}

/*
 * Dump the requested sections of the file.
 */
void processDexFile(DexFile* pDexFile, FILE * file)
{
    char* package = NULL;
    int i;
    dexFile = file;

    memset(&gOptions, 0, sizeof(gOptions));
    gOptions.disassemble = true;
    if (gOptions.dumpRegisterMaps) {
        dumpRegisterMaps(pDexFile);
        return;
    }

    if (gOptions.showFileHeaders) {
        dumpFileHeader(pDexFile);
        dumpOptDirectory(pDexFile);
    }

    if (gOptions.outputFormat == OUTPUT_XML)
        fprintf(dexFile, "<api>\n");

    for (i = 0; i < (int) pDexFile->pHeader->classDefsSize; i++) {
        if (gOptions.showSectionHeaders)
            dumpClassDef(pDexFile, i);

        dumpClass(pDexFile, i, &package);
    }

    /* free the last one allocated */
    if (package != NULL) {
        fprintf(dexFile, "</package>\n");
        free(package);
    }

    if (gOptions.outputFormat == OUTPUT_XML)
        fprintf(dexFile, "</api>\n");
}
void dexdumpOut(const Method * const m , FILE * file)
{
    processDexFile(m->clazz->pDvmDex->pDexFile, file);
}
