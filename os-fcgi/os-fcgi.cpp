// os-insight.cpp: ���������� ����� ����� ��� ����������� ����������.
//

#ifdef _MSC_VER
#include "win32/stdafx.h"
#include <Windows.h>
#pragma comment (lib, "Ws2_32.lib")
#endif

#include "os/objectscript.h"
#include "fcgi-2.4.1/include/fcgi_stdio.h"
#include "MPFDParser-1.0/Parser.h"
#include <stdlib.h>

using namespace ObjectScript;

class FCGX_OS: public OS
{
protected:

	FCGX_Request * request;
	int shutdown_funcs_id;
	bool header_sent;

	virtual ~FCGX_OS()
	{
	}

public:

	FCGX_OS()
	{
		request = NULL;
		header_sent = false;
	}

	void initPreScript()
	{
		// setSetting(OS_SETTING_CREATE_DEBUG_EVAL_OPCODES, true);
		OS::initPreScript();
	}

	void initEnv(const char * var_name, char ** envp)
	{
		newObject();
		for(; *envp; envp++){
			const char * value = *envp;
			const char * split = strchr(value, '=');
			OS_ASSERT(split);
			if(split){
				pushStackValue(-1);
				pushString(value, split - value);
				pushString(split + 1);
				setProperty();
			}
		}
		setGlobal(var_name);
	}

	void echo(const OS_CHAR * str)
	{
		if(!header_sent){
			header_sent = true;
			FCGX_PutS("Content-type: text/html\r\n\r\n", request->out);
		}
		FCGX_PutS(str, request->out);
	}

	/* void printf(const OS_CHAR * fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		FCGX_VFPrintF(request->out, fmt, ap);
		va_end(ap);
	} */

	void setSmartProperty(const OS_CHAR * name)
	{
		int offs = getAbsoluteOffs(-2);
		bool index = false;
		const OS_CHAR * cur = name;
		for(; *cur; cur++){
			if(*cur == (index ? OS_TEXT(']') : OS_TEXT('[')) || *cur == OS_TEXT('.')){
				if(cur > name){
					// newObject();
					// pushStackValue(-3);
					pushString(name, cur - name);
					// pushStackValue(-3);
					// setProperty();
					// move(-1, -2);
					// remove(-3);
				}
				if(*cur == OS_TEXT('[')){
					index = true;
				}else if(*cur == OS_TEXT(']')){
					index = false;
				}
				name = cur + 1;
			}
		}
		if(*name){
			// setProperty(name);
			pushString(name);
		}
		int count = getAbsoluteOffs(-2) - offs;
		for(int i = 0; i < count-1; i++){
			newObject();
			pushStackValue();
			setProperty(offs, toString(offs + 2 + i));
			move(-1, offs);
			remove(offs + 1);
		}
		String prop_name = toString();
		remove(offs + 2, count);
		setProperty(prop_name);
	}

	static int triggerHeaderSent(OS * p_os, int params, int, int, void*)
	{
		FCGX_OS * os = (FCGX_OS*)p_os;
		os->header_sent = true;
		return 0;
	}

	static int registerShutdownFunction(OS * p_os, int params, int, int, void*)
	{
		if(params > 0){
			FCGX_OS * os = (FCGX_OS*)p_os;
			int offs = os->getAbsoluteOffs(-params);
			os->pushValueById(os->shutdown_funcs_id);
			for(int i = 0; i < params; i++){
				os->pushStackValue();
				os->pushStackValue(offs+i);
				os->pushStackValue();
				os->setProperty();
			}
		}
		return 0;
	}

	void triggerShutdownFunctions()
	{
		pushValueById(shutdown_funcs_id);
		Core::Value list = core->getStackValue(-1);
		Core::GCValue * obj = list.getGCValue();
		OS_ASSERT(OS_VALUE_TYPE(list) == OS_VALUE_TYPE_OBJECT && obj);
		if(obj->table && obj->table->count){
			Core::Property * prop = obj->table->last;
			for(; prop; prop = prop->prev){
				if(prop->value.isFunction()){
					core->pushValue(prop->value);
					pushNull();
					call();
				}
			}
		}
		pop();
	}

	void registerFunctions()
	{
		FuncDef funcs[] = {
			{"registerShutdownFunction", FCGX_OS::registerShutdownFunction},
			{"triggerHeaderSent", FCGX_OS::triggerHeaderSent},
			{}
		};
		pushGlobals();
		setFuncs(funcs);
		pop();
	}

	void processRequest(FCGX_Request * p_request)
	{
		request = p_request;

		pushStackValue(OS_REGISTER_USERPOOL);
		newObject();
		shutdown_funcs_id = getValueId();
		addProperty();

		registerFunctions();

		initEnv("_SERVER", request->envp);

		newObject();
		setGlobal("_POST");
		
		newObject();
		setGlobal("_GET");
		
		newObject();
		setGlobal("_FILES");
		
		newObject();
		setGlobal("_COOKIE");

#define OS_AUTO_TEXT(exp) OS_TEXT(#exp)
		eval(OS_AUTO_TEXT(
			if('HTTP_COOKIE' in _SERVER)
			for(var k, v in _SERVER.HTTP_COOKIE.split(';')){
				v = v.trim().split('=')
				if(#v == 2){
					_COOKIE[v[0]] = v[1]
				}
			}	
		));
		
		getGlobal("_SERVER");
		getProperty("CONTENT_LENGTH");
		int content_length = popInt();

		int post_max_size = 1024*1024*8;
		if(content_length > post_max_size){
			FCGX_FPrintF(request->out, "POST Content-Length of %d bytes exceeds the limit of %d bytes", content_length, post_max_size);
			return;
		}

		getGlobal("_SERVER");
		getProperty("CONTENT_TYPE");
		String content_type = popString();

		const char * multipart_form_data = "multipart/form-data;";
		int multipart_form_data_len = strlen(multipart_form_data);

		MPFD::Parser POSTParser = MPFD::Parser();
		if(content_length > 0 && content_type.getLen() > 0 && strncmp(content_type.toChar(), multipart_form_data, multipart_form_data_len) == 0){
			POSTParser.SetTempDirForFileUpload("/tmp");
			// POSTParser.SetMaxCollectedDataLength(20*1024);
			POSTParser.SetContentType(content_type.toChar());

			int max_temp_buf_size = (int)(1024*1024*0.1);
			int temp_buf_size = content_length < max_temp_buf_size ? content_length : max_temp_buf_size;
			char * temp_buf = new char[temp_buf_size + 1];
			for(int cur_len; (cur_len = FCGX_GetStr(temp_buf, temp_buf_size, request->in)) > 0;){
				POSTParser.AcceptSomeData(temp_buf, cur_len);
			}
			delete [] temp_buf;
			temp_buf = NULL;
			
			// POSTParser.SetExternalDataBuffer(buf, len);
			POSTParser.FinishData();

			std::map<std::string, MPFD::Field *> fields = POSTParser.GetFieldsMap();
			// FCGX_FPrintF(request->out, "Have %d fields<p>\n", fields.size());

			std::map<std::string, MPFD::Field *>::iterator it;
			for(it = fields.begin(); it != fields.end(); it++){
				MPFD::Field * field = fields[it->first];
				if(field->GetType() == MPFD::Field::TextType){
					getGlobal("_POST");
					pushString(field->GetTextTypeContent().c_str());
					setSmartProperty(it->first.c_str());
				}else{
					getGlobal("_FILES");
					newObject();
					{
						pushStackValue();
						pushString(field->GetFileName().c_str());
						setProperty("name");
						
						pushStackValue();
						pushString(field->GetFileMimeType().c_str());
						setProperty("type");
						
						pushStackValue();
						pushString(field->GetTempFileNameEx().c_str());
						setProperty("tmp_name");
						
						pushStackValue();
						pushNumber(getFileSize(field->GetTempFileNameEx().c_str()));
						setProperty("size");
					}
					setSmartProperty(it->first.c_str());
				}
			}
		}
		
		initEnv("_ENV", environ);
		
		getGlobal("_SERVER");
		getProperty("SCRIPT_FILENAME");
		String script_filename = popString();
		
		String ext = getFilenameExt(script_filename);
		if(ext == OS_EXT_SOURCECODE || ext == OS_EXT_TEMPLATE){
			require(script_filename, true);
		}else{
			// TODO: output not OS file
			if(!header_sent){
				header_sent = true;
				if(ext == ".txt"){
					FCGX_PutS("Content-type: text/plain\r\n", request->out);
				}else{
					FCGX_PutS("Content-type: text/html\r\n", request->out);
				}
				FCGX_PutS("\r\n", request->out);
			}
			void * f = openFile(script_filename, "rb");
			if(f){
				const int BUF_SIZE = 1024*256;
				int size = getFileSize(f);
				void * buf = malloc(BUF_SIZE < size ? BUF_SIZE : size OS_DBG_FILEPOS);
				for(int i = 0; i < size; i += BUF_SIZE){
					int len = BUF_SIZE < size - i ? BUF_SIZE : size - i;
					readFile(buf, len, f);
					FCGX_PutStr((const char*)buf, len, request->out);
				}
				closeFile(f);
				free(buf);				
			}else{
				FCGX_PutS("Error open file: ", request->out);
				FCGX_PutS(script_filename, request->out);
			}
		}

		triggerShutdownFunctions();
		// FCGX_Finish_r(request);
		FCGX_FFlush(request->out);
	}
};

#if 0
static void printEnv(FCGX_Stream *out, char *label, char **envp)
{
    FCGX_FPrintF(out, "%s:<br>\n<pre>\n", label);
    for( ; *envp != NULL; envp++) {
        FCGX_FPrintF(out, "%s\n", *envp);
    }
    FCGX_FPrintF(out, "</pre><p>\n");
}
#endif

void log(const char * msg)
{
	FILE * f = fopen("/tmp/os-fcgi.log", "wt");
	if(f){
		fwrite(msg, strlen(msg), 1, f);
		fclose(f);
	}
}

#ifdef _MSC_VER
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char * argv[])
#endif
{
	const char * port = ":9000";
    int listen_queue_backlog = 400;

	if(FCGX_Init()){
		exit(1); 
	}

    int  listen_socket = FCGX_OpenSocket(port, listen_queue_backlog);
    if(listen_socket < 0){
		printf("listen_socket < 0 \n");
		exit(1);
	}

    FCGX_Request request;
    if(FCGX_InitRequest(&request, listen_socket, 0)){
		printf("error init request \n");
		exit(1);
	}

    while(FCGX_Accept_r(&request) == 0){
#if 1
		FCGX_OS * os = OS::create(new FCGX_OS());
		os->processRequest(&request);
        os->release();
		FCGX_Finish_r(&request);
#elif 0
        FCGX_FPrintF(request.out, "Content-type: text/html\r\n\r\n<TITLE>fastcgi</TITLE>\n<H1>Fastcgi: Hello world.</H1>\n");

        printEnv(request.out, "Request environment", request.envp);
        printEnv(request.out, "Initial environment", environ);

		int maxPostDataLen = 5*1024*1024;
		char *contentLength = FCGX_GetParam("CONTENT_LENGTH", request.envp);
		int len = contentLength ? strtol(contentLength, NULL, 10) : 0;
		if(len > maxPostDataLen){
			FCGX_FPrintF(request.out, "Max post data len: %d > %d\n", len, maxPostDataLen);
			continue;
		}
		
		const char * multipartFormData = "multipart/form-data;";
		int multipartFormDataLen = strlen(multipartFormData);
	
		char * contentType = FCGX_GetParam("CONTENT_TYPE", request.envp);
		if(contentType && len > 0 && strncmp(contentType, multipartFormData, multipartFormDataLen) == 0){
			MPFD::Parser POSTParser = MPFD::Parser();
			POSTParser.SetTempDirForFileUpload("/tmp");
			// POSTParser.SetMaxCollectedDataLength(20*1024);
			POSTParser.SetContentType(contentType);

			char * buf = new char[len];
			len = FCGX_GetStr(buf, len, request.in);

			POSTParser.SetExternalDataBuffer(buf, len);
			POSTParser.FinishData();

			std::map<std::string, MPFD::Field *> fields = POSTParser.GetFieldsMap();
			FCGX_FPrintF(request.out, "Have %d fields<p>\n", fields.size());

			std::map<std::string, MPFD::Field *>::iterator it;
			for(it = fields.begin(); it != fields.end(); it++){
				MPFD::Field * field = fields[it->first];
				if(field->GetType() == MPFD::Field::TextType){
					FCGX_FPrintF(request.out, "Got text field: '%s', value: '%s'<br>\n", it->first.c_str(), field->GetTextTypeContent().c_str());
				}else{
					FCGX_FPrintF(request.out, "Got text field: '%s', filename: '%s', tempfilename: '%s', mime-type: '%s'<br>\n", it->first.c_str(), 
						field->GetFileName().c_str(),
						field->GetTempFileNameEx().c_str(),
						field->GetFileMimeType().c_str()
						// field->GetTextTypeContent().c_str()
						);
				}
			}
		}else{
            FCGX_FPrintF(request.out, "No data from standard input.<p>\n");
		}
        FCGX_Finish_r(&request);
#else
        FCGX_FPrintF(request.out, "Content-type: text/html\r\n\r\n<TITLE>fastcgi</TITLE>\n<H1>Fastcgi: Hello world.</H1>\n");
		char *contentLength = FCGX_GetParam("CONTENT_LENGTH", request.envp);
		int len = contentLength ? strtol(contentLength, NULL, 10) : 0;
        if (len <= 0) {
            FCGX_FPrintF(request.out, "No data from standard input.<p>\n");
        }
        else {
            int i, ch;

            FCGX_FPrintF(request.out, "Standard input:<br>\n<pre>\n");
            for (i = 0; i < len; i++) {
                if ((ch = FCGX_GetChar(request.in)) < 0) {
                    FCGX_FPrintF(request.out,
                        "Error: Not enough bytes received on standard input<p>\n");
                    break;
                }
				switch(ch){
				case '<':
					FCGX_FPrintF(request.out, "&lt;");
					break;

				case '>':
					FCGX_FPrintF(request.out, "&gt;");
					break;

				default:
					FCGX_PutChar(ch, request.out);
				}
            }
            FCGX_FPrintF(request.out, "\n</pre><p>\n");
        }
        FCGX_Finish_r(&request);
#endif
    }

	return 0;
}

