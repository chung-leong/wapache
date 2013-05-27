#ifndef __WAPACHE_CONSTANTS_H
#define __WAPACHE_CONSTANTS_H

struct named_constant {
	const char *name;
	LONG value;
};

#define IDM_ABOUT				104
#define IDM_EXIT				105
#define IDM_CLOSE				106
#define IDM_FOCUS				107

#define CONSTANT_DECL(a)	extern named_constant a##_CONSTANTS[]

#define CONSTANT_START(a)	named_constant a##_CONSTANTS[] = {
#define CONSTANT_END(a)		{NULL} };
#define CONSTANT(e)			{ #e, e },

#define CONSTANT_VALUE(a, b)	GetConstantValue(a##_CONSTANTS, b)

int GetConstantValue(named_constant *list, const char *name);

extern const char * const status_lines[RESPONSE_CODES];

CONSTANT_DECL(MSHTML);

#endif