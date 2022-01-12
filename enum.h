#ifndef AEM_ENUM_H
#define AEM_ENUM_H

// Enum helpers
#define AEM_ENUM_DEFINE_ITER(label, name) label,
//#define AEM_ENUM_ELEMENTS(def) def(AEM_ENUM_DEFINE_ITER)

#define AEM_ENUM_CASE_NAME_ITER(label, name) case label: return #name;
//#define AEM_ENUM_CASE_NAME(def) def(AEM_ENUM_CASE_NAME_ITER)

#define AEM_ENUM_DECLARE(tag, def) \
	enum tag { \
		def##_DEFINE(AEM_ENUM_DEFINE_ITER) \
		def##_MAX \
	}; \
	const char *tag##_name(enum tag x);

#define AEM_ENUM_DEFINE(tag, def) \
	const char *tag##_name(enum tag x) \
	{ \
		switch (x) { \
		def##_DEFINE(AEM_ENUM_CASE_NAME_ITER) \
		default: return NULL; \
		} \
	}


#endif /* AEM_ENUM_H */
