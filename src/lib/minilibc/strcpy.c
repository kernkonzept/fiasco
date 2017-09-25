
char* strcpy(register char* s,register const char* t) {

  char *dest=s;

  while ((*dest++=*t++))
    ;
    
  return s;
}
