/********************************************************

	Declaration of registry access functions
	Copyright 2000 Eugene Smith (divx@euro.ru)

*********************************************************/


#ifndef REGISTRY_H
#define REGISTRY_H
#ifdef __cplusplus
extern "C" {
#endif
long RegOpenKeyExA(long key, const char* subkey, long reserved, long access, int* newkey);
long RegCloseKey(long key);
long RegQueryValueExA(long key, const char* value, int* reserved, int* type, int* data, int* count);
long RegCreateKeyExA(long key, const char* name, long reserved,
							   void* classs, long options, long security,
							   void* sec_attr, int* newkey, int* status);
long RegSetValueExA(long key, const char* name, long v1, long v2, const void* data, long size);
#ifdef __cplusplus
};
#endif
#endif
