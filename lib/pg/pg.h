#pragma once 


#define PG_DECLARE(_type, _name)                                        \
    extern _type _name ## _System;                                      \
    extern _type _name ## _Copy;                                        \
    static inline const _type* _name(void) { return &_name ## _System; }\
    static inline _type* _name ## Mutable(void) { return &_name ## _System; }\
    struct _dummy                                                       \
    /**/
