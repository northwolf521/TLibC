#include "protocol/tlibc_xml_reader.h"
#include "core/tlibc_util.h"
#include "protocol/tlibc_abstract_reader.h"
#include "core/tlibc_error_code.h"
#include "tlibc_xml_reader_l.h"

#include "protocol/tlibc_xml_reader_scanner.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

TLIBC_ERROR_CODE tlibc_xml_reader_init(TLIBC_XML_READER *self, const char *file_name)
{
	TLIBC_ERROR_CODE ret = E_TLIBC_NOERROR;
	FILE* fin;
	char c;
	TLIBC_XML_READER_SCANNER_CONTEXT *scanner = NULL;

	strncpy(self->scanner_context.yylloc.file_name, file_name, TLIBC_MAX_FILE_PATH_LENGTH);
	self->scanner_context.yylloc.file_name[TLIBC_MAX_FILE_PATH_LENGTH - 1] = 0;


	self->buff_size = 0;
	fin = fopen(file_name, "rb");
	if(fin == NULL)
	{
		ret = E_TLIBC_CAN_NOT_OPEN_FILE;
		goto ERROR_RET;
	}
	while((c = (char)fgetc(fin)) != EOF)
	{
		if(self->buff_size >= TLIBC_XML_READER_BUFF_SIZE)
		{
			ret = E_TLIBC_OUT_OF_MEMORY;
			goto ERROR_RET;
		}
		self->buff[self->buff_size] = c;
		++self->buff_size;
	}

	scanner = &self->scanner_context;
	scanner->yy_state = yycINITIAL;
	scanner->yy_start = self->buff;
	scanner->yy_cursor = scanner->yy_start;
	scanner->yy_limit = scanner->yy_start + TLIBC_XML_READER_BUFF_SIZE;	
	scanner->yy_marker = scanner->yy_start;
	scanner->yy_last = scanner->yy_start;
	scanner->yylineno = 1;
	scanner->yycolumn = 1;

	self->struct_deep = 0;



	tlibc_abstract_reader_init(&self->super);


	self->super.read_struct_begin = tlibc_xml_read_struct_begin;
	self->super.read_struct_end = tlibc_xml_read_struct_end;
	self->super.read_enum_begin = tlibc_xml_read_enum_begin;

	self->super.read_vector_begin = tlibc_xml_read_vector_begin;
	self->super.read_vector_end = tlibc_xml_read_vector_end;
	self->super.read_field_begin = tlibc_xml_read_field_begin;
	self->super.read_field_end = tlibc_xml_read_field_end;

	self->super.read_tint8 = tlibc_xml_read_tint8;
	self->super.read_tint16 = tlibc_xml_read_tint16;
	self->super.read_tint32 = tlibc_xml_read_tint32;
	self->super.read_tint64 = tlibc_xml_read_tint64;

	self->super.read_tuint8 = tlibc_xml_read_tuint8;
	self->super.read_tuint16 = tlibc_xml_read_tuint16;
	self->super.read_tuint32 = tlibc_xml_read_tuint32;
	self->super.read_tuint64 = tlibc_xml_read_tuint64;

	self->super.read_tdouble = tlibc_xml_read_tdouble;
	self->super.read_tstring = tlibc_xml_read_tstring;
	self->super.read_tchar = tlibc_xml_read_tchar;
	self->pre_read_uint16_field_once = hpfalse;
	self->ignore_int32_once = hpfalse;

	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_struct_begin(TLIBC_ABSTRACT_READER *super, const char *struct_name)
{
	TLIBC_ERROR_CODE ret = E_TLIBC_NOERROR;
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);	
	TLIBC_XML_READER_TOKEN token;

	if(self->struct_deep != 0)
	{
		goto done;
	}

	token = tlibc_xml_reader_get_token(&self->scanner_context);
	if(token != tok_tag_begin)
	{
		ret = E_TLIBC_XML_SYNTAX_ERROR;
		goto ERROR_RET;
	}

	if(strcmp(struct_name, self->scanner_context.tag_name) != 0)
	{
		ret = E_TLIBC_XML_TAG_MISMATCH;
		goto ERROR_RET;
	}

done:
	++self->struct_deep;
	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_struct_end(TLIBC_ABSTRACT_READER *super, const char *struct_name)
{
	TLIBC_ERROR_CODE ret;
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	TLIBC_XML_READER_TOKEN token;

	if(self->struct_deep != 0)
	{
		goto done;		
	}

	token = tlibc_xml_reader_get_token(&self->scanner_context);
	if(token != tok_tag_end)
	{
		ret = E_TLIBC_XML_SYNTAX_ERROR;
		goto ERROR_RET;
	}
	if(strcmp(struct_name, self->scanner_context.tag_name) != 0)
	{
		ret = E_TLIBC_XML_TAG_MISMATCH;
		goto ERROR_RET;
	}	
	
done:
	--self->struct_deep;
	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_enum_begin(TLIBC_ABSTRACT_READER *super, const char *enum_name)
{
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	TLIBC_UNUSED(enum_name);

	self->ignore_int32_once = hptrue;

	return E_TLIBC_NOERROR;
}

TLIBC_ERROR_CODE tlibc_xml_read_vector_begin(TLIBC_ABSTRACT_READER *super)
{
	TLIBC_ERROR_CODE ret = E_TLIBC_NOERROR;
	tuint32 level;
	tuint16 count;
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);	
	TLIBC_XML_READER_SCANNER_CONTEXT scanner_context = self->scanner_context;
	count = 0;
	level = 0;
	do
	{
		TLIBC_XML_READER_TOKEN token = tlibc_xml_reader_get_token(&scanner_context);
		switch(token)
		{
			case tok_tag_begin:				
				if(level == 1)
				{
					++count;
				}
				++level;
				break;
			case tok_tag_end:
				--level;
				break;
			default:
				ret = E_TLIBC_XML_SYNTAX_ERROR;
				goto ERROR_RET;
		}
	}while(level != 0);

	ret = tlibc_xml_read_field_begin(super, "vector");
	self->pre_read_uint16_field_once = hptrue;
	self->ui16 = count;

	return ret;
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_vector_end(TLIBC_ABSTRACT_READER *super)
{
	return tlibc_xml_read_field_end(super, "vector");
}

TLIBC_ERROR_CODE tlibc_xml_read_field_begin(TLIBC_ABSTRACT_READER *super, const char *var_name)
{	
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	TLIBC_ERROR_CODE ret;
	TLIBC_XML_READER_TOKEN token;	

	if(self->pre_read_uint16_field_once)
	{
		goto done;
	}
	token = tlibc_xml_reader_get_token(&self->scanner_context);
	if(token != tok_tag_begin)
	{
		ret = E_TLIBC_XML_SYNTAX_ERROR;
		goto ERROR_RET;
	}
	if(strcmp(self->scanner_context.tag_name, var_name) != 0)
	{
		ret = E_TLIBC_XML_TAG_MISMATCH;
		goto ERROR_RET;
	}

done:
	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_field_end(TLIBC_ABSTRACT_READER *super, const char *var_name)
{
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);	
	TLIBC_ERROR_CODE ret;
	TLIBC_XML_READER_TOKEN token;

	if(self->pre_read_uint16_field_once)
	{
		self->pre_read_uint16_field_once = hpfalse;
		goto done;
	}
	token = tlibc_xml_reader_get_token(&self->scanner_context);
	if(token != tok_tag_end)
	{
		ret = E_TLIBC_XML_SYNTAX_ERROR;
		goto ERROR_RET;
	}
	if(strcmp(self->scanner_context.tag_name, var_name) != 0)
	{
		ret = E_TLIBC_XML_TAG_MISMATCH;
		goto ERROR_RET;
	}

done:
	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tdouble(TLIBC_ABSTRACT_READER *super, double *val)
{
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	TLIBC_ERROR_CODE ret;
	
	ret = tlibc_xml_reader_get_content(&self->scanner_context);
	if(ret != E_TLIBC_NOERROR)
	{
		goto ERROR_RET;
	}
	
	
	errno = 0;
	*self->scanner_context.content_end = 0;
	sscanf(self->scanner_context.content_begin, "%lf", val);
	*self->scanner_context.content_end = '<';
	if(errno != 0)
	{
		ret = E_TLIBC_XML_CONTENT_CONVERT_ERROR;
		goto ERROR_RET;
	}

	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tint8(TLIBC_ABSTRACT_READER *super, tint8 *val)
{
	tint64 i64;
	TLIBC_ERROR_CODE ret = tlibc_xml_read_tint64(super, &i64);
	*val = (tint8)i64;
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tint16(TLIBC_ABSTRACT_READER *super, tint16 *val)
{
	tint64 i64;
	TLIBC_ERROR_CODE ret = tlibc_xml_read_tint64(super, &i64);
	*val = (tint16)i64;
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tint32(TLIBC_ABSTRACT_READER *super, tint32 *val)
{
	tint64 i64;
	TLIBC_ERROR_CODE ret = E_TLIBC_NOERROR;
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	if(self->ignore_int32_once)
	{
		self->ignore_int32_once = hpfalse;
		ret = E_TLIBC_IGNORE;
		goto done;
	}
	
	ret = tlibc_xml_read_tint64(super, &i64);
	*val = (tint32)i64;
done:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tint64(TLIBC_ABSTRACT_READER *super, tint64 *val)
{
	TLIBC_ERROR_CODE ret = E_TLIBC_NOERROR;
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	tlibc_xml_reader_get_content(&self->scanner_context);


	errno = 0;
	*self->scanner_context.content_end = 0;
	sscanf(self->scanner_context.content_begin, "%lld", val);
	*self->scanner_context.content_end = '<';
	if(errno != 0)
	{
		ret = E_TLIBC_XML_CONTENT_CONVERT_ERROR;
		goto ERROR_RET;
	}

	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tuint8(TLIBC_ABSTRACT_READER *super, tuint8 *val)
{
	tuint64 ui64;
	TLIBC_ERROR_CODE ret = tlibc_xml_read_tuint64(super, &ui64);
	*val = (tuint8)ui64;
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tuint32(TLIBC_ABSTRACT_READER *super, tuint32 *val)
{
	tuint64 ui64;
	TLIBC_ERROR_CODE ret = tlibc_xml_read_tuint64(super, &ui64);
	*val = (tuint32)ui64;
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tuint16(TLIBC_ABSTRACT_READER *super, tuint16 *val)
{
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	tuint64 ui64;
	TLIBC_ERROR_CODE ret;
	if(self->pre_read_uint16_field_once)
	{
		ui64 = self->ui16;
		ret = E_TLIBC_NOERROR;
	}
	else
	{
		ret = tlibc_xml_read_tuint64(super, &ui64);
	}
	
	*val = (tuint16)ui64;
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tuint64(TLIBC_ABSTRACT_READER *super, tuint64 *val)
{
	TLIBC_ERROR_CODE ret = E_TLIBC_NOERROR;
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	tlibc_xml_reader_get_content(&self->scanner_context);

	errno = 0;
	*self->scanner_context.content_end = 0;
	sscanf(self->scanner_context.content_begin, "%llu", val);
	*self->scanner_context.content_end = '<';
	if(errno != 0)
	{
		ret = E_TLIBC_XML_CONTENT_CONVERT_ERROR;
		goto ERROR_RET;
	}

	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}


static TLIBC_ERROR_CODE read_char(TLIBC_XML_READER* self, tchar *ch)
{
	char c;
	if(self->scanner_context.content_begin >= self->scanner_context.content_end)
	{		
		goto ERROR_RET;
	}
	c = *self->scanner_context.content_begin++;
	
	if(c == '&')
	{
		char c2;
		if(self->scanner_context.content_begin >= self->scanner_context.content_end)
		{
			goto ERROR_RET;
		}
		c2 = *self->scanner_context.content_begin++;
		if(c2 == 'l')
		{
			//&lt
			*ch = '<';
		}
		else if(c2 == 'g')
		{
			//&gt
			*ch = '>';
		}
		else
		{
			char c3;
			if(self->scanner_context.content_begin >= self->scanner_context.content_end)
			{
				goto ERROR_RET;
			}
			c3 = *self->scanner_context.content_begin++;
			if(c3 == 'm')
			{
				//&amp
				*ch = '&';
			}
			else if(c3 == 'p')
			{
				//&apos
				*ch = '\'';
			}
			else if(c3 == 'u')
			{
				//&auot
				*ch = '\"';
			}
		}
	}
	else
	{
		*ch = c;
	}

	return E_TLIBC_NOERROR;
ERROR_RET:
	return E_TLIBC_OUT_OF_MEMORY;
}

TLIBC_ERROR_CODE tlibc_xml_read_tchar(TLIBC_ABSTRACT_READER *super, char *val)
{
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	TLIBC_ERROR_CODE ret;
	ret = tlibc_xml_reader_get_content(&self->scanner_context);
	if(ret != E_TLIBC_NOERROR)
	{
		goto ERROR_RET;
	}
	return read_char(self, val);
ERROR_RET:
	return ret;
}

TLIBC_ERROR_CODE tlibc_xml_read_tstring(TLIBC_ABSTRACT_READER *super, tchar *str, tuint32 str_len)
{
	TLIBC_XML_READER *self = TLIBC_CONTAINER_OF(super, TLIBC_XML_READER, super);
	tuint32 len = 0;
	TLIBC_ERROR_CODE ret;

	ret = tlibc_xml_reader_get_content(&self->scanner_context);
	if(ret != E_TLIBC_NOERROR)
	{
		goto ERROR_RET;
	}

	while(self->scanner_context.content_begin < self->scanner_context.content_end)
	{
		char c;
		ret = read_char(self, &c);
		if(ret != E_TLIBC_NOERROR)
		{
			goto ERROR_RET;
		}
		if(len >= str_len)
		{
			ret = E_TLIBC_OUT_OF_MEMORY;
			goto ERROR_RET;
		}
		str[len++] = c;
	}
	str[len] = 0;

	return E_TLIBC_NOERROR;
ERROR_RET:
	return ret;
}


