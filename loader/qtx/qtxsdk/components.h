typedef long ComponentResult;
typedef unsigned char UInt8;
typedef int OSType;

// codec private shit:
typedef void *GlobalsPtr;
typedef void **Globals;


struct ComponentParameters {
    UInt8                           flags;                      /* call modifiers: sync/async, deferred, immed, etc */
    UInt8                           paramSize;                  /* size in bytes of actual parameters passed to this call */
    short                           what;                       /* routine selector, negative for Component management calls */
    long                            params[1];                  /* actual parameters for the indicated routine */
};
typedef struct ComponentParameters      ComponentParameters;


struct ComponentDescription {
    OSType                          componentType;              /* A unique 4-byte code indentifying the command set */
    OSType                          componentSubType;           /* Particular flavor of this instance */
    OSType                          componentManufacturer;      /* Vendor indentification */
    unsigned long                   componentFlags;             /* 8 each for Component,Type,SubType,Manuf/revision */
    unsigned long                   componentFlagsMask;         /* Mask for specifying which flags to consider in search, zero during registration */
};
typedef struct ComponentDescription     ComponentDescription;


struct ResourceSpec {
    OSType                          resType;                    /* 4-byte code    */
    short                           resID;                      /*         */
};
typedef struct ResourceSpec             ResourceSpec;


struct ComponentResource {
    ComponentDescription            cd;                         /* Registration parameters */
    ResourceSpec                    component;                  /* resource where Component code is found */
    ResourceSpec                    componentName;              /* name string resource */
    ResourceSpec                    componentInfo;              /* info string resource */
    ResourceSpec                    componentIcon;              /* icon resource */
};
typedef struct ComponentResource        ComponentResource;
typedef ComponentResource *             ComponentResourcePtr;
typedef ComponentResourcePtr *          ComponentResourceHandle;


struct ComponentRecord {
    long                            data[1];
};
typedef struct ComponentRecord          ComponentRecord;
typedef ComponentRecord *               Component;


struct ComponentInstanceRecord {
    long                            data[1];
};
typedef struct ComponentInstanceRecord  ComponentInstanceRecord;

typedef ComponentInstanceRecord *       ComponentInstance;

