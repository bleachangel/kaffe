/*
 * itypes.c
 * Internal types.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "classMethod.h"
#include "jtypes.h"
#include "itypes.h"
#include "baseClasses.h"
#include "stringSupport.h"
#include "object.h"

Hjava_lang_Class* intClass;
Hjava_lang_Class* longClass;
Hjava_lang_Class* booleanClass;
Hjava_lang_Class* charClass;
Hjava_lang_Class* floatClass; 
Hjava_lang_Class* doubleClass;
Hjava_lang_Class* byteClass; 
Hjava_lang_Class* shortClass;     
Hjava_lang_Class* voidClass;

extern gcFuncs gcClassObject;

Hjava_lang_Class* types[MAXTYPES];

static void
initPrimClass(Hjava_lang_Class** class, char* name, char sig, int len)
{
	Hjava_lang_Class* clazz = newClass();
	(*class) = clazz;
	gc_add_ref(clazz);

	clazz->dtable = _PRIMITIVE_DTABLE;
	clazz->name = utf8ConstNew(name, -1);
	CLASS_PRIM_SIG(clazz) = sig;
        CLASS_PRIM_NAME(clazz) = utf8ConstNew(&sig, 1);
	TYPE_PRIM_SIZE(clazz) = len;
}

/*
 * Intialise the internal types.
 */
void
initTypes(void)
{
	initPrimClass(&byteClass, "byte", 'B', 1);
	initPrimClass(&shortClass, "short", 'S', 2);
	initPrimClass(&intClass, "int", 'I', 4);
	initPrimClass(&longClass, "long", 'J', 8);
	initPrimClass(&booleanClass, "boolean", 'Z', 1);
	initPrimClass(&charClass, "char", 'C', 2);
	initPrimClass(&floatClass, "float", 'F', 4);
	initPrimClass(&doubleClass, "double", 'D', 8);
	initPrimClass(&voidClass, "void", 'V', 0);

	TYPE_CLASS(TYPE_Boolean) = booleanClass;
	TYPE_CLASS(TYPE_Char) = charClass;
	TYPE_CLASS(TYPE_Float) = floatClass;
	TYPE_CLASS(TYPE_Double) = doubleClass;
	TYPE_CLASS(TYPE_Byte) = byteClass;
	TYPE_CLASS(TYPE_Short) = shortClass;
	TYPE_CLASS(TYPE_Int) = intClass;
	TYPE_CLASS(TYPE_Long) = longClass;
}

/*
 * Finish the internal types.
 */
void
finishTypes(void)
{
	byteClass->head.dtable = ClassClass->dtable;
	shortClass->head.dtable = ClassClass->dtable;
	intClass->head.dtable = ClassClass->dtable;
	longClass->head.dtable = ClassClass->dtable;
	booleanClass->head.dtable = ClassClass->dtable;
	charClass->head.dtable = ClassClass->dtable;
	floatClass->head.dtable = ClassClass->dtable;
	doubleClass->head.dtable = ClassClass->dtable;
	voidClass->head.dtable = ClassClass->dtable;
}

Hjava_lang_Class*
getClassFromSignature(const char* sig, Hjava_lang_ClassLoader* loader, errorInfo *einfo)
{
	return (classFromSig(&sig, loader, einfo));
}

Hjava_lang_Class*
classFromSig(const char** strp, Hjava_lang_ClassLoader* loader, errorInfo *einfo)
{
	Hjava_lang_Class* cl;
	Utf8Const* utf8;
	const char* start;
	const char* end;

	switch (*(*strp)++) {
	case 'V': return (voidClass);
	case 'I': return (intClass);
	case 'Z': return (booleanClass);
	case 'S': return (shortClass);
	case 'B': return (byteClass);
	case 'C': return (charClass);
	case 'F': return (floatClass);
	case 'D': return (doubleClass);
	case 'J': return (longClass);
	case '[': return (lookupArray(classFromSig(strp, loader, einfo)));
	case 'L':
		start = *strp;
		for (end = start; *end != 0 && *end != ';'; end++)
			;
		*strp = end;
		if (*end != 0) {
			(*strp)++;
		}
		utf8 = utf8ConstNew(start, end - start);
		cl = loadClass(utf8, loader, einfo);
		utf8ConstRelease(utf8);
		return(cl);
	default:
		/* malformed signature */
		SET_LANG_EXCEPTION(einfo, VerifyError);
		return (NULL);
	}
}
