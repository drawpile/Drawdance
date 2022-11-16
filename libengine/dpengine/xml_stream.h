#ifndef DPENGINE_XML_STREAM
#define DPENGINE_XML_STREAM
#include <dpcommon/common.h>


typedef struct DP_XmlElement DP_XmlElement;

typedef bool (*DP_XmlStartElementFn)(void *user, DP_XmlElement *element);
typedef bool (*DP_XmlTextContentFn)(void *user, size_t len, const char *text);
typedef bool (*DP_XmlEndElementFn)(void *user);


bool DP_xml_stream(size_t size, const char *buffer,
                   DP_XmlStartElementFn on_start_element,
                   DP_XmlTextContentFn on_text_content,
                   DP_XmlEndElementFn on_end_element, void *user);


const char *DP_xml_element_name(DP_XmlElement *element);

const char *DP_xml_element_attribute(DP_XmlElement *element, const char *name);


#endif
