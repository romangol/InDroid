#include "indroid/tools/dex_hunter.h"
#include "native/InternalNativePriv.h"
#include <asm/siginfo.h>
#include "libdex/DexClass.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define DIAOS_DBG 1
#if defined(DIAOS_DBG)
# define GOSSIP(...) ALOG( LOG_VERBOSE, "GCY", __VA_ARGS__)
# define GOSSIP1(...) ALOG( LOG_VERBOSE, "NB", __VA_ARGS__)
#else
# define GOSSIP(...) (void(0)) 
#endif

struct arg{
    DvmDex* pDvmDex;
    char const * dumppath; 
    FILE * dexFile;
}param;

void ReadClassDataHeader(const uint8_t** pData,
        DexClassDataHeader *pHeader) {
    pHeader->staticFieldsSize = readUnsignedLeb128(pData);
    pHeader->instanceFieldsSize = readUnsignedLeb128(pData);
    pHeader->directMethodsSize = readUnsignedLeb128(pData);
    pHeader->virtualMethodsSize = readUnsignedLeb128(pData);
}

void ReadClassDataField(const uint8_t** pData, DexField* pField) {
    pField->fieldIdx = readUnsignedLeb128(pData);
    pField->accessFlags = readUnsignedLeb128(pData);
}

void ReadClassDataMethod(const uint8_t** pData, DexMethod* pMethod) {
    pMethod->methodIdx = readUnsignedLeb128(pData);
    pMethod->accessFlags = readUnsignedLeb128(pData);
    pMethod->codeOff = readUnsignedLeb128(pData);
}

DexClassData* ReadClassData(const uint8_t** pData) {

    DexClassDataHeader header;

    if (*pData == NULL) {
        return NULL;
    }

    ReadClassDataHeader(pData,&header);

    size_t resultSize = sizeof(DexClassData) + (header.staticFieldsSize * sizeof(DexField)) + (header.instanceFieldsSize * sizeof(DexField)) + (header.directMethodsSize * sizeof(DexMethod)) + (header.virtualMethodsSize * sizeof(DexMethod));

    DexClassData* result = (DexClassData*) malloc(resultSize);

    if (result == NULL) {
        return NULL;
    }

    uint8_t* ptr = ((uint8_t*) result) + sizeof(DexClassData);

    result->header = header;

    if (header.staticFieldsSize != 0) {
        result->staticFields = (DexField*) ptr;
        ptr += header.staticFieldsSize * sizeof(DexField);
    } else {
        result->staticFields = NULL;
    }

    if (header.instanceFieldsSize != 0) {
        result->instanceFields = (DexField*) ptr;
        ptr += header.instanceFieldsSize * sizeof(DexField);
    } else {
        result->instanceFields = NULL;
    }

    if (header.directMethodsSize != 0) {
        result->directMethods = (DexMethod*) ptr;
        ptr += header.directMethodsSize * sizeof(DexMethod);
    } else {
        result->directMethods = NULL;
    }

    if (header.virtualMethodsSize != 0) {
        result->virtualMethods = (DexMethod*) ptr;
    } else {
        result->virtualMethods = NULL;
    }

    for (uint32_t i = 0; i < header.staticFieldsSize; i++) {
        ReadClassDataField(pData, &result->staticFields[i]);
    }

    for (uint32_t i = 0; i < header.instanceFieldsSize; i++) {
        ReadClassDataField(pData, &result->instanceFields[i]);
    }

    for (uint32_t i = 0; i < header.directMethodsSize; i++) {
        ReadClassDataMethod(pData, &result->directMethods[i]);
    }

    for (uint32_t i = 0; i < header.virtualMethodsSize; i++) {
        ReadClassDataMethod(pData, &result->virtualMethods[i]);
    }

    return result;
}

void writeLeb128(uint8_t ** ptr, uint32_t data)
{
    while (true) {
        uint8_t out = data & 0x7f;
        if (out != data) {
            *(*ptr)++ = out | 0x80;
            data >>= 7;
        } else {
            *(*ptr)++ = out;
            break;
        }
    }
}

uint8_t* EncodeClassData(DexClassData *pData, int& len)
{
    len=0;

    len+=unsignedLeb128Size(pData->header.staticFieldsSize);
    len+=unsignedLeb128Size(pData->header.instanceFieldsSize);
    len+=unsignedLeb128Size(pData->header.directMethodsSize);
    len+=unsignedLeb128Size(pData->header.virtualMethodsSize);

    if (pData->staticFields) {
        for (uint32_t i = 0; i < pData->header.staticFieldsSize; i++) {
            len+=unsignedLeb128Size(pData->staticFields[i].fieldIdx);
            len+=unsignedLeb128Size(pData->staticFields[i].accessFlags);
        }
    }

    if (pData->instanceFields) {
        for (uint32_t i = 0; i < pData->header.instanceFieldsSize; i++) {
            len+=unsignedLeb128Size(pData->instanceFields[i].fieldIdx);
            len+=unsignedLeb128Size(pData->instanceFields[i].accessFlags);
        }
    }

    if (pData->directMethods) {
        for (uint32_t i=0; i<pData->header.directMethodsSize; i++) {
            len+=unsignedLeb128Size(pData->directMethods[i].methodIdx);
            len+=unsignedLeb128Size(pData->directMethods[i].accessFlags);
            len+=unsignedLeb128Size(pData->directMethods[i].codeOff);
        }
    }

    if (pData->virtualMethods) {
        for (uint32_t i=0; i<pData->header.virtualMethodsSize; i++) {
            len+=unsignedLeb128Size(pData->virtualMethods[i].methodIdx);
            len+=unsignedLeb128Size(pData->virtualMethods[i].accessFlags);
            len+=unsignedLeb128Size(pData->virtualMethods[i].codeOff);
        }
    }

    uint8_t * store = (uint8_t *) malloc(len);

    if (!store) {
        return NULL;
    }

    uint8_t * result=store;

    writeLeb128(&store,pData->header.staticFieldsSize);
    writeLeb128(&store,pData->header.instanceFieldsSize);
    writeLeb128(&store,pData->header.directMethodsSize);
    writeLeb128(&store,pData->header.virtualMethodsSize);
    if (pData->staticFields) {
        for (uint32_t i = 0; i < pData->header.staticFieldsSize; i++) {
            writeLeb128(&store,pData->staticFields[i].fieldIdx);
            writeLeb128(&store,pData->staticFields[i].accessFlags);
        }
    }

    if (pData->instanceFields) {
        for (uint32_t i = 0; i < pData->header.instanceFieldsSize; i++) {
            writeLeb128(&store,pData->instanceFields[i].fieldIdx);
            writeLeb128(&store,pData->instanceFields[i].accessFlags);
        }
    }

    if (pData->directMethods) {
        for (uint32_t i=0; i<pData->header.directMethodsSize; i++) {
            writeLeb128(&store,pData->directMethods[i].methodIdx);
            writeLeb128(&store,pData->directMethods[i].accessFlags);
            writeLeb128(&store,pData->directMethods[i].codeOff);
        }
    }

    if (pData->virtualMethods) {
        for (uint32_t i=0; i<pData->header.virtualMethodsSize; i++) {
            writeLeb128(&store,pData->virtualMethods[i].methodIdx);
            writeLeb128(&store,pData->virtualMethods[i].accessFlags);
            writeLeb128(&store,pData->virtualMethods[i].codeOff);
        }
    }

    free(pData);
    return result;
}

uint8_t* codeitem_end(const u1** pData)
{
    uint32_t num_of_list = readUnsignedLeb128(pData);
    for (;num_of_list>0;num_of_list--) {
        int32_t num_of_handlers=readSignedLeb128(pData);
        int num=num_of_handlers;
        if (num_of_handlers<=0) {
            num=-num_of_handlers;
        }
        for (; num > 0; num--) {
            readUnsignedLeb128(pData);
            readUnsignedLeb128(pData);
        }
        if (num_of_handlers<=0) {
            readUnsignedLeb128(pData);
        }
    }
    return (uint8_t*)(*pData);
}

void writepagesU8(uint8_t * addr, FILE *fp, int length)
{
	int j = length / 4095;
        for(int i = 0; i<= j; i++)
        {
                int cLength = (j -i) == 0 ? length % 4095 :  4095;
                fwrite(addr, 1, cLength, fp);
                fflush(fp);
                addr += cLength;
        }
}

void writepages(const u1 *addr, FILE *fp, int length)
{
        int j = length / 4095;
        for(int i = 0; i<= j; i++)
        {
                int cLength = (j -i) == 0 ? length % 4095 :  4095;
                fwrite(addr, 1, cLength, fp);
                fflush(fp);
                addr += cLength;
        }
}

void writepagesChar(char *addr, FILE *fp, int length)
{
        int j = length / 4095;
        for(int i = 0; i<= j; i++)
        {
                int cLength = (j -i) == 0 ? length % 4095 :  4095;
                fwrite(addr, 1, cLength, fp);
                fflush(fp);
                addr += cLength;
        }
}

void* DumpClass(void *parament)
{
  DvmDex* pDvmDex=((struct arg*)parament)->pDvmDex;
  char const * dumppath=((struct arg*)parament)->dumppath;
  DexFile* pDexFile=pDvmDex->pDexFile;

  u4 time=dvmGetRelativeTimeMsec();
  GOSSIP("GOT IT begin: %d ms",time);

  char *path = new char[100];
  strcpy(path,dumppath);
  strcat(path,"/classdef");
  FILE *fp = fopen(path, "wb+");

  strcpy(path,dumppath);
  strcat(path,"/extra");
  FILE *fp1 = fopen(path,"wb+");

  char padding=0;
  const char* header="Landroid";
  unsigned int num_class_defs=pDexFile->pHeader->classDefsSize;
  uint32_t total_pointer=pDexFile->pHeader->fileSize;
  uint32_t rec=total_pointer;

  while (total_pointer&3) {
      total_pointer++;
  }

  int inc=total_pointer-rec;
  uint32_t start = pDexFile->pHeader->classDefsOff+sizeof(DexClassDef)*num_class_defs;
  uint32_t end = pDexFile->pHeader->fileSize;

  for (size_t i=0;i<num_class_defs;i++) 
  {
      bool need_extra=false;
      const u1* data=NULL;
      DexClassData* pData = NULL;
      bool pass=false;
      const DexClassDef *pClassDef = dexGetClassDef(pDvmDex->pDexFile, i);
      const char *descriptor = dexGetClassDescriptor(pDvmDex->pDexFile,pClassDef);

      if(!strncmp(header,descriptor,8)||!pClassDef->classDataOff)
      {
          pass=true;
          goto classdef;
      }

      if(pClassDef->classDataOff<start || pClassDef->classDataOff>end)
      {
          need_extra=true;
      }
      data=dexGetClassData(pDexFile,pClassDef);
      pData = ReadClassData(&data);
      if (!pData) {
          continue;
      }
      if (pData->directMethods) {
          for (uint32_t i=0; i<pData->header.directMethodsSize; i++) {
              u4 codeitem_off = pData->directMethods[i].codeOff;
              if ((codeitem_off<start || codeitem_off>end) && codeitem_off!=0) {
		  need_extra=true;
		  DexCode *code = (DexCode*)(pDexFile->baseAddr + codeitem_off);
                  pData->directMethods[i].codeOff = total_pointer;
                  uint8_t *item=(uint8_t *) code;
                  int code_item_len = 0;
                  if (code->triesSize) {
                      const u1 * handler_data = dexGetCatchHandlerData(code);
                      const u1** phandler=(const u1**)&handler_data;
                      uint8_t * tail=codeitem_end(phandler);
                      code_item_len = (int)(tail-item);
                  }else{
                      code_item_len = 16+code->insnsSize*2;
                  }

                  GOSSIP("GOT IT method code changed");

                  //fwrite(item,1,code_item_len,fp1);
                  writepagesU8(item, fp1, code_item_len);
		  fflush(fp1);
                  total_pointer+=code_item_len;
                  while (total_pointer&3) {
                      fwrite(&padding,1,1,fp1);
                      fflush(fp1);
                      total_pointer++;
                  }
		  GOSSIP1("total_pointer:%d,code_item_len:%d\n", total_pointer, code_item_len);
              }
          }
      }
      if (pData->virtualMethods) {
          for (uint32_t i=0; i<pData->header.virtualMethodsSize; i++) {
              u4 codeitem_off = pData->virtualMethods[i].codeOff;

              if ((codeitem_off<start || codeitem_off>end)&&codeitem_off!=0) {
                  need_extra=true;
		  DexCode *code = (DexCode*)(pDexFile->baseAddr + codeitem_off);
                  pData->virtualMethods[i].codeOff = total_pointer;
                  uint8_t *item=(uint8_t *) code;
                  int code_item_len = 0;
                  if (code->triesSize) {
                      const u1 *handler_data = dexGetCatchHandlerData(code);
                      const u1** phandler=(const u1**)&handler_data;
                      uint8_t * tail=codeitem_end(phandler);
                      code_item_len = (int)(tail-item);
                  }else{
                      code_item_len = 16+code->insnsSize*2;
                  }

                  GOSSIP("GOT IT method code changed");

                  //fwrite(item,1,code_item_len,fp1);
                  writepagesU8(item, fp1, code_item_len);
		  fflush(fp1);
                  total_pointer+=code_item_len;
                  while (total_pointer&3) {
                      fwrite(&padding,1,1,fp1);
                      fflush(fp1);
                      total_pointer++;
                  }
		  GOSSIP1("total_pointer:%d,code_item_len:%d\n", total_pointer, code_item_len);
              }
          }
      }

classdef:
       DexClassDef temp=*pClassDef;
       uint8_t *p = (uint8_t *)&temp;
       if (need_extra) {
           GOSSIP("GOT IT classdata before");
           int class_data_len = 0;
           uint8_t *out = EncodeClassData(pData,class_data_len);
           if (!out) {
               continue;
           }

           temp.classDataOff = total_pointer;
	   GOSSIP1("total_pointer:%d\n", total_pointer); 
	   GOSSIP1("OUT:%d,%d\n",*out, *(out+1));
           fwrite(out,1,class_data_len,fp1);
           fflush(fp1);
           total_pointer+=class_data_len;
           while (total_pointer&3) {
               fwrite(&padding,1,1,fp1);
               fflush(fp1);
               total_pointer++;
           }
           free(out);
           GOSSIP("GOT IT classdata written");
       }else{
           if (pData) {
               free(pData);
           }
       }

       if (pass) {
           temp.classDataOff=0;
           temp.annotationsOff=0;
       }
       temp.annotationsOff=0;
       GOSSIP("GOT IT classdef");
       fwrite(p, sizeof(DexClassDef), 1, fp);
       fflush(fp);
  }

  fclose(fp1);
  fclose(fp);

  fp = ((struct arg*)parament)->dexFile;
  rewind(fp);

  int fd=-1;
  int r=-1;
  int len=0;  
  char *addr=NULL;
  struct stat st;

  strcpy(path,dumppath);
  strcat(path,"/part1");

  fd=open(path,O_RDONLY,0666);
  if (fd==-1) {
      return NULL;
  }

  r=fstat(fd,&st);  
  if(r==-1){  
      close(fd);  
      return NULL;
  }

  len=st.st_size;
  addr=(char *)mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
  writepagesChar(addr, fp, len);
  fflush(fp);
  munmap(addr,len);
  close(fd);

  strcpy(path,dumppath);
  strcat(path,"/classdef");

  fd=open(path,O_RDONLY,0666);
  if (fd==-1) {
      return NULL;
  }

  r=fstat(fd,&st);  
  if(r==-1){  
      close(fd);  
      return NULL;
  }

  len=st.st_size;
  addr=(char *)mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
  fwrite(addr,1,len,fp);
  fflush(fp);
  munmap(addr,len);
  close(fd);

  strcpy(path,dumppath);
  strcat(path,"/data");

  fd=open(path,O_RDONLY,0666);
  if (fd==-1) {
      return NULL;
  }

  r=fstat(fd,&st);  
  if(r==-1){  
      close(fd);  
      return NULL;
  }

  len=st.st_size;
  addr=(char *)mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
  writepagesChar(addr, fp, len);
  fflush(fp);
  munmap(addr,len);
  close(fd);

  while (inc>0) {
      fwrite(&padding,1,1,fp);
      fflush(fp);
      inc--;
  }

  strcpy(path,dumppath);
  strcat(path,"/extra");

  fd=open(path,O_RDONLY,0666);
  if (fd==-1) {
      return NULL;
  }

  r=fstat(fd,&st);  
  if(r==-1){  
      close(fd);  
      return NULL;
  }

  len=st.st_size;
  addr=(char *)mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
  writepagesChar(addr, fp, len);
  fflush(fp);
  munmap(addr,len);
  close(fd);

  fclose(fp);
  delete path;

  time=dvmGetRelativeTimeMsec();
  GOSSIP("GOT IT end: %d ms",time);

  return NULL;
}

void dexdumpTrueOut(const Method * const m , FILE * file, char const * dumppath) 
{
	DvmDex* pDvmDex=m->clazz->pDvmDex;
	DexFile* pDexFile=pDvmDex->pDexFile;

        char * temp=new char[100];
        strcpy(temp,dumppath);
        strcat(temp,"/part1");
        FILE *fp = fopen(temp, "wb+");
        const u1 *addr = pDexFile->baseAddr;
        int length=int(pDexFile->pHeader->classDefsOff);
        writepages(addr, fp, length);
        fflush(fp);
        fclose(fp);

        strcpy(temp,dumppath);
        strcat(temp,"/data");
        fp=fopen(temp, "wb+");
        addr=pDexFile->baseAddr+pDexFile->pHeader->classDefsOff+sizeof(DexClassDef)*pDexFile->pHeader->classDefsSize;
        length=int(pDexFile->pHeader->fileSize-pDexFile->pHeader->classDefsOff-sizeof(DexClassDef)*pDexFile->pHeader->classDefsSize);
	writepages(addr, fp, length);
        fflush(fp);
        fclose(fp);
        delete temp;

        param.pDvmDex=pDvmDex;
	param.dumppath=dumppath;
	param.dexFile=file;
        pthread_t dumpthread;
        dvmCreateInternalThread(&dumpthread,"ClassDumper",DumpClass,(void*)&param);                   
	//DumpClass((void*)&param);
}
