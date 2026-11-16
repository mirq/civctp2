#include "c3.h"
#include "ldlif.h"
#include "CivPaths.h"
#include "c3errors.h"

#include "StringHash.h"
#include "pointerlist.h"
#include "AvlTree.h"

#include "aui_ui.h"

#include "ldl_data.hpp"
#include "ldl_attr.hpp"
#include "c3errors.h"
#include <inttypes.h>

class LDLString {
	char *m_name;
  public:
	LDLString(const char *text) {
		m_name = new char[strlen(text) + 1];
		strcpy(m_name, text);
	}
	~LDLString() {
		delete [] m_name;
	}

	char * GetName() const { return m_name; }
};

StringHash<LDLString> *s_ldlStringHash = NULL;

PointerList<ldl_datablock> *s_blockStack = NULL;
AvlTree<ldl_datablock *> *s_blockTree = NULL;
PointerList<ldl_datablock> *s_topLevelList = NULL;

extern "C" { void ldlif_report_error(char *text); }

void ldlif_report_error(char *text)
{
	c3errors_ErrorDialog("LDL", text);
}

ldl_datablock *ldlif_find_block(char const * name)
{
	Comparable<ldl_datablock *> *myKey;
	ldl_datablock dummy(aui_UI::CalculateHash(name));
	myKey = s_blockTree->Search(&dummy);
	if(myKey) {
		return myKey->Key();
	}
	return NULL;
}

int ldlif_find_file(const char *filename, char *fullpath)
{
	if(!g_civPaths->FindFile(C3DIR_LAYOUT, filename, fullpath, false, true, true, false))
		return 0;
	return 1;
}

int ldlif_find_localized_file(const char *filename, char * fullpath)
{
	if (!g_civPaths->FindFile(C3DIR_LAYOUT, filename, fullpath))
		return 0;
	return 1;
}


#if defined(CTP2_AMIGA_BUILD)
// An index of the stored names beside the StringHash (which keeps owning
// them): open addressing on a case-folded FNV-1a hash. StringHash's key is a
// sum of character pairs modulo 1024, so similar names share chains, each
// entry compared with a case-insensitive compare; looking names up was the
// largest single part of building a window from its layout on a 68060.
namespace
{
	char **  s_nameIndex     = NULL;
	uint32_t s_nameIndexSize = 0;    // a power of two
	uint32_t s_nameIndexUsed = 0;

	inline uint32_t ldlif_name_hash(const char * name)
	{
		uint32_t h = 2166136261u;
		for (; *name; ++name)
		{
			uint32_t c = (unsigned char) *name;
			if (c - 'A' < 26u) c += 'a' - 'A';
			h = (h ^ c) * 16777619u;
		}
		return h;
	}

	void ldlif_name_index_put(char * name)
	{
		uint32_t i = ldlif_name_hash(name) & (s_nameIndexSize - 1);
		while (s_nameIndex[i]) i = (i + 1) & (s_nameIndexSize - 1);
		s_nameIndex[i] = name;
	}

	void ldlif_name_index_add(char * name)
	{
		if (2 * (s_nameIndexUsed + 1) > s_nameIndexSize)
		{
			char **        old     = s_nameIndex;
			uint32_t const oldSize = s_nameIndexSize;
			s_nameIndexSize = oldSize ? 2 * oldSize : 8192;
			s_nameIndex     = new char *[s_nameIndexSize];
			memset(s_nameIndex, 0, s_nameIndexSize * sizeof(char *));
			for (uint32_t n = 0; n < oldSize; ++n)
			{
				if (old[n]) ldlif_name_index_put(old[n]);
			}
			delete [] old;
		}
		ldlif_name_index_put(name);
		++s_nameIndexUsed;
	}

	void ldlif_name_index_clear()
	{
		delete [] s_nameIndex;
		s_nameIndex     = NULL;
		s_nameIndexSize = 0;
		s_nameIndexUsed = 0;
	}
}

char *ldlif_findnameptr(const char *name)
{
	if (!s_nameIndexSize) return NULL;
	uint32_t i = ldlif_name_hash(name) & (s_nameIndexSize - 1);
	for (char * stored; (stored = s_nameIndex[i]) != NULL; i = (i + 1) & (s_nameIndexSize - 1))
	{
		if (strcasecmp(stored, name) == 0) return stored;
	}
	return NULL;
}
#endif

char *ldlif_getnameptr(const char *name)
{
#if defined(CTP2_AMIGA_BUILD)
	if (char * stored = ldlif_findnameptr(name))
	{
		return stored;
	}
#else
	const LDLString *str = s_ldlStringHash->Get(name);
	if(str) {
		return str->GetName();
	}
#endif

	LDLString *newstr = new LDLString(name);
	s_ldlStringHash->Add(newstr);
#if defined(CTP2_AMIGA_BUILD)
	ldlif_name_index_add(newstr->GetName());
#endif
	return newstr->GetName();
}

char *ldlif_getstringptr(const char *text)
{
	return ldlif_getnameptr(text);
}

void ldlif_add_name(void **newnames, char *name, void *oldnames)
{
	PointerList<char> *namelist = (PointerList<char> *)oldnames;
	if(!namelist) {
		namelist = new PointerList<char>;
	}
	namelist->AddHead(name);
	*newnames = (void *)namelist;
}

void ldlif_init_log()
{
#ifdef _DEBUG
	FILE *f = fopen("logs" FILE_SEP "ldlparselog.txt", "w");
	if(f) {
		fprintf(f, "%" PRId64 "\n", time(0));
		fclose(f);
	}
#endif
}
void ldlif_log(const char *format, ...)
{
#ifdef _DEBUG
	va_list list;
	va_start(list, format);

	FILE *f = fopen("logs" FILE_SEP "ldlparselog.txt", "a");
	vfprintf(f, format, list);
	fclose(f);
	va_end(list);
#endif
}

void ldlif_indent_log(int indent)
{
#ifdef _DEBUG
	int i;
	for(i = 0; i < indent; i++) {
		ldlif_log("    ");
	}
#endif
}

void ldlif_start_block(void *names)
{
	PointerList<char> *namelist = (PointerList<char> *)names;
	Assert(namelist);

	ldl_datablock *block = new ldl_datablock(namelist);

	if(s_blockStack->GetTail()) {
		s_blockStack->GetTail()->AddChild(block);
	}
	s_blockStack->AddTail(block);
	ldlif_add_block_to_tree(block);
}

static cmp_t ldlif_compare_blocks(ldl_datablock *b1, ldl_datablock *b2)
{
	if(b1->GetHash() < b2->GetHash()) return MIN_CMP;
	if(b1->GetHash() > b2->GetHash()) return MAX_CMP;
	return EQ_CMP;
}

void ldlif_add_block_to_tree(ldl_datablock *block)
{
	char fullname[256];
	block->GetFullName(fullname);

	ldlif_log("Added: %s\n", fullname);

	block->SetHash(aui_UI::CalculateHash(fullname));
	Comparable<ldl_datablock *> *cmp = new Comparable<ldl_datablock *>(block, ldlif_compare_blocks);
	if(s_blockTree->Insert(cmp)) {
		char buf[300];
		sprintf(buf, "Duplicate block %s\n", fullname);
		ldlif_report_error(buf);
		delete cmp;
	}

	PointerList<ldl_datablock> *childList = block->GetChildList();
	PointerList<ldl_datablock>::Walker walk(childList);
	while(walk.IsValid()) {
		ldlif_add_block_to_tree(walk.GetObj());
		walk.Next();
	}
}

void ldlif_remove_block_from_tree(ldl_datablock *block)
{
	Comparable<ldl_datablock *> *myKey;
	char fullname[256];
	block->GetFullName(fullname);
	ldl_datablock dummy(aui_UI::CalculateHash(fullname));
	myKey = s_blockTree->Delete(&dummy);
	if(myKey) {
		delete myKey;
	}
}

void *ldlif_end_block(void *names)
{
	PointerList<char> *namelist = (PointerList<char> *)names;
	Assert(namelist);

	ldl_datablock *block = s_blockStack->RemoveTail();

	block->AddTemplateChildren();

	if(!s_blockStack->GetTail()) {
		s_topLevelList->AddTail(block);

	}

	delete namelist;
	return block;
}

void *ldlif_add_empty_block(void *names)
{
	ldlif_start_block(names);
	return ldlif_end_block(names);
}

void ldlif_add_bool_attribute(const char *name, int val)
{
	ldl_attributeValue<bool> *attr = new ldl_attributeValue<bool>(name, ATTRIBUTE_TYPE_BOOL, val != 0);
	s_blockStack->GetTail()->AddAttribute(attr);
}

void ldlif_add_int_attribute(const char *name, int val)
{
	ldl_attributeValue<int> *attr = new ldl_attributeValue<int>(name, ATTRIBUTE_TYPE_INT, val);
	s_blockStack->GetTail()->AddAttribute(attr);
}

void ldlif_add_float_attribute(const char *name, double val)
{
	ldl_attributeValue<double> *attr = new ldl_attributeValue<double>(name, ATTRIBUTE_TYPE_DOUBLE, val);
	s_blockStack->GetTail()->AddAttribute(attr);
}

void ldlif_add_string_attribute(const char *name, const char *val)
{
	ldl_attributeValue<const char *> *attr = new ldl_attributeValue<const char *>(name, ATTRIBUTE_TYPE_STRING, val);
	s_blockStack->GetTail()->AddAttribute(attr);
}

void ldlif_allocate_stuff()
{
	s_ldlStringHash = new StringHash<LDLString>(1024);
	s_blockStack = new PointerList<ldl_datablock>;
	s_blockTree = new AvlTree<ldl_datablock *>;
	s_topLevelList = new PointerList<ldl_datablock>;
}

void ldlif_deallocate_stuff()
{
	delete s_blockStack;
	s_blockStack = NULL;

	s_topLevelList->DeleteAll();
	delete s_topLevelList;
	s_topLevelList = NULL;

	delete s_ldlStringHash;
	s_ldlStringHash = NULL;
#if defined(CTP2_AMIGA_BUILD)
	ldlif_name_index_clear();
#endif

	delete s_blockTree;
	s_blockTree = NULL;
}
