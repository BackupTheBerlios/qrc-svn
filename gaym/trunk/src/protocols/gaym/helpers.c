#include "helpers.h"


char * return_string_between(const char* startbit, const char* endbit, const char* source) {

  char* start=0;
  char* end=0;
  
  start=strstr(source,startbit);
  
  if(start)
{
    start+=strlen(startbit);
    end = strstr(start,endbit);
}
  gaim_debug_misc("gaym","source: %d; start: %d; end: %d\n",source,start,end);
  if(start && end)
    return g_strdup_printf("%.*s",end-start,start);
  else 
    return 0;
  
}

char* convert_nick_to_gc(char* nick) {
  int i;
  char* out=g_strdup(nick);
  for(i=0; i<strlen(out); i++)
    if(out[i]=='.')
      out[i]='|';
  gaim_debug_misc("gaym","Converted %s to %s\n",nick,out);
  return out;
  } 
 
  char* convert_nick_from_gc(char* nick) {
    int i;
    char* out=g_strdup(nick);
    for(i=0; i<strlen(out); i++)
      if(out[i]=='|')
        out[i]='.';
    gaim_debug_misc("gaym","Converted %s to %s\n",nick,out);
    return out;
  }  