#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "vprintf_backend.h"

static inline unsigned int skip_to(const char *format) {
  int unsigned nr;
  for (nr=0; format[nr] && (format[nr]!='%'); ++nr);
  return nr;
}

#define A_WRITE(fn,buf,sz)	((fn)->put((buf),(sz),(fn)->data))

static int visible_strlen(char const *s, unsigned int *wh_sz)
{
  int l = 0;
  int in_esc = 0;
  *wh_sz = 0;
  for ( ; *s; ++s, ++(*wh_sz))
    {
      if (in_esc == 2)
	{
	  if (!(*s >= '0' && *s <= '9') && *s != ';')
	    in_esc = 0;

	  continue;
	}
      else if (in_esc == 1 && *s == '[')
	{
	  in_esc = 2;
	  continue;
	}

      if (*s == '\033')
	{
	  in_esc = 1;
	  continue;
	}
      ++l;
    }

  return l + in_esc;
}

static char* pad_line[16]= { "                ", "0000000000000000", };
static inline int write_pad(struct output_op* fn, int len, int padwith) {
  int nr=0;
  for (;len>15;len-=16,nr+=16) {
    A_WRITE(fn,pad_line[(padwith=='0')?1:0],16);
  }
  if (len>0) {
    A_WRITE(fn,pad_line[(padwith=='0')?1:0],(unsigned int)len); nr+=len;
  }
  return nr;
}

int __v_printf(struct output_op* fn, const char *format, va_list arg_ptr)
{
  int len=0;

  while (*format) {
    unsigned int sz = skip_to(format);
    if (sz) {
      A_WRITE(fn,format,sz); len+=sz;
      format+=sz;
    }
    if (*format=='%') {
      char buf[32];

      char ch, padwith=' ';
      char *s;

      char flag_in_sign=0;
      char flag_upcase=0;
      char flag_hash=0;
      char flag_left=0;
      char flag_space=0;
      char flag_sign=0;
      char flag_dot=0;
      char flag_size_t = 0;
      signed char flag_long=0;

      unsigned int base;
      unsigned int width=0, preci=0;

      long int number=0;
      long long llnumber=0;

      unsigned int wh_sz;

      ++format;
inn_printf:
      switch(ch=*format++) {
      case 0:
	return -1;
	break;

      /* FLAGS */
      case '#':
	flag_hash=1;
	goto inn_printf;

      case 'h':
	--flag_long;
	goto inn_printf;
      case 'l':
	++flag_long;
	goto inn_printf;
      case 'z':
	flag_size_t = 1;
	goto inn_printf;

      case '0':
	padwith='0';
	goto inn_printf;

      case '-':
	flag_left=1;
	goto inn_printf;

      case ' ':
	flag_space=1;
	goto inn_printf;

      case '+':
	flag_sign=1;
	goto inn_printf;

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
	if(flag_dot) return -1;
	width=strtoul(format-1,&s,10);
	format=s;
	goto inn_printf;

      case '*':
	width=va_arg(arg_ptr,int);
	goto inn_printf;

      case '.':
	flag_dot=1;
	if (*format=='*') {
	  preci=va_arg(arg_ptr,int);
	  ++format;
	} else {
	  long int tmp=strtol(format,&s,10);
	  preci=tmp<0?0:tmp;
	  format=s;
	}
	goto inn_printf;

      /* print a char or % */
      case 'c':
	ch=(char)va_arg(arg_ptr,int);
      case '%':
	A_WRITE(fn,&ch,1); ++len;
	break;

      /* print a string */
      case 's':
	s=va_arg(arg_ptr,char *);
	if (!s) s="(null)";
	sz = visible_strlen(s, &wh_sz);
	// FIXME: could not limit output when invisibible charachters are
	// in the output
	if (sz == wh_sz && flag_dot && sz>preci)
	  {
	    sz = preci;
	    wh_sz = sz;
	  }

print_out:
	if (width && (!flag_left)) {
	  len+=write_pad(fn,(signed int)width-(signed int)sz,padwith);
	}
	A_WRITE(fn,s,wh_sz); len+=wh_sz;
	if (width && (flag_left)) {
	  len+=write_pad(fn,(signed int)width-(signed int)sz,' ');
	}
	break;

      /* print an integer value */
      case 'b':
	base=2;
	sz=0;
	goto num_printf;
      case 'p':
	flag_hash=1;
	if(sizeof(void*)>sizeof(unsigned)) 
	  ++flag_long;
	if(sizeof(void*)>sizeof(long))
	  ++flag_long;
	ch='x';
      case 'X':
	flag_upcase=(ch=='X');
      case 'x':
	base=16;
	sz=0;
	if (flag_hash) {
	  buf[1]='0';
	  buf[2]=ch;
	  sz=2;
	}
	goto num_printf;
      case 'd':
      case 'i':
	flag_in_sign=1;
      case 'u':
	base=10;
	sz=0;
	goto num_printf;
      case 'o':
	base=8;
	sz=0;
	if (flag_hash) {
	  buf[1]='0';
	  ++sz;
	}

num_printf:
	if (flag_long>0) {
	  if (flag_long>1)
	    llnumber=va_arg(arg_ptr,long long);
	  else
	    number=va_arg(arg_ptr,long);
	}
	else if (flag_size_t)
          number=va_arg(arg_ptr,size_t);
        else
	  number=va_arg(arg_ptr,int);

	if (flag_in_sign) {
	  if ((flag_long>1)&&(llnumber<0)) {
	    llnumber=-llnumber;
	    flag_in_sign=2;
	  } else
	    if (number<0) {
	      number=-number;
	      flag_in_sign=2;
	    }
	}
	if (flag_long<1) number&=0xffffffff;
	if (flag_long<0) number&=0xffff;
	if (flag_long<-1) number&=0xff;
	if (flag_long>1)
	  sz += __lltostr(buf+1+sz,sizeof(buf)-5,
			  (unsigned long long) llnumber,base,flag_upcase);
	else
	  sz += __ltostr(buf+1+sz,sizeof(buf)-5,
			 (unsigned long) number,base,flag_upcase);

	s=buf+1;

	if (flag_in_sign==2) {
	  *(--s)='-';
	  ++sz;
	} else if ((flag_in_sign)&&(flag_sign || flag_space)) {
	  *(--s)=(flag_sign)?'+':' ';
	  ++sz;
	}
	wh_sz = sz;

	goto print_out;

      default:
	break;
      }
    }
  }
  return len;
}

